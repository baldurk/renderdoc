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

#include <windows.h>
#include <string>
#include <vector>

#include <replay/renderdoc.h>

#include "resource.h"

// breakpad
#include "common/windows/http_upload.h"
#include "client/windows/crash_generation/client_info.h"
#include "client/windows/crash_generation/crash_generation_server.h"

#include "miniz.h"

using std::string;
using std::wstring;
using std::vector;

using google_breakpad::ClientInfo;
using google_breakpad::CrashGenerationServer;

bool exitServer = false;

static HINSTANCE CrashHandlerInst = 0;
static HWND CrashHandlerWnd = 0;

bool uploadReport = false;
bool uploadDump = false;
bool uploadLog = false;
string reproSteps = "";

wstring dump = L"";
vector<google_breakpad::CustomInfoEntry> customInfo;
wstring logpath = L"";

HINSTANCE hInstance = NULL;

INT_PTR CALLBACK CrashHandlerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			HANDLE hIcon = LoadImage(CrashHandlerInst, MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);

			if(hIcon)
			{
				SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
				SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			}

			SetDlgItemTextW(hDlg, IDC_WELCOMETEXT,
				L"RenderDoc has encountered an unhandled exception or other similar unrecoverable error.\n\n" \
				L"If you had captured but not saved a logfile it should still be available in %TEMP% and will not be deleted," \
				L"you can try loading it again.\n\n" \
				L"A minidump has been created and the RenderDoc diagnostic log (NOT any capture logfile) is available if you would like " \
				L"to send them back to be analysed. The path for both is found below if you would like to inspect their contents and censor as appropriate.\n\n" \
				L"Neither contains any significant private information, the minidump has some internal states and local memory at the time of the " \
				L"crash & thread stacks, etc. The diagnostic log contains diagnostic messages like warnings and errors.\n\n" \
				L"The only other information sent is the version of RenderDoc, C# exception callstack, and any notes you include.\n\n" \
				L"Any repro steps or notes would be helpful to include with the report. If you'd like to be contacted about the bug " \
				L"e.g. for updates about its status just include your email & name. Thank you!\n\n" \
				L"Baldur (baldurk@baldurk.org)");

			SetDlgItemTextW(hDlg, IDC_DUMPPATH, dump.c_str());
			SetDlgItemTextW(hDlg, IDC_LOGPATH, logpath.c_str());

			CheckDlgButton(hDlg, IDC_SENDDUMP, BST_CHECKED);
			CheckDlgButton(hDlg, IDC_SENDLOG, BST_CHECKED);
		}

		case WM_SHOWWINDOW:
		{

			{
				RECT r;
				GetClientRect(hDlg, &r);

				int xPos = (GetSystemMetrics(SM_CXSCREEN) - r.right)/2;
				int yPos = (GetSystemMetrics(SM_CYSCREEN) - r.bottom)/2;

				SetWindowPos(hDlg, NULL, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			}

			return (INT_PTR)TRUE;
		}

		case WM_COMMAND:
		{
			int ID = LOWORD(wParam);

			if(ID == IDC_DONTSEND)
			{
				EndDialog(hDlg, 0);
				return (INT_PTR)TRUE;
			}
			else if(ID == IDC_SEND)
			{
				uploadReport = true;
				uploadDump = (IsDlgButtonChecked(hDlg, IDC_SENDDUMP) != 0);
				uploadLog = (IsDlgButtonChecked(hDlg, IDC_SENDLOG) != 0);

				char notes[4097] = {0};
				
				GetDlgItemTextA(hDlg, IDC_NAME, notes, 4096);
				notes[4096] = 0;

				reproSteps = "Name: ";
				reproSteps += notes;
				reproSteps += "\n";

				memset(notes, 0, 4096);
				GetDlgItemTextA(hDlg, IDC_EMAIL, notes, 4096);
				notes[4096] = 0;

				reproSteps += "Email: ";
				reproSteps += notes;
				reproSteps += "\n\n";
				
				memset(notes, 0, 4096);
				GetDlgItemTextA(hDlg, IDC_REPRO, notes, 4096);
				notes[4096] = 0;

				reproSteps += notes;

				EndDialog(hDlg, 0);
				return (INT_PTR)TRUE;
			}
		}
		break;
		
		case WM_QUIT:
		case WM_DESTROY:
		case WM_CLOSE:
		{
			EndDialog(hDlg, 0);
			return (INT_PTR)TRUE;
		}
	    break;
	}
	return (INT_PTR)FALSE;
}

static void _cdecl OnClientCrashed(void* context, const ClientInfo* client_info, const wstring* dump_path)
{
	if(dump_path)
	{
		dump = *dump_path;

		google_breakpad::CustomClientInfo custom = client_info->GetCustomInfo();

		for(size_t i=0; i < custom.count; i++)
			customInfo.push_back(custom.entries[i]);
	}

	exitServer = true;
}

static void _cdecl OnClientExited(void* context, const ClientInfo* client_info)
{
	exitServer = true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if(msg == WM_CLOSE)   { DestroyWindow(hwnd); return 0; }
	if(msg == WM_DESTROY) { PostQuitMessage(0);  return 0; }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

wstring GetUsername()
{
	wchar_t username[256] = {0};
	DWORD usersize = 255; 
	GetUserNameW(username, &usersize);

	return username;
}

void DisplayRendererPreview(ReplayRenderer *renderer)
{
	if(renderer == NULL) return;

	HWND wnd = 0;

	wnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"renderdoccmd", L"renderdoccmd", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
		NULL, NULL, hInstance, NULL);

	if(wnd == NULL)
	{
		return;
	}

	ShowWindow(wnd, SW_SHOW);
	UpdateWindow(wnd);

	rdctype::array<FetchTexture> texs;
	ReplayRenderer_GetTextures(renderer, &texs);

	ReplayOutput *out = ReplayRenderer_CreateOutput(renderer, wnd);

	ReplayRenderer_SetFrameEvent(renderer, 0, 10000000);

	OutputConfig c;
	c.m_Type = eOutputType_TexDisplay;

	ReplayOutput_SetOutputConfig(out, c);

	for(int32_t i=0; i < texs.count; i++)
	{
		if(texs[i].creationFlags & eTextureCreate_SwapBuffer)
		{
			TextureDisplay d;
			d.texid = texs[i].ID;
			d.mip = 0;
			d.sampleIdx = ~0U;
			d.overlay = eTexOverlay_None;
			d.CustomShader = ResourceId();
			d.HDRMul = -1.0f;
			d.linearDisplayAsGamma = true;
			d.FlipY = false;
			d.rangemin = 0.0f;
			d.rangemax = 1.0f;
			d.scale = 1.0f;
			d.offx = 0.0f;
			d.offy = 0.0f;
			d.sliceFace = 0;
			d.rawoutput = false;
			d.lightBackgroundColour = d.darkBackgroundColour = 
				FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
			d.Red = d.Green = d.Blue = true;
			d.Alpha = false;

			ReplayOutput_SetTextureDisplay(out, d);

			break;
		}
	}

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while(true)
	{
		// Check to see if any messages are waiting in the queue
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Translate the message and dispatch it to WindowProc()
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// If the message is WM_QUIT, exit the while loop
		if(msg.message == WM_QUIT)
			break;

		ReplayRenderer_SetFrameEvent(renderer, 0, 10000000+rand()%1000);

		ReplayOutput_SetOutputConfig(out, c);

		ReplayOutput_Display(out);

		Sleep(40);
	}
}

int renderdoccmd(int argc, wchar_t **argv);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd )
{
	LPWSTR *argv;
	int argc;

	argv = CommandLineToArgvW(GetCommandLine(), &argc);

	hInstance = hInst;
	
	WNDCLASSEX wc;
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON));
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = L"renderdoccmd";
	wc.hIconSm       = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON));

	if(!RegisterClassEx(&wc))
	{
		return 1;
	}
	
	CrashGenerationServer *crashServer = NULL;

	// special WIN32 option for launching the crash handler
	if(argc == 2 && !_wcsicmp(argv[1], L"--crashhandle"))
	{
		wchar_t tempPath[MAX_PATH] = {0};
		GetTempPathW(MAX_PATH-1, tempPath);

		Sleep(100);

		wstring dumpFolder = tempPath;
		dumpFolder += L"RenderDocDumps";

		CreateDirectoryW(dumpFolder.c_str(), NULL);

		crashServer = new CrashGenerationServer(L"\\\\.\\pipe\\RenderDocBreakpadServer",
												NULL, NULL, NULL, OnClientCrashed, NULL,
												OnClientExited, NULL, NULL, NULL, true,
												&dumpFolder);

		if (!crashServer->Start()) {
			delete crashServer;
			crashServer = NULL;
			return 1;
		}

		CrashHandlerInst = hInstance;
		
		CrashHandlerWnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"renderdoccmd", L"renderdoccmd", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 10, 10,
			NULL, NULL, hInstance, NULL);

		HANDLE hIcon = LoadImage(CrashHandlerInst, MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);

		if(hIcon)
		{
			SendMessage(CrashHandlerWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			SendMessage(CrashHandlerWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		}

		ShowWindow(CrashHandlerWnd, SW_HIDE);
		
		HANDLE readyEvent = CreateEventA(NULL, TRUE, FALSE, "RENDERDOC_CRASHHANDLE");

		if(readyEvent != NULL)
		{
			SetEvent(readyEvent);

			CloseHandle(readyEvent);
		}

		MSG msg;
		ZeroMemory(&msg, sizeof(msg));
		while(!exitServer)
		{
			// Check to see if any messages are waiting in the queue
			while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				// Translate the message and dispatch it to WindowProc()
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			// If the message is WM_QUIT, exit the while loop
			if(msg.message == WM_QUIT)
				break;
			
			Sleep(100);
		}
		
		delete crashServer;
		crashServer = NULL;

		if(!dump.empty())
		{
			logpath = L"";

			string report = "";

			for(size_t i=0; i < customInfo.size(); i++)
			{
				wstring name = customInfo[i].name;
				wstring val = customInfo[i].value;

				if(name == L"logpath")
				{
					logpath = val;
				}
				else if(name == L"ptime")
				{
					// breakpad uptime, ignore.
				}
				else
				{
					report += string(name.begin(), name.end()) + ": " + string(val.begin(), val.end()) + "\n";
				}
			}

			DialogBox(CrashHandlerInst, MAKEINTRESOURCE(IDD_CRASH_HANDLER), CrashHandlerWnd, (DLGPROC)CrashHandlerProc);
			
			report += "\n\nRepro steps/Notes:\n\n" + reproSteps;

			{
				FILE *f = NULL;
				_wfopen_s(&f, logpath.c_str(), L"r");
				if(f)
				{
					fseek(f, 0, SEEK_END);
					long filesize = ftell(f);
					fseek(f, 0, SEEK_SET);

					if(filesize > 10)
					{
						char *error_log = new char[filesize+1];
						memset(error_log, 0, filesize+1);

						fread(error_log, 1, filesize, f);

						char *managed_callstack = strstr(error_log, "--- Begin C# Exception Data ---");
						if(managed_callstack)
						{
							report += managed_callstack;
							report += "\n\n";
						}

						delete[] error_log;
					}
					
					fclose(f);
				}
			}

			if(uploadReport)
			{
				mz_zip_archive zip;
				ZeroMemory(&zip, sizeof(zip));

				wstring destzip = dumpFolder + L"\\report.zip";

				DeleteFileW(destzip.c_str());

				mz_zip_writer_init_wfile(&zip, destzip.c_str(), 0);
				mz_zip_writer_add_mem(&zip, "report.txt", report.c_str(), report.length(), MZ_BEST_COMPRESSION);

				if(uploadDump && !dump.empty())
					mz_zip_writer_add_wfile(&zip, "minidump.dmp", dump.c_str(), NULL, 0, MZ_BEST_COMPRESSION);

				if(uploadLog && !logpath.empty())
					mz_zip_writer_add_wfile(&zip, "error.log", logpath.c_str(), NULL, 0, MZ_BEST_COMPRESSION);

				mz_zip_writer_finalize_archive(&zip);
				mz_zip_writer_end(&zip);

				int timeout = 10000;
				wstring body = L"";
				int code = 0;

				std::map<wstring, wstring> params;

				google_breakpad::HTTPUpload::SendRequest(L"http://renderdoc.org/bugsubmit", params,
					dumpFolder + L"\\report.zip", L"report", &timeout, &body, &code);

				DeleteFileW(destzip.c_str());
			}
		}

		if(!dump.empty())
			DeleteFileW(dump.c_str());

		if(!logpath.empty())
			DeleteFileW(logpath.c_str());

		return 0;
	}

	return renderdoccmd(argc, argv);
}
