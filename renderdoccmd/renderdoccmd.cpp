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

#include <string>

#include <replay/renderdoc_replay.h>
#include <app/renderdoc_app.h>

using std::string;
using std::wstring;

uint32_t wtoi(wchar_t *str)
{
	uint32_t ret = 0;

	while(str && *str)
	{
		if(*str > L'9' || *str < L'0')
			break;

		uint32_t digit = uint32_t(*str-L'0');

		ret *= 10;
		ret += digit;

		str++;
	}

	return ret;
}

bool argequal(const wchar_t *a, const wchar_t *b)
{
	if(a == NULL || b == NULL) return false;

	while(*a && *b)
	{
		if(towlower(*a) != towlower(*b))
			break;

		a++;
		b++;
	}

	// if both reached null terminator then strings are equal
	return *a == 0 && *b == 0;
}

// defined in *_specific.cpp
void DisplayRendererPreview(ReplayRenderer *renderer);
wstring GetUsername();

int renderdoccmd(int argc, wchar_t **argv)
{
	CaptureOptions opts;
	opts.AllowFullscreen = false;
	opts.AllowVSync = false;
	opts.DelayForDebugger = 5;
	opts.HookIntoChildren = true;

	if(argc >= 2)
	{
		// fall through and print usage
		if(argequal(argv[1], L"--help") || argequal(argv[1], L"-h"))
		{
		}
		// if we were given a logfile, load it and continually replay it.
		else if(wcsstr(argv[1], L".rdc") != NULL)
		{
			float progress = 0.0f;
			ReplayRenderer *renderer = NULL;
			auto status = RENDERDOC_CreateReplayRenderer(argv[1], &progress, &renderer);

			if(renderer && status == eReplayCreate_Success)
				DisplayRendererPreview(renderer);

			ReplayRenderer_Shutdown(renderer);
			return 0;
		}
		// replay a logfile
		else if(argequal(argv[1], L"--replay") || argequal(argv[1], L"-r"))
		{
			if(argc >= 3)
			{
				float progress = 0.0f;
				ReplayRenderer *renderer = NULL;
				auto status = RENDERDOC_CreateReplayRenderer(argv[2], &progress, &renderer);

				if(renderer && status == eReplayCreate_Success)
					DisplayRendererPreview(renderer);

				ReplayRenderer_Shutdown(renderer);
				return 0;
			}
			else
			{
				fprintf(stderr, "Not enough parameters to --replay");
			}
		}
#ifdef WIN32
		// if we were given an executable on windows, inject into it
		// can't do this on other platforms as there's no nice extension
		// and we don't want to just launch any single parameter in case it's
		// -h or -help or some other guess/typo
		else if(wcsstr(argv[1], L".exe") != NULL)
		{
			uint32_t ident = RENDERDOC_ExecuteAndInject(argv[1], NULL, NULL, NULL, &opts, false);

			if(ident == 0)
				fprintf(stderr, "Failed to create & inject\n");
			else
				fprintf(stderr, "Created & injected as %d\n", ident);

			return ident;
		}
#endif
		// capture a program with default capture options
		else if(argequal(argv[1], L"--capture") || argequal(argv[1], L"-c"))
		{
			if(argc >= 3)
			{
				uint32_t ident = RENDERDOC_ExecuteAndInject(argv[2], NULL, NULL, NULL, &opts, false);

				if(ident == 0)
					fprintf(stderr, "Failed to create & inject to '%ls'\n", argv[2]);
				else
					fprintf(stderr, "Created & injected '%ls' as %d\n", argv[2], ident);

				return ident;
			}
			else
			{
				fprintf(stderr, "Not enough parameters to --capture");
			}
		}
		// inject into a running process with default capture options
		else if(argequal(argv[1], L"--inject") || argequal(argv[1], L"-i"))
		{
			if(argc >= 3)
			{
				wchar_t *pid = argv[2];
				while(*pid == L'"' || iswspace(*pid)) pid++;

				uint32_t pidNum = (uint32_t)wtoi(pid);

				uint32_t ident = RENDERDOC_InjectIntoProcess(pidNum, NULL, &opts, false);

				if(ident == 0)
					printf("Failed to inject to %u\n", pidNum);
				else
					printf("Injected to %u as %u\n", pidNum, ident);

				return ident;
			}
			else
			{
				fprintf(stderr, "Not enough parameters to --inject");
			}
		}
		// spawn remote replay host
		else if(argequal(argv[1], L"--replayhost") || argequal(argv[1], L"-rh"))
		{
			RENDERDOC_SpawnReplayHost(NULL);
			return 1;
		}
		// replay a logfile over the network on a remote host
		else if(argequal(argv[1], L"--remotereplay") || argequal(argv[1], L"-rr"))
		{
			if(argc >= 4)
			{
				RemoteRenderer *remote = NULL;
				auto status = RENDERDOC_CreateRemoteReplayConnection(argv[2], &remote);

				if(remote == NULL || status != eReplayCreate_Success)
					return 1;

				float progress = 0.0f;

				ReplayRenderer *renderer = NULL;
				status = RemoteRenderer_CreateProxyRenderer(remote, 0, argv[3], &progress, &renderer);

				if(renderer && status == eReplayCreate_Success)
					DisplayRendererPreview(renderer);

				RemoteRenderer_Shutdown(remote);
				return 0;
			}
			else
			{
				fprintf(stderr, "Not enough parameters to --remotereplay");
			}
		}
		// not documented/useful for manual use on the cmd line, used internally
		else if(argequal(argv[1], L"--cap32for64"))
		{
			if(argc >= 5)
			{
				wchar_t *pid = argv[2];
				while(*pid == L'"' || iswspace(*pid)) pid++;

				uint32_t pidNum = (uint32_t)wtoi(pid);

				wchar_t *log = argv[3];

				CaptureOptions cmdopts;

				string optstring(&argv[4][0], &argv[4][0] + wcslen(argv[4]));

				cmdopts.FromString(optstring);

				return RENDERDOC_InjectIntoProcess(pidNum, log, &cmdopts, false);
			}
			else
			{
				fprintf(stderr, "Not enough parameters to --cap32for64");
			}
		}
	}
	
	fprintf(stderr, "renderdoccmd usage:\n\n");
	fprintf(stderr, "  <file.rdc>                        Launch a preview window that replays this logfile and\n");
	fprintf(stderr, "                                    displays the backbuffer.\n");
#ifdef WIN32
	fprintf(stderr, "  <program.exe>                     Launch a capture of this program with default options.\n");
#endif
	fprintf(stderr, "\n");												      
	fprintf(stderr, "  -h,  --help                       Displays this help message.\n");
	fprintf(stderr, "  -c,  --capture PROGRAM            Launches capture of the program with default options.\n");
	fprintf(stderr, "  -i,  --inject PID                 Injects into the specified PID to capture.\n");
	fprintf(stderr, "  -r,  --replay LOGFILE             Launch a preview window that replays this logfile and\n");
	fprintf(stderr, "                                    displays the backbuffer.\n");
	fprintf(stderr, "  -rh, --replayhost                 Starts a replay host server that can be used to remotely\n");
	fprintf(stderr, "                                    replay logfiles from another machine.\n");
	fprintf(stderr, "  -rr, --remotereplay HOST LOGFILE  Launch a replay of the logfile and display a preview\n");
	fprintf(stderr, "                                    window. Use the remote host to replay all commands.\n");

	return 1;
}
