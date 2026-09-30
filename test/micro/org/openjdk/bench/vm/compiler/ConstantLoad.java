/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
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
 */
package org.openjdk.bench.vm.compiler;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.CompilerControl;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;
import org.openjdk.jmh.infra.Blackhole;

import java.util.concurrent.TimeUnit;

/**
 * Isolates materialization of int, class and oop (String) constants in JIT
 * generated code.
 *
 * Each helper is marked DONT_INLINE and simply returns a compile-time
 * constant. As a result the compiled body reduces to "materialize the
 * constant, return it", so the generated code exercises exactly the
 * constant-loading path (e.g. immediate loads / constant-pool loads on the
 * target ISA) without surrounding arithmetic, allocation or call overhead.
 *
 * The caller consumes each returned value with a Blackhole so the constant
 * cannot be folded into the loop and the helper cannot be optimized away.
 *
 * Several magnitudes of int constant are provided because ISAs materialize
 * small vs. large immediates differently (e.g. on ppc64le a value may fit a
 * single addi/li, need addis+ori, or be loaded from the constant pool).
 */
@Fork(value = 3)
@Warmup(iterations = 5, time = 3)
@Measurement(iterations = 5, time = 3)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@BenchmarkMode(Mode.AverageTime)
@State(Scope.Thread)
public class ConstantLoad {

    // ---- int constants of increasing magnitude -------------------------

    // @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    private static int int32_0() {
        return 0x12345678;         // needs a two-instruction / hi+lo load
    }

    private static int int32_1() {
        return 0x13345678;         // needs a two-instruction / hi+lo load
    }

    private static int int32_3() {
        return 0x12445678;         // needs a two-instruction / hi+lo load
    }

    private static int int32_3() {
        return 0x12355678;         // needs a two-instruction / hi+lo load
    }

    // @Benchmark
    // public int loadInt32() {
    //     return int32();
    // }

    // Variants that consume via a Blackhole in a tight loop, to observe the
    // constant load in isolation without the return crossing an inlining
    // boundary back into the JMH harness.

    @Benchmark
    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    public void loadInt32Loop(Blackhole bh) {
        for (int i = 0; i < 100; i++) {
            bh.consume(int32_0());
            bh.consume(int32_1());
            bh.consume(int32_2());
            bh.consume(int32_3());
        }
    }
}
