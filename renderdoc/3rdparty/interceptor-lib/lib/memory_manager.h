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

#ifndef INTERCEPTOR_MEMORY_MANAGER_H_
#define INTERCEPTOR_MEMORY_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace interceptor {

class MemoryManager {
 public:
  MemoryManager(int prot, int flags);
  ~MemoryManager();

  void *Allocate(size_t size, size_t alignment,
                 uintptr_t range_start = std::numeric_limits<uintptr_t>::min(),
                 uintptr_t range_end = std::numeric_limits<uintptr_t>::max());

 private:
  class Allocation {
   public:
    Allocation(void *start, size_t size)
        : start_(static_cast<uint8_t *>(start)), size_(size), offset_(0) {}

    uintptr_t start() const { return reinterpret_cast<uintptr_t>(start_); }
    uintptr_t end() const { return start() + size_; }
    size_t size() const { return size_; }

    void *Alloc(size_t size, size_t alignment, uintptr_t range_start,
                uintptr_t range_end);

   private:
    uint8_t *const start_;
    const size_t size_;
    size_t offset_;

    size_t CalculateNewOffset(size_t size, size_t alignment) const;
  };

  const int prot_;
  const int flags_;

  std::vector<Allocation> allocations_;
};

}  // end of namespace interceptor

#endif  // INTERCEPTOR_MEMORY_MANAGER_H_
