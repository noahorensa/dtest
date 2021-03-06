/*
 * Copyright (c) 2021 Noah Orensa.
 * Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <dtest.h>

module("performance-test")
.dependsOn({
    "unit-test"
});

perf("performance-test", "pass")
.body([] {
    for (int i = 0; i < 1000000; ++i);
})
.baseline([] {
    for (int i = 0; i < 8000000; ++i);
});

perf("performance-test", "too-slow")
.expect(Status::TOO_SLOW)
.body([] {
    for (int i = 0; i < 8000000; ++i);
})
.baseline([] {
    for (int i = 0; i < 1000000; ++i);
});

perf("performance-test", "pass-ratio")
.performanceMarginAsBaselineRatio(0.7)
.body([] {
    for (int i = 0; i < 20000000; ++i);
})
.baseline([] {
    for (int i = 0; i < 80000000; ++i);
});

perf("performance-test", "too-slow-ratio")
.performanceMarginAsBaselineRatio(0.7)
.expect(Status::TOO_SLOW)
.body([] {
    for (int i = 0; i < 80000000; ++i);
})
.baseline([] {
    for (int i = 0; i < 20000000; ++i);
});

perf("performance-test", "error-msg")
.expect(Status::TOO_SLOW)
.body([] {
    for (int i = 0; i < 8000000; ++i);
    err("error from body");
})
.baseline([] {
    err("error from baseline");
});
