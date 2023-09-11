/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
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

package gc.testlibrary;

import jdk.test.lib.process.ProcessTools;
import jdk.test.whitebox.gc.GC;
import jtreg.SkippedException;
import jdk.test.lib.Utils;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;



public class Configurations {
    public record GCConfiguration(GC gc, List<String> args) {}

    public static <T> List<T> concat(List<T> a, List<T> b) {
        return Stream.concat(a.stream(), b.stream()).toList();
    }

    public static Stream<GCConfiguration> supportedGcConfigurations() {
        return List.of(new GCConfiguration(GC.Serial, List.of("-XX:+UseSerialGC")),
                       new GCConfiguration(GC.Parallel, List.of("-XX:+UseParallelGC")),
                       new GCConfiguration(GC.G1, List.of("-XX:+UseG1GC")),
                       new GCConfiguration(GC.Z, List.of("-XX:+UseZGC")),
                       new GCConfiguration(GC.Z, List.of("-XX:+UseZGC", "-XX:+ZGenerational")),
                       new GCConfiguration(GC.Shenandoah, List.of("-XX:+UseShenandoahGC")),
                       new GCConfiguration(GC.Epsilon, List.of("-XX:+UnlockExperimentalVMOptions", "-XX:+UseEpsilonGC")))
            .stream()
            .filter(p -> p.gc().isSupported());
    }

    public static boolean conflictsWithFlags(String s) {
        return Arrays.stream(Utils.getTestJavaOpts()).anyMatch(s::equals) ;
    }

    public static Optional<GC> getTestGC() {
        return Stream.of(Utils.getTestJavaOpts())
            .flatMap(s -> GC.fromVmFlag(s).stream())
            .findFirst();
    }

    public static Stream<GCConfiguration> gcConfigurations() {
        return getTestGC()
            .map(testGC -> supportedGcConfigurations().filter(c -> c.gc() == testGC)) // test only the gc explicitly chosen (and only if it is supported)
            .orElseGet(() -> supportedGcConfigurations());                            // test all gc configurations
    }

    public interface ThrowingFunction<T,R> {
        public R apply(T t) throws Exception;
    }

    public static String str(Throwable t) {
        StringWriter writer = new StringWriter();
        t.printStackTrace(new PrintWriter(writer));
        return writer.toString();
    }

    static boolean exclude(List<String> options, Set<String> forbiddenOptions) {
        return options.stream().anyMatch(forbiddenOptions::contains);
    }

    static List<String> cleanClashingOptions(List<String> cmd) {
        Set<String> testJavaOpts = new HashSet<>(Arrays.asList(Utils.getTestJavaOpts()));
        return cmd.stream().filter(option -> !testJavaOpts.contains(option)).toList();
    }

    public static void runConfigurations(Stream<List<String>> cmds, Set<String> excludeOptions, ThrowingFunction<ProcessBuilder, Optional<String>> test) {
        List<List<String>> processed = cmds
            .filter(cmd -> !exclude(cmd, excludeOptions)) // exclude command line configurations that the test writer do not want
            .map(Configurations::cleanClashingOptions)    // remove command line options if they clash with java/vm.options
            .toList();

        if (processed.size() == 0) {
            throw new SkippedException("No configurations left after exclusion filter: " + excludeOptions.stream().sorted().collect(Collectors.joining(", ")));
        }

        List<String> errors = processed.stream()
            .map(cmd -> {
                    try {
                        ProcessBuilder pb = ProcessTools.createTestJvm(cmd);
                        return test.apply(pb);
                    } catch (Exception e) {
                        return Optional.of("Test with configuration: " + cmd + " failed after throwing exception: " + str(e));
                    }
                })
            .flatMap(Optional::stream)
            .toList();

        if (!errors.isEmpty()) {
            throw new RuntimeException(errors.toString());
        }
    }
}
