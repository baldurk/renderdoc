/******************************************************************************
 * The MIT License (MIT)
 * 
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



#if USE_BREAKPAD && defined(RENDERDOC_OFFICIAL_BUILD)

#define CRASH_HANDLER_ENABLED

// breakpad
#include "breakpad/client/windows/handler/exception_handler.h"
#include "breakpad/client/windows/common/ipc_protocol.h"

class CrashHandler : public ICrashHandler
{
	public:
		CrashHandler(ICrashHandler *existing)
		{
			m_ExHandler = NULL;

			google_breakpad::AppMemoryList mem;

			if(existing)
				mem = ((CrashHandler *)existing)->m_ExHandler->QueryRegisteredAppMemory();

			SAFE_DELETE(existing);

			///////////////////

			wchar_t tempPath[MAX_PATH] = {0};
			GetTempPathW(MAX_PATH-1, tempPath);

			wstring dumpFolder = tempPath;
			dumpFolder += L"RenderDocDumps";

			CreateDirectoryW(dumpFolder.c_str(), NULL);

			MINIDUMP_TYPE dumpType = MINIDUMP_TYPE(MiniDumpNormal|MiniDumpWithIndirectlyReferencedMemory);

			{
				PROCESS_INFORMATION pi;
				STARTUPINFOW si;
				RDCEraseEl(pi);
				RDCEraseEl(si);

				HANDLE waitEvent = CreateEventA(NULL, TRUE, FALSE, "RENDERDOC_CRASHHANDLE");

				wchar_t radpath[MAX_PATH] = {0};
				GetModuleFileNameW(GetModuleHandleA("renderdoc.dll"), radpath, MAX_PATH-1);

				size_t len = wcslen(radpath);

				wchar_t *slash = wcsrchr(radpath, L'\\');

				if(slash)
				{
					*slash = 0;
				}
				else
				{
					slash = wcsrchr(radpath, L'/');

					if(slash)
						*slash = 0;
					else
					{
						radpath[0] = '.';
						radpath[1] = 0;
					}
				}

				wstring cmdline = L"\"";
				cmdline += radpath;
				cmdline += L"/renderdoccmd.exe\" --crashhandle";
				
				wchar_t *paramsAlloc = new wchar_t[512];

				wcscpy_s(paramsAlloc, 511, cmdline.c_str());

				CreateProcessW(NULL, paramsAlloc, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

				DWORD res = WaitForSingleObject(waitEvent, 2000);

				CloseHandle(waitEvent);
			}

			static google_breakpad::CustomInfoEntry breakpadCustomInfo[] = {
				google_breakpad::CustomInfoEntry(L"version", RENDERDOC_VERSION_STRING_W),
				google_breakpad::CustomInfoEntry(L"logpath", RDCGETLOGFILE()),
				google_breakpad::CustomInfoEntry(L"gitcommit", WIDEN(GIT_COMMIT_HASH)),
			};

			breakpadCustomInfo[1].set_value(RDCGETLOGFILE());

			google_breakpad::CustomClientInfo custom = { &breakpadCustomInfo[0], ARRAY_COUNT(breakpadCustomInfo) };

			_CrtSetReportMode(_CRT_ASSERT, 0);
			m_ExHandler = new google_breakpad::ExceptionHandler(dumpFolder.c_str(),
				NULL,
				NULL,
				NULL,
				google_breakpad::ExceptionHandler::HANDLER_ALL,
				dumpType,
				L"\\\\.\\pipe\\RenderDocBreakpadServer",
				&custom);

			m_ExHandler->set_handle_debug_exceptions(true);

			for(size_t i=0; i < mem.size(); i++)
				m_ExHandler->RegisterAppMemory((void *)mem[i].ptr, mem[i].length);
		}

		virtual ~CrashHandler()
		{
			SAFE_DELETE(m_ExHandler);
		}

		void WriteMinidump()
		{
			m_ExHandler->WriteMinidump();
		}

		void WriteMinidump(void *data)
		{
			m_ExHandler->WriteMinidumpForException((EXCEPTION_POINTERS *)data);
		}

		void RegisterMemoryRegion(void *mem, size_t size)
		{
			m_ExHandler->RegisterAppMemory(mem, size);
		}

		void UnregisterMemoryRegion(void *mem)
		{
			m_ExHandler->UnregisterAppMemory(mem);
		}

	private:
		google_breakpad::ExceptionHandler *m_ExHandler;
};

#endif
