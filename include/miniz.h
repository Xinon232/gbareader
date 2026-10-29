#pragma once

// GBA Reader configuration: tinfl only, no allocation, compressor or ZIP API.
#define MINIZ_NO_MALLOC
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#define MINIZ_LITTLE_ENDIAN 1
#define MINIZ_HAS_64BIT_REGISTERS 0
#include "miniz_common.h"
#include "miniz_tinfl.h"
