/*
 * Copyright (c) 1997, 2023, Oracle and/or its affiliates. All rights reserved.
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
#include "code/nativeInst.hpp"
#include "runtime/atomic.hpp"

int64_t PCN_lookup_success = 0;
int64_t PCN_lookup_failure = 0;
int64_t PCN_decode_success = 0;
int64_t PCN_decode_failure = 0;
int64_t PCN_patch_success = 0;
int64_t PCN_patch_cb_offset_failure = 0;
int64_t PCN_patch_oopmap_slot_failure = 0;

NativePostCallNop* nativePostCallNop_at_stats(address address) {
  NativePostCallNop* nop = nativePostCallNop_at(address);
  if (UseNewCode) {
    int32_t oopmap_slot;
    int32_t cb_offset;
    if (nop != nullptr) {
      bool decode_succ = nop->decode(oopmap_slot, cb_offset);
      Atomic::inc(&PCN_lookup_success, memory_order_relaxed);
      Atomic::inc(decode_succ ? &PCN_decode_success : &PCN_decode_failure, memory_order_relaxed);
    } else {
      Atomic::inc(&PCN_lookup_failure, memory_order_relaxed);
    }
  }
  return nop;
}

void PCN_print_stats() {
  if (!UseNewCode) return;
  tty->print_cr("PCN lookup success: " INT64_FORMAT, PCN_lookup_success);
  tty->print_cr("PCN lookup failure: " INT64_FORMAT, PCN_lookup_failure);
  tty->print_cr("PCN decode success: " INT64_FORMAT, PCN_decode_success);
  tty->print_cr("PCN decode failure: " INT64_FORMAT, PCN_decode_failure);
  tty->print_cr("PCN patch success: " INT64_FORMAT, PCN_patch_success);
  tty->print_cr("PCN patch cb_offset failure: " INT64_FORMAT, PCN_patch_cb_offset_failure);
  tty->print_cr("PCN patch oopmap_slot failure: " INT64_FORMAT, PCN_patch_oopmap_slot_failure);
}
