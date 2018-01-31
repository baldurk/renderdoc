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

#ifndef INTERCEPTOR_LINKER_H_
#define INTERCEPTOR_LINKER_H_

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "error.h"

namespace interceptor {

class Linker {
 public:
  struct Symbol {
    std::string name;
    uintptr_t address;
    size_t size;
    uint32_t flags;
  };

  void RefreshSymbolList();

  std::vector<Symbol> FindSymbols(const char *name);

 private:
  Error ParseLibrary(const char *name, uintptr_t base_address);

  void AddSymbol(const std::string &name, uintptr_t address, size_t size,
                 uint32_t flags);

  std::map<uintptr_t, std::string> loaded_libraries_;
  std::multimap<uintptr_t, Symbol> addr_to_symbol_;
  std::multimap<std::string, Symbol> name_to_symbol_;
};

}  // end of namespace interceptor

#endif  // INTERCEPTOR_LINKER_H_
