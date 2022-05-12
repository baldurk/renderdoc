/*
* Copyright 2014-2022 NVIDIA Corporation.  All rights reserved.
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

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <cassert>
#include "nvperf_host.h"
#include "nvperf_target.h"
#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/time.h>
#endif

#define NV_PERF_UNUSED_VARIABLE(var_) (void)(var_)

namespace nv { namespace perf {

    inline int FormatTimeCommon(char* pBuf, size_t size, uint32_t hour, uint32_t minute, uint32_t second, uint32_t milliSecond)
    {
        const int written = snprintf(pBuf, size, "%02u:%02u:%02u:%03u", hour, minute, second, milliSecond);
        return written;
    }

    inline int FormatDateCommon(char* pBuf, size_t size, uint32_t year, uint32_t month, uint32_t day)
    {
        const char* pMonth = [&](){
            static const char* s_months[12] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };

            if (1 <= month && month <= 12)
            {
                return s_months[month - 1];
            }
            return "???";
        }();
        const int written = snprintf(pBuf, size, "%4u-%s-%02u", year, pMonth, day);
        return written;
    }

#if defined(_WIN32)
    typedef struct _FILETIME LogTimeStamp;

    inline void UserLogImplPlatform(const char* pMessage)
    {
        OutputDebugStringA(pMessage);
    }

    inline void GetTimeStamp(LogTimeStamp* pTimestamp)
    {
        GetSystemTimeAsFileTime(pTimestamp);
    }

    inline size_t FormatTime(LogTimeStamp* pTimestamp, char* pBuf, size_t size)
    {
        SYSTEMTIME utc, stime;
        FileTimeToSystemTime(pTimestamp, &utc);
        SystemTimeToTzSpecificLocalTime(NULL, &utc, &stime);
        return FormatTimeCommon(pBuf, size, (uint32_t)stime.wHour, (uint32_t)stime.wMinute, (uint32_t)stime.wSecond, (uint32_t)stime.wMilliseconds);
    }

    inline size_t FormatDate(LogTimeStamp* pTimestamp, char* pBuf, size_t size)
    {
        SYSTEMTIME utc, stime;
        FileTimeToSystemTime(pTimestamp, &utc);
        SystemTimeToTzSpecificLocalTime(NULL, &utc, &stime);
        return FormatDateCommon(pBuf, size, (uint32_t)stime.wYear, (uint32_t)stime.wMonth, (uint32_t)stime.wDay);
    }

    inline bool GetEnvVariable(const char* pName, std::string& value)
    {
        auto neededSize = ::GetEnvironmentVariableA(pName, nullptr, 0);
        if (!neededSize)
        {
            value = "";
            return false;
        }
        value.resize(neededSize, '\0');
        neededSize = ::GetEnvironmentVariableA(pName, &value[0], neededSize);
        value.resize(value.size() - 1);
        return true;
    }

    inline FILE* OpenFile(const char* pFileName, const char* pMode)
    {
        FILE* pFile = nullptr;
        fopen_s(&pFile, pFileName, pMode);
        return pFile;
    }
#else // !defined(_WIN32)
    typedef struct timeval LogTimeStamp;

    inline void UserLogImplPlatform(const char* pMessage)
    {
        (void*)pMessage;
    }

    inline void GetTimeStamp(LogTimeStamp* pTimestamp)
    {
        gettimeofday(pTimestamp, 0);
    }

    inline size_t FormatTime(LogTimeStamp* pTimestamp, char* pBuf, size_t size)
    {
        const struct tm* ltm = localtime(&pTimestamp->tv_sec);
        int milliseconds = pTimestamp->tv_usec / 1000;
        return FormatTimeCommon(pBuf, size, (uint32_t)ltm->tm_hour, (uint32_t)ltm->tm_min, (uint32_t)ltm->tm_sec, (uint32_t)milliseconds);
    }

    inline size_t FormatDate(LogTimeStamp* pTimestamp, char* pBuf, size_t size)
    {
        const struct tm* ltm = localtime(&pTimestamp->tv_sec);
        return FormatDateCommon(pBuf, size, (uint32_t)ltm->tm_year + 1900, (uint32_t)ltm->tm_mon + 1, (uint32_t)ltm->tm_mday);
    }

    inline bool GetEnvVariable(const char* pName, std::string& value)
    {
        auto pValue = std::getenv(pName);
        if (!pValue)
        {
            value = "";
            return false;
        }
        value = pValue;
        return true;
    }

    inline FILE* OpenFile(const char* pFileName, const char* pMode)
    {
        FILE* pFile = std::fopen(pFileName, pMode);
        return pFile;
    }
#endif // defined(_WIN32)

}}

#ifndef NV_PERF_LOG_INF
#define NV_PERF_LOG_INF(level_, ...) ::nv::perf::UserLog(LogSeverity::Inf, level_, __FUNCTION__, __VA_ARGS__)
#endif

#ifndef NV_PERF_LOG_WRN
#define NV_PERF_LOG_WRN(level_, ...) ::nv::perf::UserLog(LogSeverity::Wrn, level_, __FUNCTION__, __VA_ARGS__)
#endif

#ifndef NV_PERF_LOG_ERR
#define NV_PERF_LOG_ERR(level_, ...) ::nv::perf::UserLog(LogSeverity::Err, level_, __FUNCTION__, __VA_ARGS__)
#endif

namespace nv { namespace perf {

    enum class LogSeverity
    {
        Inf,
        Wrn,
        Err,
        COUNT
    };

    struct LogSettings
    {
        uint32_t volumeLevels[(unsigned)LogSeverity::COUNT] = { 50, 50, 50 };

#if defined(_WIN32)
        bool writePlatform = true;
#else
        bool writePlatform = false;
#endif
        bool writeStderr                        = true;
        FILE* writeFileFD                       = nullptr;
        bool appendToFile                       = true;
        LogSeverity flushFileSeverity           = LogSeverity::Err;

        bool logDate                            = true;
        bool logTime                            = true;

        LogSettings()
        {
#if defined(_WIN32)
            {
                std::string envValue;
                if (GetEnvVariable("NV_PERF_LOG_ENABLE_PLATFORM", envValue))
                {
                    char* pEnd = nullptr;
                    writePlatform = !!strtol(envValue.c_str(), &pEnd, 0);
                }
            }
#endif
            {
                std::string envValue;
                if (GetEnvVariable("NV_PERF_LOG_ENABLE_STDERR", envValue))
                {
                    char* pEnd = nullptr;
                    writeStderr = !!strtol(envValue.c_str(), &pEnd, 0);
                }
            }
            {
                std::string envValue;
                if (GetEnvVariable("NV_PERF_LOG_ENABLE_FILE", envValue))
                {
                    FILE* fp = OpenFile(envValue.c_str(), appendToFile ? "a" : "w");
                    assert(fp);
                    writeFileFD = fp;
                }
            }
            {
                std::string envValue;
                if (GetEnvVariable("NV_PERF_LOG_FILE_FLUSH_SEVERITY", envValue))
                {
                    char* pEnd = nullptr;
                    int severity = strtol(envValue.c_str(), &pEnd, 0);
                    if (0 <= severity && severity < (int)LogSeverity::COUNT)
                    {
                        flushFileSeverity = (LogSeverity)severity;
                    }
                }
            }
        }

        ~LogSettings()
        {
            if (writeFileFD)
            {
                fclose(writeFileFD);
            }
        }
    };


    inline LogSettings* GetLogSettingsStorage_()
    {
        static LogSettings settings;
        return &settings;
    }

    inline uint32_t GetLogVolumeLevel(LogSeverity severity)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        if ((uint32_t)severity < 3)
        {
            return pSettings->volumeLevels[(uint32_t)severity];
        }
        return 0;
    }

    // Higher values produce more log output.  0 <= volumeLevel <= 100
    // Technically it's more like a noise floor (all messages below this level are treated as noise and discarded).
    inline void SetLogVolumeLevel(LogSeverity severity, uint32_t volumeLevel)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        if ((uint32_t)severity < 3)
        {
            pSettings->volumeLevels[(uint32_t)severity] = volumeLevel;
        }
    }

    inline void SetLogAppendToFile(bool enable)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        pSettings->appendToFile = enable;
    }

    inline void SetLogFlushSeverity(LogSeverity severity)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        if (0 <= (int)severity && (int)severity < (int)LogSeverity::COUNT)
        {
            pSettings->flushFileSeverity = severity;
        }
    }

    inline void SetLogDate(bool enable)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        pSettings->logDate = enable;
    }

    inline void SetLogTime(bool enable)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        pSettings->logTime = enable;
    }

    inline bool UserLogEnablePlatform(bool enable)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        pSettings->writePlatform = enable;
        return true;
    }

    inline bool UserLogEnableStderr(bool enable)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        pSettings->writeStderr = enable;
        return true;
    }

    inline bool UserLogEnableFile(const char* filename)
    {
        LogSettings* pSettings = GetLogSettingsStorage_();
        if (filename)
        {
            FILE* fp = OpenFile(filename, pSettings->appendToFile ? "a" : "w");
            if (!fp)
            {
                return false;
            }
            pSettings->writeFileFD = fp;
        }
        return true;
    }

    inline void UserLogImplStderr(const char* pMessage) 
    {
        fprintf(stderr, "%s", pMessage);
    }

    inline void UserLogImplFile(const char* pMessage, FILE* fd)
    {
        fprintf(fd, "%s", pMessage);
    }

    inline void UserLogImplFileFlush(FILE* fd)
    {
        fflush(fd);
    }

    inline void UserLog(LogSeverity severity, uint32_t level, const char* pFunctionName, const char* pFormat, ...)
    {
        const uint32_t volumeLevel = GetLogVolumeLevel(severity);
        if (volumeLevel < level)
        {
            return;
        }

        LogSettings& settings = *GetLogSettingsStorage_();

        va_list args;

        va_start(args, pFormat);
        const size_t length = vsnprintf(nullptr, 0, pFormat, args);
        va_end(args);

        std::string str;
        str.append(length + 1, ' ');
        va_start(args, pFormat);
        vsnprintf(&str[0], length+1, pFormat, args);
        va_end(args);
        str.back() = '\0'; // ensure NULL terminated

        const char* const pPrefix = [&]() {
            switch (severity)
            {
                case (LogSeverity::Inf): return "NVPERF|INF|";
                case (LogSeverity::Wrn): return "NVPERF|WRN|";
                case (LogSeverity::Err): return "NVPERF|ERR|";
                default:                 return "NVPERF|???|";
            }
        }();

        char datebuf[16];
        char timebuf[16];
        if (settings.logDate || settings.logTime)
        {
            LogTimeStamp time;
            GetTimeStamp(&time);
            if (settings.logDate)
            {
                FormatDate(&time, datebuf, sizeof(datebuf));
            }
            if (settings.logTime)
            {
                FormatTime(&time, timebuf, sizeof(timebuf));
            }
        }

        if (settings.writePlatform)
        {
            UserLogImplPlatform(pPrefix);
            if (settings.logDate)
            {
                UserLogImplPlatform(datebuf);
                UserLogImplPlatform("|");
            }
            if (settings.logTime)
            {
                UserLogImplPlatform(timebuf);
                UserLogImplPlatform("|");
            }
            UserLogImplPlatform(pFunctionName);
            UserLogImplPlatform(" || ");
            UserLogImplPlatform(str.c_str());
        }
        if (settings.writeStderr)
        {
            UserLogImplStderr(pPrefix);
            if (settings.logDate)
            {
                UserLogImplStderr(datebuf);
                UserLogImplStderr("|");
            }
            if (settings.logTime)
            {
                UserLogImplStderr(timebuf);
                UserLogImplStderr("|");
            }
            UserLogImplStderr(pFunctionName);
            UserLogImplStderr(" || ");
            UserLogImplStderr(str.c_str());
        }
        if (settings.writeFileFD)
        {
            UserLogImplFile(pPrefix, settings.writeFileFD);
            if (settings.logDate)
            {
                UserLogImplFile(datebuf, settings.writeFileFD);
                UserLogImplFile("|", settings.writeFileFD);
            }
            if (settings.logTime)
            {
                UserLogImplFile(timebuf, settings.writeFileFD);
                UserLogImplFile("|", settings.writeFileFD);
            }
            UserLogImplFile(pFunctionName, settings.writeFileFD);
            UserLogImplFile(" || ", settings.writeFileFD);
            UserLogImplFile(str.c_str(), settings.writeFileFD);
            if (severity >= settings.flushFileSeverity)
            {
                UserLogImplFileFlush(settings.writeFileFD);
            }
        }
    }

    inline bool InitializeNvPerf()
    {
        NVPA_Status nvpaStatus;

        NVPW_InitializeHost_Params initializeHostParams = { NVPW_InitializeHost_Params_STRUCT_SIZE };
        nvpaStatus = NVPW_InitializeHost(&initializeHostParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_InitalizeHost failed\n");
            return false;
        }

        NVPW_InitializeTarget_Params initializeTargetParams = { NVPW_InitializeTarget_Params_STRUCT_SIZE };
        nvpaStatus = NVPW_InitializeTarget(&initializeTargetParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_InitializeTarget failed\n");
            return false;
        }

        return true;
    }

}}
