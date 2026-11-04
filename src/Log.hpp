#pragma once
// Log files: everything the game prints (stdout + stderr, including the [SLOW] / [SPIKE] / [perf] lines) is also written to
//   <data folder>/logs/hypervox-YYYY-MM-DD_HH-MM-SS.log     (one file per run, the 10 newest are kept)
//   <data folder>/logs/latest.log                           (a copy of the current run)
// A crash (segfault, abort, uncaught exception ...) writes "[hypervox][CRASH]" plus a backtrace into the same file first.
void logInit();
