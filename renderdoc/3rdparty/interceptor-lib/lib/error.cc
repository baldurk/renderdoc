/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "error.h"

#include <cstdio>
#include <vector>

using namespace interceptor;

Error::Error(const char *format, ...) {
  va_list args, copy_args;
  va_start(args, format);
  va_copy(copy_args, args);

  char buf[1024];
  size_t len = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (len >= sizeof(buf)) {
    std::vector<char> buf2(len + 1);
    vsnprintf(buf2.data(), buf2.size(), format, args);
    va_end(copy_args);
    message_.assign(buf2.data());
  } else {
    message_.assign(buf);
  }
}
