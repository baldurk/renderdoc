// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CLIENT_WINDOWS_COMMON_IPC_PROTOCOL_H__
#define CLIENT_WINDOWS_COMMON_IPC_PROTOCOL_H__

#include <Windows.h>
#include "dbghelp/DbgHelp.h"
#include <string>
#include <utility>
#include "breakpad/common/windows/string_utils-inl.h"
#include "breakpad/google_breakpad/common/minidump_format.h"

namespace google_breakpad {
	
// These entries store a list of memory regions that the client wants included
// in the minidump.
struct AppMemory {
  ULONG64 ptr;
  ULONG length;

  bool operator==(const struct AppMemory& other) const {
    return ptr == other.ptr;
  }

  bool operator==(const void* other) const {
    return ptr == reinterpret_cast<ULONG64>(other);
  }
};

struct AppMemoryInfo {
  AppMemoryInfo() : entries(NULL), count(0) {}
  const AppMemory* entries;
  ULONG count;
};

// Name/value pair for custom client information.
struct CustomInfoEntry {
  // Maximum length for name and value for client custom info.
  static const int kNameMaxLength = 64;
  static const int kValueMaxLength = 320; // large enough for MAX_PATH

  CustomInfoEntry() {
    // Putting name and value in initializer list makes VC++ show warning 4351.
    set_name(NULL);
    set_value(NULL);
  }

  CustomInfoEntry(const wchar_t* name_arg, const wchar_t* value_arg) {
    set_name(name_arg);
    set_value(value_arg);
  }

  void set_name(const wchar_t* name_arg) {
    if (!name_arg) {
      name[0] = L'\0';
      return;
    }
    WindowsStringUtils::safe_wcscpy(name, kNameMaxLength, name_arg);
  }

  void set_value(const wchar_t* value_arg) {
    if (!value_arg) {
      value[0] = L'\0';
      return;
    }

    WindowsStringUtils::safe_wcscpy(value, kValueMaxLength, value_arg);
  }

  void set(const wchar_t* name_arg, const wchar_t* value_arg) {
    set_name(name_arg);
    set_value(value_arg);
  }

  wchar_t name[kNameMaxLength];
  wchar_t value[kValueMaxLength];
};

// Constants for the protocol between client and the server.

// Tags sent with each message indicating the purpose of
// the message.
enum MessageTag {
  MESSAGE_TAG_NONE = 0,
  MESSAGE_TAG_REGISTRATION_REQUEST = 1,
  MESSAGE_TAG_REGISTRATION_RESPONSE = 2,
  MESSAGE_TAG_REGISTRATION_ACK = 3,
  MESSAGE_TAG_UPLOAD_REQUEST = 4
};

struct CustomClientInfo {
  const CustomInfoEntry* entries;
  size_t count;
};

// Message structure for IPC between crash client and crash server.
struct ProtocolMessage {
  ProtocolMessage()
      : tag(MESSAGE_TAG_NONE),
        id(0),
        dump_type(MiniDumpNormal),
        thread_id(0),
        exception_pointers(NULL),
        assert_info(NULL),
        custom_client_info(),
		app_memory_info(NULL),
        dump_request_handle(NULL),
        dump_generated_handle(NULL),
        server_alive_handle(NULL) {
  }

  // Creates an instance with the given parameters.
  ProtocolMessage(MessageTag arg_tag,
                  DWORD arg_id,
                  MINIDUMP_TYPE arg_dump_type,
                  DWORD* arg_thread_id,
                  EXCEPTION_POINTERS** arg_exception_pointers,
                  MDRawAssertionInfo* arg_assert_info,
                  const CustomClientInfo& custom_info,
				  AppMemoryInfo* app_mem_info,
                  HANDLE arg_dump_request_handle,
                  HANDLE arg_dump_generated_handle,
                  HANDLE arg_server_alive)
    : tag(arg_tag),
      id(arg_id),
      dump_type(arg_dump_type),
      thread_id(arg_thread_id),
      exception_pointers(arg_exception_pointers),
      assert_info(arg_assert_info),
      custom_client_info(custom_info),
	  app_memory_info(app_mem_info),
      dump_request_handle(arg_dump_request_handle),
      dump_generated_handle(arg_dump_generated_handle),
      server_alive_handle(arg_server_alive) {
  }

  // Tag in the message.
  MessageTag tag;

  // The id for this message. This may be either a process id or a crash id
  // depending on the type of message.
  DWORD id;

  // Dump type requested.
  MINIDUMP_TYPE dump_type;

  // Client thread id pointer.
  DWORD* thread_id;

  // Exception information.
  EXCEPTION_POINTERS** exception_pointers;

  // Assert information in case of an invalid parameter or
  // pure call failure.
  MDRawAssertionInfo* assert_info;

  // Custom specified app regions of memory
  AppMemoryInfo* app_memory_info;

  // Custom client information.
  CustomClientInfo custom_client_info;

  // Handle to signal the crash event.
  HANDLE dump_request_handle;

  // Handle to check if server is done generating crash.
  HANDLE dump_generated_handle;

  // Handle to a mutex that becomes signaled (WAIT_ABANDONED)
  // if server process goes down.
  HANDLE server_alive_handle;

 private:
  // Disable copy ctor and operator=.
  ProtocolMessage(const ProtocolMessage& msg);
  ProtocolMessage& operator=(const ProtocolMessage& msg);
};

}  // namespace google_breakpad

#endif  // CLIENT_WINDOWS_COMMON_IPC_PROTOCOL_H__
