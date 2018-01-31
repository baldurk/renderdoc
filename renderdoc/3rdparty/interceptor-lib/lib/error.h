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

#ifndef INTERCEPTOR_ERROR_H_
#define INTERCEPTOR_ERROR_H_

#include <string>

namespace interceptor {

class Error {
 public:
  Error() = default;
  Error(const char *format, ...) __attribute__((format(printf, 2, 3)));

  bool Fail() const { return !Success(); }
  bool Success() const { return message_.empty(); }
  bool operator()() const { return Success(); }

  const std::string &GetMessage() const { return message_; }

 private:
  std::string message_;
};

}  // end of namespace interceptor

#endif  // INTERCEPTOR_ERROR_H_
