// Implementation file for stb headers - public domain -  https://github.com/nothings/stb

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)

#define STB_IMAGE_WRITE_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION

#pragma warning(disable:4996) // function unsafe (fopen vs fopen_s)
#pragma warning(disable:4456) // declaration hides previous local declaration
#pragma warning(disable:4457) // declaration hides function parameter

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_truetype.h"
