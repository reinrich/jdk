/*
 * Copyright (c) 2018, 2025, Oracle and/or its affiliates. All rights reserved.
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

package jdk.jfr.api.recording.settings;


import java.io.File;
import java.io.FileReader;
import java.io.Reader;
import java.util.Map;

import jdk.jfr.Configuration;
import jdk.test.lib.Asserts;
import jdk.test.lib.jfr.EventNames;

/**
 * @test
 * @summary Test setName().
 * @requires vm.flagless
 * @requires vm.hasJFR
 * @library /test/lib
 * @run main/othervm jdk.jfr.api.recording.settings.TestCreateConfigFromReader
 */
public class TestCreateConfigFromReader {

    private static final String DIR = System.getProperty("test.src", ".");

    public static void main(String[] args) throws Throwable {
        testOkConfig();
        testNullReader();
    }

    private static void testOkConfig() throws Exception {
        File settingsFile = new File(DIR, "settings.jfc");
        if(!settingsFile.exists()) throw new RuntimeException("File " + settingsFile.getAbsolutePath() +  " not found ");

            FileReader reader = new FileReader(settingsFile);

            Configuration config = Configuration.create(reader);
            Map<String, String> settings = config.getSettings();

            Asserts.assertEquals(4, settings.size(), "Settings size differes from the expected size");
            String[] keys = {"enabled", "stackTrace", "threshold", "custom"};
            String[] vals = {"true", "true", "1 ms", "5"};
            for(int i=0; i<keys.length; ++i) {
                String fullKey = EventNames.JavaMonitorWait + "#" + keys[i];
                Asserts.assertEquals(vals[i], settings.get(fullKey), "Settings value differs from the expected: " + keys[i]);
            }
            Asserts.assertEquals(null, settings.get("doesNotExists"), "Error getting on-existent setting");

            Asserts.assertEquals("Oracle", config.getProvider(), "Configuration provider differs from the expected");
            Asserts.assertEquals("TestSettings", config.getLabel(), "Configuration label differs from the expected");
            Asserts.assertEquals("SampleConfiguration", config.getDescription(), "Configuration description differs from the expected");
            Asserts.assertNull(config.getName(), "Name should be null if created from reader");
    }

    private static void testNullReader() throws Exception {
       try {
           Configuration.create((Reader)null);
           Asserts.fail("Exception was not thrown");
       } catch(NullPointerException x) {
       }
    }

}
