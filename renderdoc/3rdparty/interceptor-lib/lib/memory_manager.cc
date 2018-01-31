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

#include "memory_manager.h"

#include <sys/mman.h>
#include <unistd.h>
#include <cassert>

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

using namespace interceptor;

MemoryManager::MemoryManager(int prot, int flags)
    : prot_(prot), flags_(flags) {}

MemoryManager::~MemoryManager() {
  for (const auto &alloc : allocations_)
    munmap(reinterpret_cast<void *>(alloc.start()), alloc.size());
}

void *MemoryManager::Allocate(size_t size, size_t alignment,
                              uintptr_t range_start, uintptr_t range_end) {
  assert(size > 0 && "Can't allocate 0 or negative amount of memory.");
  assert(size <= PAGE_SIZE && "Can't allocate more then PAGE_SIZE memory");
  assert(PAGE_SIZE % alignment == 0 &&
         "Can met alignment requiremenet not "
         "satisfied by a page boundary");

  for (Allocation &alloc : allocations_) {
    if (void *addr = alloc.Alloc(size, alignment, range_start, range_end))
      return addr;
    if (range_start >= alloc.start() && range_start <= alloc.end())
      range_start = alloc.end();
  }
  void *target = mmap(reinterpret_cast<void *>(range_start), PAGE_SIZE, prot_,
                      flags_, 0, 0);
  if (!target) return nullptr;

  allocations_.emplace_back(target, PAGE_SIZE);
  return allocations_.back().Alloc(size, alignment, range_start, range_end);
}

void *MemoryManager::Allocation::Alloc(size_t size, size_t alignment,
                                       uintptr_t range_start,
                                       uintptr_t range_end) {
  size_t new_offset = CalculateNewOffset(size, alignment);
  if (new_offset + size > size_) return nullptr;  // Doesn't fit

  uintptr_t address = reinterpret_cast<uintptr_t>(start_ + new_offset);
  if (address < range_start || address > range_end)
    return nullptr;  // Out of range

  offset_ = new_offset + size;
  return start_ + new_offset;
}

size_t MemoryManager::Allocation::CalculateNewOffset(size_t size,
                                                     size_t alignment) const {
  size_t new_offset = offset_;
  if (new_offset % alignment != 0)
    new_offset += alignment - (new_offset % alignment);
  return new_offset;
}
