/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"

rdcstr strlower(const rdcstr &str);
rdcstr strupper(const rdcstr &str);

uint32_t strhash(const char *str, uint32_t existingHash = 5381);

rdcstr get_basename(const rdcstr &path);
rdcstr get_dirname(const rdcstr &path);
rdcstr strip_extension(const rdcstr &path);

// remove everything but alphanumeric ' ' and '.'
// It replaces everything else with _
// for logging strings where they might contain garbage characters
void strip_nonbasic(rdcstr &str);

void split(const rdcstr &in, rdcarray<rdcstr> &out, const char sep);
void merge(const rdcarray<rdcstr> &in, rdcstr &out, const char sep);
