# miniz provenance

`include/miniz_common.h`, `include/miniz_tinfl.h`, and `src/miniz_tinfl.c` are the
tinfl-only files vendored from miniz commit
`77d0dce8627735138c51770d1799a1ef48f2117d`. `include/miniz.h` is a local
configuration shim disabling allocation, compression, stdio, time, zlib, and
archive APIs. The upstream MIT license is in this directory.
