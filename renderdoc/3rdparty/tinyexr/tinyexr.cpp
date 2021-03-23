#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <stdio.h>
#include <time.h>
#include "miniz/miniz.h"

// local variable is initialized but not referenced
#pragma warning(disable : 4189)
// signed/unsigned mismatch
#pragma warning(disable : 4018)
#pragma warning(disable : 4389)

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"
