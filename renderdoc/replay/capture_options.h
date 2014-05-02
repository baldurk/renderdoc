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


#pragma once

#include <string>
#include <sstream> // istringstream/ostringstream used to avoid other dependencies

typedef uint8_t byte;
typedef uint32_t bool32;

// We give every resource a globally unique ID so that we can differentiate
// between two textures allocated in the same memory (after the first is freed)
//
// it's a struct around a uint64_t to aid in template selection
struct ResourceId
{
	ResourceId() : id() {}
	ResourceId(uint64_t val, bool) { id = val; }

	bool operator ==(const ResourceId u) const
	{
		return id == u.id;
	}

	bool operator !=(const ResourceId u) const
	{
		return id != u.id;
	}

	bool operator <(const ResourceId u) const
	{
		return id < u.id;
	}

	uint64_t id;
};

struct CaptureOptions
{
	CaptureOptions()
		: AllowVSync(true),
		  AllowFullscreen(true),
		  DebugDeviceMode(false),
		  CaptureCallstacks(false),
		  CaptureCallstacksOnlyDraws(false),
		  DelayForDebugger(0),
		  CacheStateObjects(true),
		  HookIntoChildren(false),
		  RefAllResources(false),
		  SaveAllInitials(false),
		  CaptureAllCmdLists(false)
	{}

	bool32 AllowVSync;
	bool32 AllowFullscreen;
	bool32 DebugDeviceMode;
	bool32 CaptureCallstacks;
	bool32 CaptureCallstacksOnlyDraws;
	uint32_t DelayForDebugger;
	bool32 CacheStateObjects;
	bool32 HookIntoChildren;
	bool32 RefAllResources;
	bool32 SaveAllInitials;
	bool32 CaptureAllCmdLists;
	
#ifdef __cplusplus
	void FromString(std::string str)
	{
		std::istringstream iss(str);

		iss >> AllowFullscreen
				>> AllowVSync
				>> DebugDeviceMode
				>> CaptureCallstacks
				>> CaptureCallstacksOnlyDraws
				>> DelayForDebugger
				>> CacheStateObjects
				>> HookIntoChildren
				>> RefAllResources
				>> SaveAllInitials
				>> CaptureAllCmdLists;
	}

	std::string ToString() const
	{
		std::ostringstream oss;

		oss << AllowFullscreen << " "
				<< AllowVSync << " "
				<< DebugDeviceMode << " "
				<< CaptureCallstacks << " "
				<< CaptureCallstacksOnlyDraws << " "
				<< DelayForDebugger << " "
				<< CacheStateObjects << " "
				<< HookIntoChildren << " "
				<< RefAllResources << " "
				<< SaveAllInitials << " "
				<< CaptureAllCmdLists << " ";

		return oss.str();
	}
#endif
};


