/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2017 Baldur Karlsson
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

#pragma once

#define VER_STR2(a) #a
#define VER_STR(a) VER_STR2(a)

#define RENDERDOC_VERSION_MAJOR 0
#define RENDERDOC_VERSION_MINOR 34
#define RENDERDOC_VERSION_STRING \
  VER_STR(RENDERDOC_VERSION_MAJOR) "." VER_STR(RENDERDOC_VERSION_MINOR)

#if defined(GIT_COMMIT_HASH_LITERAL)
#define GIT_COMMIT_HASH VER_STR(GIT_COMMIT_HASH_LITERAL)
#elif !defined(GIT_COMMIT_HASH)
#define GIT_COMMIT_HASH "NO_GIT_COMMIT_HASH_DEFINED"
#endif
