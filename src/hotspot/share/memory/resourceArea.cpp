/*
 * Copyright (c) 1997, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "logging/logStream.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/resourceArea.inline.hpp"
#include "nmt/memTracker.hpp"
#include "runtime/atomic.hpp"
#include "runtime/javaThread.hpp"
#include "utilities/vmError.hpp"

#ifdef ASSERT

ResourceMark::ResourceMark(ResourceArea* area, Thread* thread) :
    _impl(area),
    _thread(thread),
    _previous_resource_mark(nullptr)
{
  if (_thread != nullptr) {
    assert(_thread == Thread::current(), "not the current thread");
    _previous_resource_mark = _thread->current_resource_mark();
    _thread->set_current_resource_mark(this);
  }
}

void ResourceArea::verify_has_resource_mark() {
  if (_nesting <= 0 && !VMError::is_error_reported()) {
    // Only report the first occurrence of an allocating thread that
    // is missing a ResourceMark, to avoid possible recursive errors
    // in error handling.
    static volatile bool reported = false;
    if (!Atomic::load(&reported)) {
      if (!Atomic::cmpxchg(&reported, false, true)) {
        fatal("memory leak: allocating without ResourceMark");
      }
    }
  }
}
ResourceMarkLogger::~ResourceMarkLogger() {
  LogTarget(Debug, newcode) _lt;
  if (_lt.develop_is_enabled()) {
    LogStream _ls(_lt);
    ResourceArea::SavedState ss = _rm._impl._saved_state;
    ResourceArea* ra = _rm._impl._area;
    size_t free_on_entry_b = pointer_delta(ss._max, ss._hwm, 1);
    size_t used_on_entry_b = ss._size_in_bytes - free_on_entry_b;
    size_t free_now_b = pointer_delta(ra->_max, ra->_hwm, 1);
    size_t alloc_b = (ra->size_in_bytes() - free_now_b - used_on_entry_b);
    _ls.print_cr("%s: used_kb:" SIZE_FORMAT " alloc_kb:" SIZE_FORMAT" alloc_b:" SIZE_FORMAT, _id, used_on_entry_b/K, alloc_b/K, alloc_b);
  }
}

#endif // ASSERT

//------------------------------ResourceMark-----------------------------------
// The following routines are declared in allocation.hpp and used everywhere:

// Allocation in thread-local resource area
extern char* resource_allocate_bytes(size_t size, AllocFailType alloc_failmode) {
  return Thread::current()->resource_area()->allocate_bytes(size, alloc_failmode);
}
extern char* resource_allocate_bytes(Thread* thread, size_t size, AllocFailType alloc_failmode) {
  return thread->resource_area()->allocate_bytes(size, alloc_failmode);
}

extern char* resource_reallocate_bytes( char *old, size_t old_size, size_t new_size, AllocFailType alloc_failmode){
  return (char*)Thread::current()->resource_area()->Arealloc(old, old_size, new_size, alloc_failmode);
}

extern void resource_free_bytes( Thread* thread, char *old, size_t size ) {
  thread->resource_area()->Afree(old, size);
}
