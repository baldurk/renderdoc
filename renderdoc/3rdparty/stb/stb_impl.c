// Implementation file for stb headers - public domain -  https://github.com/nothings/stb

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(x)

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_ASSERT(x)

#define STB_TRUETYPE_IMPLEMENTATION

#pragma warning(disable:4996) // function unsafe (fopen vs fopen_s)
#pragma warning(disable:4456) // declaration hides previous local declaration
#pragma warning(disable:4457) // declaration hides function parameter
#pragma warning(disable:4189) // local variable is initialized but not referenced
#pragma warning(disable:4244) // <narrowing conversion>, possible loss of data
#pragma warning(disable:4702) // unreachable code
#pragma warning(disable:4204) // nonstandard extension used: non-constant aggregate initializer 

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"
#include "stb_truetype.h"
