# astc_dec
astc_dec is a single source file **LDR** ASTC texture decompressor with the Apache 2.0 license, derived from Google's open source Android sources. The code currently only supports the LDR modes, but it would be easy to get HDR modes working again (the code is just remarked out because it depends on an FP16 class I didn't have time to reimplement). Please note that this is not an official Google repo, and is unsupported by Google. I have only placed it here for convienance to others.

This decoder is derived from the original code here:

https://chromium.googlesource.com/external/deqp/+/refs/heads/master/framework/common/tcuAstcUtil.cpp
https://chromium.googlesource.com/external/deqp/+/refs/heads/master/framework/common/tcuAstcUtil.hpp

Unlike Google's astc-codec decompressor, I found no bugs in this decoder, and it's also still usable in debug builds (astc-codec is extremely slow in debug).

The testing code was removed to reduce the code size and number of external dependencies. I also replaced the external dependencies with custom implementations, to reduce the code size. 

This codec was validated at various block sizes by feeding it random blocks, decompressing them, and comparing the decoded output vs. astc_codec:

https://github.com/google/astc-codec

Note that there were several decoding bugs in astc-codec which I locally fixed (and reported to astc-codec's github bug tracker). I have validated 4x4, 8x8, 12x12, and a few other block sizes, with both sRGB decoding enabled or disabled.

Call this function to decode ASTC blocks to 8-bit RGBA pixels:

`bool decompress(uint8_t* pDst, const uint8_t* data, bool isSRGB, int blockWidth, int blockHeight);`

pDst is a pointer to the output pixels. The component byte order from lowest to highest memory location is is R, G, B, A.

data is a pointer to the ASTC block data.

Set isSRGB to true to enable sRGB 8->16 upscaling during decompression (before weight application). It's important that you set this flag to match whatever the encoder did, otherwise you'll introduce an extra ~1 LSB of error in the output:
https://www.khronos.org/registry/DataFormat/specs/1.2/dataformat.1.2.html#astc_weight_application

blockWidth/BlockHeight are the ASTC block dimensions, like 4x4, etc.

false is returned if the block is invalid or uses an HDR mode.

To re-enable HDR decoding, you would need to implement the FP16 class, and remark out the #if 0'd lines prefixed with a "rg" comment.

Note that internally, when sRGB is disabled the codec unpacks to linear float values, then it converts this back to linear 8-bit/component. Another decode function that provides floating point output could easily be added.
