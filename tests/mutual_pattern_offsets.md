# MutualPattern offset regressions

Run without FFmpeg or libui from the repository root:

```sh
c++ -std=c++11 -Isrc -Wall -Wextra -pedantic \
    src/mutual_pattern.cpp tests/mutual_pattern_offsets.cpp \
    -o /tmp/untrunc-pattern-offset-tests
/tmp/untrunc-pattern-offset-tests
```

The test links the actual production pattern implementation. Unrelated
common.cpp helpers are replaced with throwing stubs so no FFmpeg-dependent
code is needed; these helpers must not be called by the tested methods.

The fixtures use the 32-byte pattern and offset-16 four-zero query used by
Track::hasZeroTransitions. Tests cover variable preceding bytes, non-mutual
target bytes, wrong target values, offset-zero behavior, literal wildcard
bytes, later intersections, and every valid four-byte offset with one
changed reference byte. This is a focused helper regression suite, not a
complete video-repair integration test.
