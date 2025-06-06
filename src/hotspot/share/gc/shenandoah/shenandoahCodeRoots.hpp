/*
 * Copyright (c) 2017, 2022, Red Hat, Inc. All rights reserved.
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

#ifndef SHARE_GC_SHENANDOAH_SHENANDOAHCODEROOTS_HPP
#define SHARE_GC_SHENANDOAH_SHENANDOAHCODEROOTS_HPP

#include "code/codeCache.hpp"
#include "gc/shenandoah/shenandoahLock.hpp"
#include "gc/shenandoah/shenandoahPadding.hpp"
#include "gc/shenandoah/shenandoahSharedVariables.hpp"
#include "memory/allStatic.hpp"
#include "memory/iterator.hpp"
#include "utilities/globalDefinitions.hpp"

class ShenandoahHeap;
class ShenandoahHeapRegion;
class ShenandoahNMethodTable;
class ShenandoahNMethodTableSnapshot;
class WorkerThreads;

class ShenandoahCodeRootsIterator {
  friend class ShenandoahCodeRoots;
protected:
  ShenandoahNMethodTableSnapshot* _table_snapshot;

public:
  ShenandoahCodeRootsIterator();
  ~ShenandoahCodeRootsIterator();

  void possibly_parallel_nmethods_do(NMethodClosure *f);
};

class ShenandoahCodeRoots : public AllStatic {
  friend class ShenandoahHeap;
  friend class ShenandoahCodeRootsIterator;

public:
  static void initialize();
  static void register_nmethod(nmethod* nm);
  static void unregister_nmethod(nmethod* nm);

  static ShenandoahNMethodTable* table() {
    return _nmethod_table;
  }

  // Concurrent nmethod unloading support
  static void unlink(WorkerThreads* workers, bool unloading_occurred);
  static void purge();
  static void arm_nmethods_for_mark();
  static void arm_nmethods_for_evac();
  static void disarm_nmethods();
  static int  disarmed_value()         { return _disarmed_value; }
  static int* disarmed_value_address() { return &_disarmed_value; }

  static bool use_nmethod_barriers_for_mark();

private:
  static ShenandoahNMethodTable* _nmethod_table;
  static int                     _disarmed_value;
};

#endif // SHARE_GC_SHENANDOAH_SHENANDOAHCODEROOTS_HPP
