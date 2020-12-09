/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_G1_G1FULLGCREFERENCECLOSURECONTEXT_HPP
#define SHARE_GC_G1_G1FULLGCREFERENCECLOSURECONTEXT_HPP

#include "gc/g1/g1FullCollector.hpp"
#include "gc/g1/g1FullGCOopClosures.hpp"
#include "gc/shared/referenceProcessor.hpp"

class G1FullRefProcClosureContext : public AbstractClosureContext {
  unsigned                _max_workers;
  G1FullCollector&        _collector;
  G1IsAliveClosure        _is_alive;
  G1FullKeepAliveClosure* _keep_alive;
  ThreadModel             _tm;

public:
  G1FullRefProcClosureContext(G1FullCollector& collector, unsigned max_workers)
    : _max_workers(max_workers), _collector(collector), _is_alive(&collector), _keep_alive(NEW_C_HEAP_ARRAY(G1FullKeepAliveClosure, max_workers, mtGC)), _tm(ThreadModel::Single) {}

  ~G1FullRefProcClosureContext() {
    FREE_C_HEAP_ARRAY(G1FullKeepAliveClosure, _keep_alive);
  }
  BoolObjectClosure* is_alive(uint worker_id) {
    assert(worker_id < _max_workers, "sanity");
    return &_is_alive;
  }
  OopClosure* keep_alive(uint worker_id) {
    assert(worker_id < _max_workers, "sanity");
    return ::new (&_keep_alive[index(worker_id, _tm)]) G1FullKeepAliveClosure(_collector.marker(index(worker_id, _tm)));
  };
  VoidClosure* complete_gc(uint worker_id) {
    assert(worker_id < _max_workers, "sanity");
    return _collector.marker(index(worker_id, _tm))->stack_closure();
  }
  void begin(uint queue_count, ThreadModel tm, bool marks_oops_alive) {
    log_debug(gc, ref)("G1FullRefProcClosureContext: begin");
    assert(queue_count <= _max_workers, "sanity");
    _tm = tm;
  };
};

#endif // SHARE_GC_G1_G1FULLGCREFERENCECLOSURECONTEXT_HPP
