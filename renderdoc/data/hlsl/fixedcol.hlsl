/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Baldur Karlsson
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

#define RED 0
#define GREEN 1
#define HIGHLIGHT 2
#define WIREFRAME 3

float4 main() : SV_Target0
{
#if VARIANT == RED
  return float4(1.0f, 0.0f, 0.0f, 1.0f);
#elif VARIANT == GREEN
  return float4(0.0f, 1.0f, 0.0f, 1.0f);
#elif VARIANT == HIGHLIGHT
  return float4(0.8f, 0.1f, 0.8f, 1.0f);
#elif VARIANT == WIREFRAME
  return float4(200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 1.0f);
#else
  return float4(1.0f, 0.0f, 1.0f, 1.0f);
#endif
}
