/*
 * Copyright (c) 2000, 2024, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012, 2023 SAP SE. All rights reserved.
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
#include "logging/log.hpp"
#include "logging/logStream.hpp"
#include "logging/logTag.hpp"
#include "runtime/globals.hpp"
#include "runtime/os.hpp"
#include "runtime/signature.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/nativeStackPrinter.hpp"
#include "utilities/powerOfTwo.hpp"

void ddd_print_native_stack(uint64_t data1, uint64_t data2) {
  LogTarget(Trace, newcode) _lt;
  if (_lt.develop_is_enabled()) {
    LogStream _ls(_lt);
    _ls.print_cr("data1:" UINT64_FORMAT " data2:" UINT64_FORMAT, data1, data2);
    NativeStackPrinter nsp(JavaThread::current());
    char buf[O_BUFLEN];
    address lastpc = nullptr;
    nsp.print_stack(&_ls, buf, sizeof(buf), lastpc,
        true /* print_source_info */, -1 /* max stack */);
  }
}
