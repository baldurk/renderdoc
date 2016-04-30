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


#include "common/common.h"
#include "hooks.h"

LibraryHooks &LibraryHooks::GetInstance()
{
	static LibraryHooks instance;
	return instance;
}

void LibraryHooks::RegisterHook(const char *libName, LibraryHook *hook)
{
	m_Hooks[libName] = hook;
}

void LibraryHooks::CreateHooks()
{
	HOOKS_BEGIN();
	for(auto it=m_Hooks.begin(); it!=m_Hooks.end(); ++it)
	{
		RDCDEBUG("Attempting to hook %s", it->first);

		if(it->second->CreateHooks(it->first))
		{
			RDCLOG("Loaded and hooked into %s, PID %d", it->first, Process::GetCurrentPID());
		}
		else
		{
			RDCWARN("Couldn't hook into %s", it->first);
		}
	}
	HOOKS_END();
}

void LibraryHooks::RemoveHooks()
{
	if(m_HooksRemoved) return;
	m_HooksRemoved = true;
	HOOKS_REMOVE();
}

void LibraryHooks::EnableHooks(bool enable)
{
	RDCDEBUG("%s hooks!", enable ? "Enabling" : "Disabling");
	
	for(auto it=m_Hooks.begin(); it!=m_Hooks.end(); ++it)
		it->second->EnableHooks(it->first, enable);
}

void LibraryHooks::OptionsUpdated()
{
	for(auto it=m_Hooks.begin(); it!=m_Hooks.end(); ++it)
		it->second->OptionsUpdated(it->first);
}