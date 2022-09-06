/*
 * Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_G1_G1CODECACHEREMSET_HPP
#define SHARE_GC_G1_G1CODECACHEREMSET_HPP

#include "utilities/concurrentHashTable.inline.hpp"
#include "utilities/concurrentHashTableTasks.inline.hpp"

class CodeBlobClosure;
class HeapRegion;
class nmethod;

class CounterContext {
private:
  volatile size_t _counter;
public:
  CounterContext() {
    Atomic::store(&_counter, size_t(0));
  };
  void increment() {
    Atomic::add(&_counter, size_t(1));
  }
  void decrement() {
    Atomic::sub(&_counter, size_t(1));
  }
  size_t counter() const {
    return Atomic::load(&_counter);
  }
};

static uintx hash(const nmethod* n) {
  return uintx(n) ^ (uintx(n) >> 7u);   // code heap blocks are 128 bytes aligned
}

class NmethodTableConfig : public StackObj {
public:
  typedef nmethod* Value;

  static uintx get_hash(Value const& value, bool* is_dead) {
    *is_dead = false;
    return hash(value);
  }
  static void* allocate_node(void* context, size_t size, Value const& value) {
    static_cast<CounterContext*>(context)->increment();
    return AllocateHeap(size, mtGC);
  }
  static void free_node(void* context, void* memory, Value const& value) {
    static_cast<CounterContext*>(context)->decrement();
    FreeHeap(memory);
  }
};

class NmethodTableLookup {
private:
  const nmethod* _value;

public:
  NmethodTableLookup(const nmethod* value)
    : _value(value) {
  }
  uintx get_hash() const {
    return hash(_value);
  }
  bool equals(nmethod** value, bool* is_dead) {
    *is_dead = false;
    return _value == *value;
  }
};

// Implements storage for a set of code roots.
// All methods that modify the set are not thread-safe except if otherwise noted.
class G1CodeRootSet {
  friend class G1CodeRootSetTest;
 private:
  CounterContext _cc;
  ConcurrentHashTable<NmethodTableConfig, mtGC> _table;
  const static size_t SmallSize = 32;
  const static size_t Threshold = 24;
  const static size_t LargeSize = 512;

  void move_to_large();
  void allocate_small_table();

 public:
  G1CodeRootSet() : _cc{}, _table(log2i(SmallSize), log2i(LargeSize), 0 /* use cc counters to determine hash growth instead */, false /* statistics */, &_cc, 6/*services*/-3) {} // Uncommit_lock - 1

  static size_t static_mem_size();

  void add(nmethod* method);

  bool remove(nmethod* method);

  // Safe to call without synchronization, but may return false negatives.
  bool contains(nmethod* method);

  void clear();

  void nmethods_do(CodeBlobClosure* blk) const;

  // Remove all nmethods which no longer contain pointers into our "owner" region
  void clean(HeapRegion* owner);

  bool is_empty() const { return _cc.counter() == 0; }

  // Length in elements
  size_t length() const { return _cc.counter(); }

  // Memory size in bytes taken by this set.
  size_t mem_size();

};

#endif // SHARE_GC_G1_G1CODECACHEREMSET_HPP
