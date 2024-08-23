#pragma once

#include <stdint.h>

struct  R2dFileHeader
{
    uint8_t signature[4]; // 'R2d!'
    uint32_t w;
    uint32_t h;
    uint8_t pixelStride;
    uint8_t componentCount;
    uint8_t flags;
    uint8_t packingType;
    uint8_t componentTypes[4];
    uint32_t blockCount;
    uint32_t skip;      // amount of bytes to skip for future extension
    bool CheckSignature() const;
    bool IsSupportedR2dFormat() const;
};


bool r2dIsMagicHeader(const uint8_t* fourBytesHeader);
bool r2dImageLoadFromMemory(const uint8_t* src, size_t inSize, uint8_t* dst, size_t dstBufferSize);
