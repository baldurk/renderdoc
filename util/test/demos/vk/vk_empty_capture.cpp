/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "vk_test.h"

RD_TEST(VK_Empty_Capture, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws nothing but will trigger a completely empty capture at frame 10.";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();

      if(rdoc)
      {
        rdoc->StartFrameCapture(NULL, NULL);
        rdoc->EndFrameCapture(NULL, NULL);
        msleep(1000);
      }

      if(curFrame > 50)
        break;
    }

    return 0;
  }
};

REGISTER_TEST();
