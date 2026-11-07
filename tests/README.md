# FileRead regressions

The file reader can be tested without installing FFmpeg or libui:

```sh
c++ -std=c++11 -D_FILE_OFFSET_BITS=64 -Isrc -Wall -Wextra -pedantic \
    src/file.cpp tests/file_read.cpp -o /tmp/untrunc-file-read-tests
/tmp/untrunc-file-read-tests
```

The test links the actual production file reader. It supplies only the
logging globals and endian helpers normally linked from common.cpp, whose
other functions depend on FFmpeg. Temporary fixtures use POSIX mkstemp.

Coverage includes empty and truncated files, complete and partial item
counts, typed reads, unchanged destination bytes beyond EOF, buffer refill
and direct-read paths, pointer prefetch, seeking, and zero-length requests.
This is a focused I/O regression test, not a full video-repair integration
test. Pass any argument only when demonstrating the original failures
without triggering its zero-size division by zero.
