/*
 * Copyright (c) 2013, 2021, Oracle and/or its affiliates. All rights reserved.
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

import java.awt.Frame;
import java.awt.Point;
import java.awt.Robot;
import java.awt.Rectangle;
import javax.swing.SwingUtilities;

/*
 * @test
 * @key headful
 * @bug 8012026 8196435
 * @summary Component.getMousePosition() does not work in some cases on MacOS
 * @run main GetMousePositionWithOverlay
 */

public class GetMousePositionWithOverlay {

    private static Frame backFrame;
    private static Frame frontFrame;
    private static Robot robot;

    public static void main(String[] args) throws Throwable {
        robot = new Robot();
        robot.setAutoDelay(100);

        try {
            SwingUtilities.invokeAndWait(GetMousePositionWithOverlay::constructTestUI);
            doTest();
        } finally {
            dispose();
        }

        robot.waitForIdle();

    }

    private static void doTest() throws Exception {
        SwingUtilities.invokeAndWait(() -> frontFrame.toFront());
        robot.waitForIdle();

        Rectangle bounds = new Rectangle(frontFrame.getLocationOnScreen(), frontFrame.getSize());
        robot.mouseMove(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
        robot.waitForIdle();

        Point pos = backFrame.getMousePosition();
        if (pos != null) {
            throw new RuntimeException("Test failed. Mouse position should be null but was " + pos);
        }

        pos = frontFrame.getMousePosition();
        if (pos == null) {
            throw new RuntimeException("Test failed. Mouse position should not be null");
        }

        robot.mouseMove(bounds.x + bounds.width + 5, bounds.y + bounds.height + 5);
        robot.waitForIdle();

        pos = backFrame.getMousePosition();
        if (pos == null) {
            throw new RuntimeException("Test failed. Mouse position should not be null");
        }
    }

    private static void dispose() throws Exception {
        SwingUtilities.invokeAndWait(() -> {
            if (backFrame != null) {
                backFrame.dispose();
            }

            if (frontFrame != null) {
                frontFrame.dispose();
            }
        });
    }

    private static void constructTestUI() {
        backFrame = new Frame();
        backFrame.setUndecorated(true);
        backFrame.setBounds(100, 100, 100, 100);
        backFrame.setResizable(false);
        backFrame.setVisible(true);

        frontFrame = new Frame();
        frontFrame.setUndecorated(true);
        frontFrame.setBounds(120, 120, 60, 60);
        frontFrame.setResizable(false);
        frontFrame.setVisible(true);

    }
}

