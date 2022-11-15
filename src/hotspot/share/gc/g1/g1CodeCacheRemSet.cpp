/*
 * Copyright (c) 2014, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "code/codeCache.hpp"
#include "code/nmethod.hpp"
#include "gc/g1/g1CodeCacheRemSet.hpp"
#include "gc/g1/heapRegion.hpp"
#include "memory/heap.hpp"
#include "memory/iterator.hpp"
#include "oops/access.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/atomic.hpp"
#include "services/memTracker.hpp"
#include "utilities/concurrentHashTable.hpp"
#include "utilities/stack.inline.hpp"

auto void_f = [](nmethod** val){};

size_t G1CodeRootSet::static_mem_size() {
  return 0; // we have no static members any more.
}

void G1CodeRootSet::add(nmethod* method) {
  if (_cc.counter() > Threshold) {
    _table.grow(Thread::current(), log2i(LargeSize));
  }
  NmethodTableLookup lookup(method);
  _table.insert(Thread::current(), lookup, method);
}

bool G1CodeRootSet::remove(nmethod* method) {
  NmethodTableLookup lookup(method);
  return _table.remove(Thread::current(), lookup);
}

bool G1CodeRootSet::contains(nmethod* method) {
  NmethodTableLookup lookup(method);
  return _table.get(Thread::current(), lookup, void_f, nullptr);
}

void G1CodeRootSet::clear() {
  auto eval_function = [](nmethod** val){return true;};
  _table.bulk_delete(Thread::current(), eval_function, void_f);
  _table.shrink(Thread::current(), log2i(SmallSize));
}

void G1CodeRootSet::clear_unlocked() {
  this->~G1CodeRootSet();
  new (this) G1CodeRootSet();
}

size_t G1CodeRootSet::mem_size() {
  return sizeof(*this) + _table.get_mem_size(Thread::current()) - sizeof(_table) /* _table counted in both *this and _table.get_mem_size() */
    + _cc.counter() * _table.get_node_size();
}

void G1CodeRootSet::nmethods_do(CodeBlobClosure* blk) const {
  auto f = [&](nmethod** nm) {
    blk->do_code_blob(*nm);
    return true; // continue
  };
  _table.do_safepoint_scan(f);
}

class CleanCallback : public StackObj {
  class PointsIntoHRDetectionClosure : public OopClosure {
    HeapRegion* _hr;
   public:
    bool _points_into;
    PointsIntoHRDetectionClosure(HeapRegion* hr) : _hr(hr), _points_into(false) {}

    void do_oop(narrowOop* o) {
      do_oop_work(o);
    }

    void do_oop(oop* o) {
      do_oop_work(o);
    }

    template <typename T>
    void do_oop_work(T* p) {
      if (_hr->is_in(RawAccess<>::oop_load(p))) {
        _points_into = true;
      }
    }
  };

  PointsIntoHRDetectionClosure _detector;
  CodeBlobToOopClosure _blobs;

 public:
  CleanCallback(HeapRegion* hr) : _detector(hr), _blobs(&_detector, !CodeBlobToOopClosure::FixRelocations) {}

  bool operator() (nmethod** nm) {
    _detector._points_into = false;
    _blobs.do_code_blob(*nm);
    return !_detector._points_into;
  }
};

void G1CodeRootSet::clean(HeapRegion* owner) {
  CleanCallback should_clean(owner);
  _table.bulk_delete(Thread::current(), should_clean, void_f);
}
