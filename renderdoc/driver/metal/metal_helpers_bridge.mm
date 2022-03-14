/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "metal_helpers_bridge.h"
#import <AppKit/AppKit.h>
#import <Foundation/NSStream.h>

void ObjC::Get_defaultLibraryData(bytebuf &buffer)
{
  NSBundle *mainAppBundle = [NSBundle mainBundle];
  NSString *defaultLibaryPath = [mainAppBundle pathForResource:@"default" ofType:@"metallib"];
  NSData *myData = [NSData dataWithContentsOfFile:defaultLibaryPath];
  dispatch_data_t data = dispatch_data_create(
      myData.bytes, myData.length, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  NSData *nsData = (NSData *)data;
  buffer.resize(nsData.length);
  memcpy(buffer.data(), nsData.bytes, buffer.size());
  dispatch_release(data);
}
