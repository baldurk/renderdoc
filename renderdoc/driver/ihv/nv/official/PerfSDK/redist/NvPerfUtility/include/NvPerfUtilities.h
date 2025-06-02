/*
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once
#include <string>
#include <cstdlib>
#include <fstream>
#ifdef _WIN32
#define NV_PERF_PATH_SEPARATOR '\\'
#include <windows.h>
#else
#define NV_PERF_PATH_SEPARATOR '/'
#include <unistd.h>
#include <limits.h>
#endif // _WIN32

namespace nv { namespace perf { namespace utilities {

    inline bool FileExists(const std::string& name)
    {
        std::ifstream f(name.c_str());
        return f.good();
    }

    inline std::string ExtractDirectoryPathOutOfFilePath(const std::string& filePath)
    {
        const size_t pos = filePath.find_last_of("/\\");
        if (pos == std::string::npos)
        {
            return "";
        }
        return filePath.substr(0, pos);
    }

    inline std::string GetCurrentExecutableDirectory()
    {
        std::string filePath;
#ifdef WIN32
        char pathBuffer[MAX_PATH] = {};
        DWORD ret = GetModuleFileNameA(NULL, pathBuffer, MAX_PATH);
        if (ret == 0 || ret == MAX_PATH)
        {
            return std::string();
        }
        filePath = std::string(pathBuffer);
#else
        char pathBuffer[PATH_MAX] = {};
        const ssize_t count = readlink("/proc/self/exe", pathBuffer, PATH_MAX);
        if (count == -1)
        {
            return std::string();
        }
        filePath = std::string(pathBuffer, count);
#endif

        return ExtractDirectoryPathOutOfFilePath(filePath);
    }

    inline std::string JoinDriectoryAndFileName(const std::string& directory, const std::string& fileName)
    {
        if (directory.empty())
        {
            return fileName;
        }

        if (directory.back() == NV_PERF_PATH_SEPARATOR)
        {
            return directory + fileName;
        }
        return directory + NV_PERF_PATH_SEPARATOR + fileName;
    }

}}} // namespace nv::perf::tool