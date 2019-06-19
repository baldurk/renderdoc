/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2019 Baldur Karlsson
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
******************************************************************************/

#include "common/globalconfig.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

#include "vk_resources.h"

#include <stdint.h>
#include <vector>

TEST_CASE("Test ImgRefs type", "[imgrefs]")
{
  SECTION("unsplit")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 0);
  };
  SECTION("split aspect")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    imgRefs.Split(true, false, false);
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 1);
  };
  SECTION("split levels")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    imgRefs.Split(false, true, false);
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 2);
  };
  SECTION("split layers")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    imgRefs.Split(false, false, true);
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 5);
  };
  SECTION("split aspect and levels")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    imgRefs.Split(true, true, false);
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 11 + 2);
  };
  SECTION("split aspect and layers")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    imgRefs.Split(true, false, true);
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 17 + 5);
  };
  SECTION("split levels and layers")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    imgRefs.Split(false, true, true);
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 2 * 17 + 5);
  };
  SECTION("split aspect and levels and layers")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    imgRefs.Split(true, true, true);
    CHECK(imgRefs.SubresourceIndex(VK_IMAGE_ASPECT_STENCIL_BIT, 2, 5) == 11 * 17 + 2 * 17 + 5);
  };
  SECTION("update unsplit")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    ImageRange range;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_Read};
    CHECK(imgRefs.rangeRefs == expected);
  };
  SECTION("update split aspect")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    ImageRange range;
    range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_None, eFrameRef_Read};
    CHECK(imgRefs.rangeRefs == expected);
  };
  SECTION("update split levels")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    ImageRange range;
    range.baseMipLevel = 1;
    range.levelCount = 3;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_None, eFrameRef_Read, eFrameRef_Read,
                                          eFrameRef_Read, eFrameRef_None, eFrameRef_None,
                                          eFrameRef_None, eFrameRef_None, eFrameRef_None,
                                          eFrameRef_None, eFrameRef_None};
    CHECK(imgRefs.rangeRefs == expected);
  };
  SECTION("update split layers")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    ImageRange range;
    range.baseArrayLayer = 7;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {
        eFrameRef_None, eFrameRef_None, eFrameRef_None, eFrameRef_None, eFrameRef_None,
        eFrameRef_None, eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_Read,
        eFrameRef_Read, eFrameRef_Read, eFrameRef_Read, eFrameRef_Read, eFrameRef_Read,
        eFrameRef_Read, eFrameRef_Read};
    CHECK(imgRefs.rangeRefs == expected);
  };
  SECTION("update split aspect then levels")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 11, 17, 1));
    ImageRange range0;
    range0.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    imgRefs.Update(range0, eFrameRef_Read);
    ImageRange range1;
    range1.baseMipLevel = 5;
    range1.levelCount = 2;
    imgRefs.Update(range1, eFrameRef_PartialWrite);
    std::vector<FrameRefType> expected = {
        // VK_IMAGE_ASPECT_DEPTH_BIT
        eFrameRef_None, eFrameRef_None, eFrameRef_None, eFrameRef_None, eFrameRef_None,
        eFrameRef_PartialWrite, eFrameRef_PartialWrite, eFrameRef_None, eFrameRef_None,
        eFrameRef_None, eFrameRef_None,
        // VK_IMAGE_ASPECT_STENCIL_BIT
        eFrameRef_Read, eFrameRef_Read, eFrameRef_Read, eFrameRef_Read, eFrameRef_Read,
        eFrameRef_ReadBeforeWrite, eFrameRef_ReadBeforeWrite, eFrameRef_Read, eFrameRef_Read,
        eFrameRef_Read, eFrameRef_Read,
    };
    CHECK(imgRefs.rangeRefs == expected);
  }
  SECTION("update split layers then aspects and levels")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 1}, 7, 5, 1));
    ImageRange range0;
    range0.baseArrayLayer = 1;
    range0.layerCount = 2;
    imgRefs.Update(range0, eFrameRef_Read);
    ImageRange range1;
    range1.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    range1.baseMipLevel = 2;
    range1.levelCount = 3;
    imgRefs.Update(range1, eFrameRef_PartialWrite);
    std::vector<FrameRefType> expected = {
        // (Depth, level 0)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Depth, level 1)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Depth, level 2)
        eFrameRef_PartialWrite, eFrameRef_ReadBeforeWrite, eFrameRef_ReadBeforeWrite,
        eFrameRef_PartialWrite, eFrameRef_PartialWrite,
        // (Depth, level 3)
        eFrameRef_PartialWrite, eFrameRef_ReadBeforeWrite, eFrameRef_ReadBeforeWrite,
        eFrameRef_PartialWrite, eFrameRef_PartialWrite,
        // (Depth, level 4)
        eFrameRef_PartialWrite, eFrameRef_ReadBeforeWrite, eFrameRef_ReadBeforeWrite,
        eFrameRef_PartialWrite, eFrameRef_PartialWrite,
        // (Depth, level 5)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Depth, level 6)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Stencil, level 0)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Stencil, level 1)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Stencil, level 2)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Stencil, level 3)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Stencil, level 4)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Stencil, level 5)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,
        // (Stencil, level 6)
        eFrameRef_None, eFrameRef_Read, eFrameRef_Read, eFrameRef_None, eFrameRef_None,

    };
    CHECK(imgRefs.rangeRefs == expected);
  }
  SECTION("update 3D image default view")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 5}, 11, 1, 1));
    ImageRange range;
    range.layerCount = 1;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_Read};
    CHECK(imgRefs.rangeRefs == expected);
  }
  SECTION("update 3D image 3D view")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 5}, 11, 1, 1));
    ImageRange range;
    range.layerCount = 1;
    range.viewType = VK_IMAGE_VIEW_TYPE_3D;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_Read};
    CHECK(imgRefs.rangeRefs == expected);
  }
  SECTION("update 3D image 2D view")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 5}, 11, 1, 1));
    ImageRange range;
    range.layerCount = 1;
    range.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_Read, eFrameRef_None, eFrameRef_None,
                                          eFrameRef_None, eFrameRef_None};
    CHECK(imgRefs.rangeRefs == expected);
  }
  SECTION("update 3D image 2D array view")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 5}, 11, 1, 1));
    ImageRange range;
    range.baseArrayLayer = 1;
    range.layerCount = 2;
    range.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_None, eFrameRef_Read, eFrameRef_Read,
                                          eFrameRef_None, eFrameRef_None};
    CHECK(imgRefs.rangeRefs == expected);
  }
  SECTION("update 3D image 2D array view full")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 5}, 11, 1, 1));
    ImageRange range;
    range.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_Read};
    CHECK(imgRefs.rangeRefs == expected);
  }
  SECTION("update 3D image 3D view full")
  {
    ImgRefs imgRefs(ImageInfo(VK_FORMAT_D16_UNORM_S8_UINT, {100, 100, 5}, 11, 1, 1));
    ImageRange range;
    range.viewType = VK_IMAGE_VIEW_TYPE_3D;
    imgRefs.Update(range, eFrameRef_Read);
    std::vector<FrameRefType> expected = {eFrameRef_Read};
    CHECK(imgRefs.rangeRefs == expected);
  }
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
