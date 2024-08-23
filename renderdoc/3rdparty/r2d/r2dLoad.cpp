#include "r2dLoad.h"
#include "lz4/lz4.h"

bool r2dIsMagicHeader(const uint8_t* fourBytesHeader)
{
    return 'R' == fourBytesHeader[0] &&
        '2' == fourBytesHeader[1] &&
        'd' == fourBytesHeader[2] &&
        '!' == fourBytesHeader[3];
}

enum kR2dFlags
{
    kR2dFloatPixel = 1<<0,
    kR2dByteStreams = 1<<1
};

enum kR2dPackingMethod
{
    kR2dPackNone = 0,
    kR2dPackLZ4 = 1
};

// Right now we only support RGBA float, LZ4 packed, byte stream un-interleaved
bool R2dFileHeader::IsSupportedR2dFormat() const
{
    if (pixelStride != 16)
        return false;
    if (componentCount != 4)
        return false;
    if (0 == (flags & kR2dFloatPixel))    // float pixel
        return false;
    if (0 == (flags & kR2dByteStreams))    // un-interleaved byte stream
        return false;
    for (int i=0;i<4;i++)           // RGBA
        if (componentTypes[i] != i)
            return false;
    if (packingType != uint8_t(kR2dPackLZ4))    // LZ4 packing
        return false;
    if (skip != 0)       // does not support additional future data for now
        return false;

    return true;
}

bool r2dImageLoadFromMemory(const uint8_t* src, size_t inSize, uint8_t* dst, size_t dstBufferSize)
{
    bool ret = false;

    if (inSize < sizeof(R2dFileHeader))
        return false;
    const R2dFileHeader* header = (const R2dFileHeader*)src;
    if (!header->IsSupportedR2dFormat())
        return false;

    if (dstBufferSize < header->w * header->h * 4 * sizeof(float))   // right now r2d only supports ARGB-float32
        return false;

    const uint32_t* read = (const uint32_t*)(header+1);
    const uint8_t* packedData = (const uint8_t*)(read + header->blockCount * 2);    // 2 * 32bits per entry

    uint32_t y0 = 0;
    for (uint32_t b=0;b<header->blockCount;b++)
    {
        // depack each block
        uint32_t scanlines = *read++;
        uint32_t packedSize = *read++;

        // alloc temp buffer
        const int pixelCount = header->w * scanlines;
        const int unpackedBlockSize = pixelCount * 4 * sizeof(float);
        uint8_t* tmp = (uint8_t*)malloc(unpackedBlockSize);

        // LZ4 depack
        int lzRet = LZ4_decompress_safe((const char*)packedData, (char*)tmp, (int)packedSize, (int)unpackedBlockSize);
        if (lzRet == unpackedBlockSize)
        {
            // un-interleave float data
            uint8_t* write = dst + y0 * header->w * 4 * sizeof(float);
            for (int i=0;i<pixelCount;i++)
            {
                for (int j = 0; j < 4*sizeof(float);j++)
                    *write++ = tmp[i + j*pixelCount];
            }
        }
        else
        {
            free(tmp);
            return false;
        }

        free(tmp);

        y0 += scanlines;
        packedData += packedSize;
    }

    if ( y0 == header->h)
    {
        const float* pixels = (const float*)dst;
        printf("%p\n", pixels);
        ret = true;
    }

    return ret;
}

