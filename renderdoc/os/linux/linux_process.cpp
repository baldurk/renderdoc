/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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


#include "os/os_specific.h"
#include "api/app/renderdoc_app.h"
#include "api/replay/capture_options.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>

#include "serialise/string_utils.h"

static vector<Process::EnvironmentModification> &GetEnvModifications()
{
	static vector<Process::EnvironmentModification> envCallbacks;
	return envCallbacks;
}

static map<string, string> EnvStringToEnvMap(const char **envstring)
{
	map<string, string> ret;

	const char **e = envstring;

	while(*e)
	{
		const char *equals = strchr(*e, '=');

		string name;
		string value;
		
		name.assign(*e, equals);
		value = equals+1;

		ret[name] = value;

		e++;
	}

	return ret;
}

void Process::RegisterEnvironmentModification(Process::EnvironmentModification modif)
{
	GetEnvModifications().push_back(modif);
}

// on linux we apply environment changes before launching the program, as
// there is no support for injecting/loading renderdoc into a running program
// in any way, and we also have some environment changes that we *have* to make
// for correct hooking (LD_LIBRARY_PATH/LD_PRELOAD)
//
// However we still set environment variables so that we can modify variables while
// in process (e.g. if we notice a setting and want to enable an env var as a result)
void Process::ApplyEnvironmentModification()
{
	// turn environment string to a UTF-8 map
	map<string, string> currentEnv = EnvStringToEnvMap((const char **)environ);
	vector<EnvironmentModification> &modifications = GetEnvModifications();

	for(size_t i=0; i < modifications.size(); i++)
	{
		EnvironmentModification &m = modifications[i];

		string value = currentEnv[m.name];

		switch(m.type)
		{
			case eEnvModification_Replace:
				value = m.value;
				break;
			case eEnvModification_Append:
				value += m.value;
				break;
			case eEnvModification_AppendPlatform:
			case eEnvModification_AppendColon:
				if(!value.empty())
					value += ":";
				value += m.value;
				break;
			case eEnvModification_AppendSemiColon:
				if(!value.empty())
					value += ";";
				value += m.value;
				break;
			case eEnvModification_Prepend:
				value = m.value + value;
				break;
			case eEnvModification_PrependColon:
				if(!value.empty())
					value = m.value + ":" + value;
				else
					value = m.value;
				break;
			case eEnvModification_PrependPlatform:
			case eEnvModification_PrependSemiColon:
				if(!value.empty())
					value = m.value + ";" + value;
				else
					value = m.value;
				break;
			default:
				RDCERR("Unexpected environment modification type");
		}

		setenv(m.name.c_str(), value.c_str(), true);
	}

	// these have been applied to the current process
	modifications.clear();
}

static pid_t RunProcess(const char *app, const char *workingDir, const char *cmdLine, char *const *envp)
{
	if(!app) return (pid_t)0;

	// it is safe to use app directly as execve never modifies argv
	char *emptyargv[] = { (char *) app, NULL };
	char **argv = emptyargv;

	const char *c = cmdLine;

	// parse command line into argv[], similar to how bash would
	if(cmdLine)
	{
		int argc = 1;

		// get a rough upper bound on the number of arguments
		while(*c)
		{
			if(*c == ' ' || *c == '\t') argc++;
			c++;
		}

		argv = new char*[argc+2];

		c = cmdLine;

		string a;

		argc = 0; // current argument we're fetching

		// argv[0] is the application name, by convention
		size_t len = strlen(app)+1;
		argv[argc] = new char[len];
		strcpy(argv[argc], app);

		argc++;

		bool dquot = false, squot = false; // are we inside ''s or ""s
		while(*c)
		{
			if(!dquot && !squot && (*c == ' ' || *c == '\t'))
			{
				if(!a.empty())
				{
					// if we've fetched some number of non-space characters
					argv[argc] = new char[a.length()+1];
					memcpy(argv[argc], a.c_str(), a.length()+1);
					argc++;
				}

				a = "";
			}
			else if(!dquot && *c == '"')
			{
				dquot = true;
			}
			else if(!squot && *c == '\'')
			{
				squot = true;
			}
			else if(dquot && *c == '"')
			{
				dquot = false;
			}
			else if(squot && *c == '\'')
			{
				squot = false;
			}
			else if(squot)
			{
				// single quotes don't escape, just copy literally until we leave single quote mode
				a.push_back(*c);
			}
			else if(dquot)
			{
				// handle escaping
				if(*c == '\\')
				{
					c++;
					if(*c)
					{
						a.push_back(*c);
					}
					else
					{
						RDCERR("Malformed command line:\n%s", cmdLine);
						return 0;
					}
				}
				else
				{
					a.push_back(*c);
				}
			}
			else
			{
				a.push_back(*c);
			}

			c++;
		}

		if(!a.empty())
		{
			// if we've fetched some number of non-space characters
			argv[argc] = new char[a.length()+1];
			memcpy(argv[argc], a.c_str(), a.length()+1);
			argc++;
		}

		argv[argc] = NULL;

		if(squot || dquot)
		{
			RDCERR("Malformed command line\n%s", cmdLine);
			return 0;
		}
	}

	pid_t childPid = fork();
	if(childPid == 0)
	{
		if(workingDir)
		{
			chdir(workingDir);
		}
		else
		{
			string exedir = app;
			chdir(dirname(exedir).c_str());
		}

		execve(app, argv, envp);
		RDCERR("Failed to execute %s: %s", app, strerror(errno));
		exit(0);
	}
	
	char **argv_delete = argv;

	if(argv != emptyargv)
	{
		while(*argv)
		{
			delete[] *argv;
			argv++;
		}

		delete[] argv_delete;
	}

	return childPid;
}

uint32_t Process::InjectIntoProcess(uint32_t pid, const char *logfile, const CaptureOptions *opts, bool waitForExit)
{
	RDCUNIMPLEMENTED("Injecting into already running processes on linux");
	return 0;
}

uint32_t Process::LaunchProcess(const char *app, const char *workingDir, const char *cmdLine)
{
	if(app == NULL || app[0] == 0)
	{
		RDCERR("Invalid empty 'app'");
		return 0;
	}

	return (uint32_t)RunProcess(app, workingDir, cmdLine, environ);
}

uint32_t Process::LaunchAndInjectIntoProcess(const char *app, const char *workingDir, const char *cmdLine,
                                             const char *logfile, const CaptureOptions *opts, bool waitForExit)
{
	if(app == NULL || app[0] == 0)
	{
		RDCERR("Invalid empty 'app'");
		return 0;
	}
	
	// turn environment string to a UTF-8 map
	map<string, string> env = EnvStringToEnvMap((const char **)environ);
	vector<EnvironmentModification> &modifications = GetEnvModifications();
	
	if (logfile == NULL)
		logfile = "";

	string libpath;
	{
		FileIO::GetExecutableFilename(libpath);
		libpath = dirname(libpath);
	}

	string optstr;
	{
		optstr.reserve(sizeof(CaptureOptions)*2+1);
		byte *b = (byte *)opts;
		for(size_t i=0; i < sizeof(CaptureOptions); i++)
		{
			optstr.push_back(char( 'a' + ((b[i] >> 4)&0xf) ));
			optstr.push_back(char( 'a' + ((b[i]     )&0xf) ));
		}
	}

	modifications.push_back(EnvironmentModification(eEnvModification_AppendPlatform, "LD_LIBRARY_PATH", libpath.c_str()));
	modifications.push_back(EnvironmentModification(eEnvModification_AppendPlatform, "LD_PRELOAD", "librenderdoc.so"));
	modifications.push_back(EnvironmentModification(eEnvModification_Replace, "RENDERDOC_LOGFILE", logfile));
	modifications.push_back(EnvironmentModification(eEnvModification_Replace, "RENDERDOC_CAPTUREOPTS", optstr.c_str()));

	for(size_t i=0; i < modifications.size(); i++)
	{
		EnvironmentModification &m = modifications[i];

		string &value = env[m.name];

		switch(m.type)
		{
			case eEnvModification_Replace:
				value = m.value;
				break;
			case eEnvModification_Append:
				value += m.value;
				break;
			case eEnvModification_AppendPlatform:
			case eEnvModification_AppendColon:
				if(!value.empty())
					value += ":";
				value += m.value;
				break;
			case eEnvModification_AppendSemiColon:
				if(!value.empty())
					value += ";";
				value += m.value;
				break;
			case eEnvModification_Prepend:
				value = m.value + value;
				break;
			case eEnvModification_PrependColon:
				if(!value.empty())
					value = m.value + ":" + value;
				else
					value = m.value;
				break;
			case eEnvModification_PrependPlatform:
			case eEnvModification_PrependSemiColon:
				if(!value.empty())
					value = m.value + ";" + value;
				else
					value = m.value;
				break;
			default:
				RDCERR("Unexpected environment modification type");
		}
	}

	char **envp = new char *[env.size()+1];
	envp[env.size()] = NULL;

	int i=0;
	for(auto it=env.begin(); it != env.end(); it++)
	{
		string envline = it->first + "=" + it->second;
		envp[i] = new char[envline.size()+1];
		memcpy(envp[i], envline.c_str(), envline.size()+1);
		i++;
	}

	pid_t childPid = RunProcess(app, workingDir, cmdLine, envp);

	int ret = 0;

	if(childPid != (pid_t)0)
	{
		// wait for child to have /proc/<pid> and read out tcp socket
		usleep(1000);

		string procfile = StringFormat::Fmt("/proc/%d/net/tcp", (int)childPid);

		// try for a little while for the /proc entry to appear
		for(int retry=0; retry < 10; retry++)
		{
			// back-off for each retry
			usleep(1000 + 500 * retry);

			FILE *f =	FileIO::fopen(procfile.c_str(), "r");

			if(f == NULL)
			{
				// try again in a bit
				continue;
			}

			// read through the proc file to check for an open listen socket
			while(ret == 0 && !feof(f))
			{
				const size_t sz = 512;
				char line[sz];line[sz-1] = 0;
				fgets(line, sz-1, f);

				int socketnum = 0, hexip = 0, hexport = 0;
				int num = sscanf(line, " %d: %x:%x", &socketnum, &hexip, &hexport);

				// find open listen socket on 0.0.0.0:port
				if(num == 3 && hexip == 0 &&
						hexport >= RenderDoc_FirstCaptureNetworkPort &&
						hexport <= RenderDoc_LastCaptureNetworkPort)
				{
					ret = hexport;
				}
			}

			FileIO::fclose(f);
		}

		if(waitForExit)
		{
			int dummy = 0;
			waitpid(childPid, &dummy, 0);
		}
	}

	char **envp_delete = envp;

	while(*envp)
	{
		delete[] *envp;
		envp++;
	}

	delete[] envp_delete;

	return ret;
}

void Process::StartGlobalHook(const char *pathmatch, const char *logfile, const CaptureOptions *opts)
{
	RDCUNIMPLEMENTED("Global hooking of all processes on linux");
}

void *Process::LoadModule(const char *module)
{
	return dlopen(module, RTLD_NOW);
}

void *Process::GetFunctionAddress(void *module, const char *function)
{
	if(module == NULL) return NULL;

	return dlsym(module, function);
}

uint32_t Process::GetCurrentPID()
{
	return (uint32_t)getpid();
}
