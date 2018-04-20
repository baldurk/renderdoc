/*
 *******************************************************************************
 *
 * Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

//=============================================================================
/// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief An ETW trace session that can be enable providers and start a realtime event trace.
//=============================================================================

#pragma once

#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <evntcons.h>
#include <Tdh.h>

/// The baseclass for a realtime ETW consumer.
class ETWConsumerBase
{
public:
    virtual void OnEventRecord(PEVENT_RECORD pEventRecord) = 0;
    virtual ~ETWConsumerBase() {}
};

/// An ETW trace session that can be enable providers and start a realtime event trace.
class TraceSession
{
public:
    TraceSession();
    ~TraceSession();

    bool Start();
    bool EnableProvider(const GUID& providerId, UCHAR level, ULONGLONG anyKeyword = 0, ULONGLONG allKeyword = 0);
    bool EnableProviderByGUID(const LPCWSTR& guid, UCHAR level, ULONGLONG anyKeyword = 0, ULONGLONG allKeyword = 0);
    bool Open(ETWConsumerBase* pConsumer);
    bool Process();
    bool Close();
    bool DisableProvider(const GUID& providerId);
    bool Stop();
    LONGLONG PerfFreq() const;

private:
    TCHAR m_sessionName[128];                       ///< Storage for the ETW session name
    struct
    {
        EVENT_TRACE_PROPERTIES properties;
        char                   name[128];
    } m_session;
    EVENT_TRACE_LOGFILE m_traceLogFile;             ///< The trace logfile to stream data to.
    TRACEHANDLE m_sessionHandle;                    ///< The ETW trace session handle.
    TRACEHANDLE m_traceHandle;                      ///< The handle for the active ETW trace.
};
