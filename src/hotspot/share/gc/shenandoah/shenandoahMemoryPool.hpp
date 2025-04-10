/*
 * Copyright (c) 2013, 2019, Red Hat, Inc. All rights reserved.
 * Copyright Amazon.com Inc. or its affiliates. All Rights Reserved.
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

#ifndef SHARE_GC_SHENANDOAH_SHENANDOAHMEMORYPOOL_HPP
#define SHARE_GC_SHENANDOAH_SHENANDOAHMEMORYPOOL_HPP

#include "gc/shenandoah/shenandoahHeap.hpp"
#include "services/memoryPool.hpp"
#include "services/memoryUsage.hpp"

class ShenandoahMemoryPool : public CollectedMemoryPool {
protected:
   ShenandoahHeap* _heap;

public:
  explicit ShenandoahMemoryPool(ShenandoahHeap* heap,
                       const char* name = "Shenandoah");
  MemoryUsage get_memory_usage() override;
  size_t used_in_bytes() override;
  size_t max_size() const override;

protected:
  ShenandoahMemoryPool(ShenandoahHeap* heap,
                       const char* name,
                       size_t initial_capacity,
                       size_t max_capacity);
};

class ShenandoahGenerationalMemoryPool: public ShenandoahMemoryPool {
private:
  ShenandoahGeneration* _generation;
public:
  explicit ShenandoahGenerationalMemoryPool(ShenandoahHeap* heap, const char* name, ShenandoahGeneration* generation);
  MemoryUsage get_memory_usage() override;
  size_t used_in_bytes() override;
};

class ShenandoahYoungGenMemoryPool : public ShenandoahGenerationalMemoryPool {
public:
  explicit ShenandoahYoungGenMemoryPool(ShenandoahHeap* heap);
};

class ShenandoahOldGenMemoryPool : public ShenandoahGenerationalMemoryPool {
public:
  explicit ShenandoahOldGenMemoryPool(ShenandoahHeap* heap);
};

#endif // SHARE_GC_SHENANDOAH_SHENANDOAHMEMORYPOOL_HPP
