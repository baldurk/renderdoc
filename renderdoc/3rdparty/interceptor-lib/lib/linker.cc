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

#include "linker.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"

// link.h #define EV_NONE on android what is used as an enum value inside llvm
// so it have to be included after the llvm libraries to avoid the compilation
// error.
#include <link.h>

#include "error.h"

using namespace interceptor;

static std::vector<std::string> GetLibrarySearchPaths() {
  if (sizeof(void *) == 4) return {"/system/lib", "/vendor/lib"};

  if (sizeof(void *) == 8) return {"/system/lib64", "/vendor/lib64"};

  return {};
}

static uintptr_t read_address(const char *buffer) {
  for (uintptr_t addr = 0; true; ++buffer) {
    if (*buffer >= '0' && *buffer <= '9') {
      addr *= 16;
      addr += (*buffer) - '0';
    } else if (*buffer >= 'a' && (*buffer) <= 'f') {
      addr *= 16;
      addr += (*buffer) - 'a' + 10;
    } else {
      return addr;
    }
  }
}

// Try to find the full path of the library mapped at a given address based on
// the /proc/self/maps file. This code path is used when the linker reports a
// library with file name only what is not located on the default search path
// or if the linker reports a library with bogus library name.
static std::string FindLibraryAtAddress(uintptr_t base_address) {
  FILE *f = fopen("/proc/self/maps", "r");
  if (f == nullptr) return "";

  char buffer[512];
  std::string path;
  while (fgets(buffer, sizeof(buffer), f)) {
    uintptr_t start_addr = read_address(buffer);
    if (start_addr == base_address) {
      const char *last_space = buffer - 1, *last_alpha = buffer;
      for (const char *it = buffer; *it; ++it) {
        if (*it == ' ')
          last_space = it;
        else if (std::isalnum(*it))
          last_alpha = it;
      }
      path.assign(last_space + 1, last_alpha - last_space);
    }
  }
  fclose(f);
  return path;
}

static std::string FindLibrary(const char *name, uintptr_t base_address) {
  // Absolue library path
  if (name[0] == '/') {
    if (llvm::sys::fs::exists(name)) return name;
    return "";
  }

  // Relative library path
  std::vector<std::string> search_paths = GetLibrarySearchPaths();
  for (const std::string &dir : search_paths) {
    std::string path = dir + '/' + name;
    if (llvm::sys::fs::exists(path)) return path;
  }

  // Finding the library based on absolute and relative path is failed. Try to
  // find it based on the base address in /proc/self/maps
  std::string path = FindLibraryAtAddress(base_address);
  if (llvm::sys::fs::exists(path)) return path;

  return "";
}

Error Linker::ParseLibrary(const char *name, uintptr_t base_address) {
  std::string library_path = FindLibrary(name, base_address);
  if (library_path.empty())
    return Error("Failed to find file for library: %s", name);

  llvm::Expected<llvm::object::OwningBinary<llvm::object::Binary>>
      binary_or_err = llvm::object::createBinary(library_path);
  if (!binary_or_err) {
    std::error_code ec = llvm::errorToErrorCode(binary_or_err.takeError());
    return Error("Failed to create llvm::object::Binary for '%s' (%s)",
                 library_path.c_str(), ec.message().c_str());
  }

  llvm::object::Binary &binary = *binary_or_err.get().getBinary();
  llvm::object::ObjectFile *obj_file =
      llvm::dyn_cast<llvm::object::ObjectFile>(&binary);
  if (!obj_file || !llvm::isa<llvm::object::ELFObjectFileBase>(obj_file))
    return Error("Failer to convert '%s' to an object file",
                 library_path.c_str());

  for (const auto &symbol : obj_file->symbols()) {
    llvm::Expected<llvm::StringRef> symbol_name_or_err = symbol.getName();
    if (!symbol_name_or_err || symbol_name_or_err->empty()) continue;

    std::string symbol_name = symbol_name_or_err->str();
    uint32_t flags = symbol.getFlags();
    uintptr_t address = 0;

    if (flags & llvm::object::SymbolRef::SF_Absolute) {
      llvm::Expected<uint64_t> exp_address = symbol.getAddress();
      if (!exp_address) continue;
      address = exp_address.get();
    } else {
      llvm::Expected<llvm::object::section_iterator> section_or_err =
          symbol.getSection();
      if (!section_or_err) continue;

      llvm::object::section_iterator section = section_or_err.get();
      if (section == obj_file->section_end()) continue;  // Undefined symbol

      llvm::Expected<uint64_t> exp_address = symbol.getAddress();
      if (!exp_address) continue;
      address = base_address + exp_address.get();
    }

    if (flags & llvm::object::SymbolRef::SF_Thumb) address |= 1;

    AddSymbol(symbol_name, address,
              llvm::object::ELFSymbolRef(symbol).getSize(), flags);
  }

  return Error();
}

void Linker::AddSymbol(const std::string &name, uintptr_t address, size_t size,
                       uint32_t flags) {
  Symbol symbol{name, address, size, flags};
  addr_to_symbol_.emplace(address, symbol);
  name_to_symbol_.emplace(name, symbol);
}

std::vector<Linker::Symbol> Linker::FindSymbols(const char *name) {
  RefreshSymbolList();

  std::vector<Symbol> symbols;
  auto symbol_range = name_to_symbol_.equal_range(name);
  for (auto it = symbol_range.first; it != symbol_range.second; ++it)
    symbols.push_back(it->second);
  return symbols;
}

static int CollectLibrary(struct dl_phdr_info *info, size_t size, void *data) {
  std::map<uintptr_t, std::string> *libraries =
      static_cast<std::map<uintptr_t, std::string> *>(data);

  const char *file_name = info->dlpi_name;
  if (file_name == nullptr) return 0;

  uintptr_t base_address = info->dlpi_addr;
  libraries->emplace(base_address, file_name);
  return 0;
}

void Linker::RefreshSymbolList() {
  std::map<uintptr_t, std::string> libraries;
  dl_iterate_phdr(CollectLibrary, &libraries);

  bool changed = false;
  if (libraries.size() == loaded_libraries_.size()) {
    for (const auto &it : loaded_libraries_) {
      if (libraries.count(it.first) != 1 || libraries[it.first] != it.second) {
        changed = true;
        break;
      }
    }
  } else {
    changed = true;
  }

  if (changed) {
    addr_to_symbol_.clear();
    name_to_symbol_.clear();
    loaded_libraries_.swap(libraries);
    for (const auto &it : loaded_libraries_)
      ParseLibrary(it.second.c_str(), it.first);
  }
}
