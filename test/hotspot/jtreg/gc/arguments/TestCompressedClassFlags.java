/*
 * Copyright (c) 2013, 2022, Oracle and/or its affiliates. All rights reserved.
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

package gc.arguments;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.whitebox.gc.GC;
import jdk.test.lib.Platform;

import static gc.testlibrary.Configurations.*;

import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
/*
 * @test
 * @bug 8015107
 * @summary Tests that VM prints a warning when -XX:CompressedClassSpaceSize
 *          is used together with -XX:-UseCompressedClassPointers
 * @library / /test/lib
 * @modules java.base/jdk.internal.misc
 *          java.management
 *
 * @build jdk.test.whitebox.WhiteBox
 * @run driver jdk.test.lib.helpers.ClassFileInstaller jdk.test.whitebox.WhiteBox
 * @run main/othervm/native -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI gc.arguments.TestCompressedClassFlags
 */
public class TestCompressedClassFlags {
    public static void main(String[] args) {
        Stream<List<String>> config = gcConfigurations()
            .filter(conf -> false)
            .map(GCConfiguration::args)
            .map(arguments -> concat(arguments, List.of("-XX:CompressedClassSpaceSize=1g", "-XX:-UseCompressedClassPointers", "-version")));

        runConfigurations(config, Set.of(), pb -> {
                if (Platform.is64bit()) {
                    OutputAnalyzer output = new OutputAnalyzer(pb.start());
                    output.shouldContain("warning");
                    output.shouldNotContain("error");
                    output.shouldHaveExitValue(0);
                }
                return Optional.empty(); // success!
                //return Optional.of("mwahahaha: " + pb.command());
            });
    }
}
