//=============================================================================
/// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  a class to provide service to initialize dev driver protocols to
///          capture rgp trace from within the profiled application.
//=============================================================================
#include <ctime>
#include <string>
#include <iostream>
#include <fstream>
#include "RGPClientInProcessModel.h"

#include <devDriverServer.h>
#include <devDriverClient.h>

#include <ddTransferManager.h>

#include "../../Common/DriverToolsDefinitions.h"

#include <protocols/settingsClient.h>
#include <protocols/driverControlClient.h>
#include <protocols/loggingClient.h>
#include <protocols/rgpClient.h>
#include <protocols/etwClient.h>
#include <protocols/ddURIClient.h>
#include <protocols/ddGpuCrashDumpClient.h>

#include <protocols/settingsServer.h>
#include <protocols/driverControlServer.h>
#include <protocols/loggingServer.h>
#include <protocols/rgpServer.h>
#include <protocols/etwServer.h>
#include <protocols/ddURIService.h>
#include <protocols/ddGpuCrashDumpServer.h>

#ifdef _LINUX
int sprintf_s(char* _DstBuf, size_t _SizeInBytes, const char* _Format, ...)
{
    int retVal;
    va_list arg_ptr;

    va_start(arg_ptr, _Format);
    retVal = vsnprintf(_DstBuf, _SizeInBytes, _Format, arg_ptr);
    va_end(arg_ptr);
    return retVal;
}

char* strncpy_s(char* _DstBuf, const char* _SrcBuf, size_t num)
{
    return strncpy(_DstBuf, _SrcBuf, num);
}
#endif

void* GenericAlloc(void* pUserdata, size_t size, size_t alignment, bool zero)
{
    DD_UNUSED(pUserdata);

    return DevDriver::Platform::AllocateMemory(size, alignment, zero);
}

void GenericFree(void* pUserdata, void* pMemory)
{
    DD_UNUSED(pUserdata);

    DevDriver::Platform::FreeMemory(pMemory);
}

DevDriver::AllocCb GenericAllocCb =
{
    nullptr,
    &GenericAlloc,
    &GenericFree
};

void DbgMsg(const std::string& strMsg)
{
#ifdef _DEBUG
    std::cout << strMsg << std::endl;
    char buf[1024];
    sprintf_s(buf, 1024, "%s\n", strMsg.c_str());
 #ifdef _LINUX
    printf(buf);
 #else
    OutputDebugStringA(buf);
 #endif
#else
    DD_UNUSED(strMsg);
#endif // def _DEBUG
}

// Worker thread states
enum WorkerThreadState
{
    STATE_INIT,
    STATE_CAPTURING,
    STATE_IDLE,
    STATE_FINISHED,
    STATE_DONE,
};

// The global worker thread state
static std::atomic<WorkerThreadState> g_workerState(STATE_INIT);

// The global worker thread mutex
static std::mutex g_workerThreadMutex;

RGPClientInProcessModel::RGPClientInProcessModel() :
    m_pClient(nullptr),
    m_profileCaptured(false),
    m_finished(false),
    m_requestingShutdown(false),
    m_beginTag(0),
    m_endTag(0)
{
    m_beginMarker.clear();
    m_endMarker.clear();
    m_threadContext.m_pContext = nullptr;
    m_threadContext.m_pClient = nullptr;
}

RGPClientInProcessModel::~RGPClientInProcessModel()
{
    if (m_threadContext.m_pContext != nullptr && m_threadContext.m_pClient != nullptr)
    {
        Finish();
    }
}

bool RGPClientInProcessModel::Init(bool rgpEnabled)
{
    bool success = InitDriverProtocols();
    if (rgpEnabled == true && success == true)
    {
        CreateWorkerThreadToResumeDriverAndCollectRgpTrace();
        return true;
    }
    return false;
}

void RGPClientInProcessModel::Finish()
{
    m_requestingShutdown = true;
    if (m_finished == false)
    {
        g_workerThreadMutex.lock();
        while (g_workerState != STATE_DONE)
        {
            g_workerThreadMutex.unlock();
            DevDriver::Platform::Sleep(10);
            g_workerThreadMutex.lock();
            if (g_workerState == STATE_IDLE)
            {
                g_workerState = STATE_FINISHED;
            }
        }
        g_workerThreadMutex.unlock();
        DeInitDriverProtocols();
        m_finished = true;
    }
}

bool RGPClientInProcessModel::InitDriverProtocols()
{
    DevDriver::ListenerCreateInfo createInfo = {};
    const char* kListenerDescription = "Radeon Developer Service [RGPClientInProcess]";
    DevDriver::Platform::Strncpy(createInfo.description, kListenerDescription, sizeof(createInfo.description));
    createInfo.flags.enableServer = 1;
    createInfo.serverCreateInfo.enabledProtocols.etw = true;

    createInfo.allocCb = GenericAllocCb;

    DevDriver::Result result = m_listenerCore.Initialize(createInfo);
    if (DevDriver::Result::Success != result)
    {
        DbgMsg("Failed to initialize listener core");
        return false;
    }
    DbgMsg("Listener core initialized successfully");

    DevDriver::DevDriverClientCreateInfo clientCreateInfo = {};
    clientCreateInfo.transportCreateInfo.type = DevDriver::TransportType::Local;

    DevDriver::Platform::Strncpy(clientCreateInfo.transportCreateInfo.clientDescription, "RGPClientInProcess", sizeof(clientCreateInfo.transportCreateInfo.clientDescription));

    clientCreateInfo.transportCreateInfo.componentType = DevDriver::Component::Tool;

    clientCreateInfo.transportCreateInfo.createUpdateThread = true;
    clientCreateInfo.transportCreateInfo.initialFlags = static_cast<DevDriver::StatusFlags>(DevDriver::ClientStatusFlags::DeveloperModeEnabled) |
                                                        static_cast<DevDriver::StatusFlags>(DevDriver::ClientStatusFlags::HaltOnConnect);

    clientCreateInfo.transportCreateInfo.allocCb = createInfo.allocCb;

    m_pClient = new(std::nothrow) DevDriver::DevDriverClient(clientCreateInfo);
    if (nullptr == m_pClient)
    {
        DbgMsg("Failed to allocate memory for client");
        return false;
    }

    DevDriver::Result initResult = m_pClient->Initialize();
    if (DevDriver::Result::Success != initResult)
    {
        DbgMsg("Failed to initialize client");
        return false;
    }
    DbgMsg("Client initialized successfully");

	return true;
}

void RGPClientInProcessModel::DeInitDriverProtocols()
{
#if 1
    if (m_thread.Join() != DevDriver::Result::Success)
    {
        DbgMsg("Failed to join rgp client thread");
    }
#endif

    if (m_pClient != nullptr)
    {
        m_pClient->Destroy();
        delete m_pClient;
        m_pClient = nullptr;
    }

    m_listenerCore.Destroy();
}

//-----------------------------------------------------------------------------
/// Generate the name of the profile to be saved. This is obtained from the
/// process name and a generated time stamp, in much the same way that the
/// panel does things.
/// \param profileName A string to accept the generated profile name string
//-----------------------------------------------------------------------------
void RGPClientInProcessModel::GenerateProfileName(std::string& profileName)
{
    char processName[1024];
    DevDriver::Platform::GetProcessName(&processName[0], sizeof(processName));

    // calculate the timestamp as yyyymmdd-hhmmss
    const int BUFFER_SIZE = 32;
    char timeStamp[BUFFER_SIZE];

    time_t t = time(nullptr);
    tm* timePtr = localtime(&t);
    sprintf_s(timeStamp, BUFFER_SIZE, "-%04d%02d%02d-%02d%02d%02d", timePtr->tm_year + 1900, timePtr->tm_mon + 1, timePtr->tm_mday, timePtr->tm_hour, timePtr->tm_min, timePtr->tm_sec);

    // build the filename, consisting of exe name, timestamp and rgp extension. Rip off the '.exe' from the executable name if it exists
    std::string delimiter = ".exe";
    size_t pos = 0;
    std::string executableName = processName;

    if ((pos = executableName.find(delimiter)) != std::string::npos)
    {
        executableName = executableName.substr(0, pos);
    }

    profileName = executableName + timeStamp + gs_RGP_TRACE_EXTENSION;
}

//-----------------------------------------------------------------------------
/// Set the GPU clock mode to be used when collecting an RGP trace.
/// \returns Result::Success if the clock mode was set correctly, and an error code if it failed.
//-----------------------------------------------------------------------------
DevDriver::Result RGPClientInProcessModel::SetGPUClockMode(
    DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient,
    DevDriver::DriverControlProtocol::DeviceClockMode      kTraceClockMode)
{
    using namespace DevDriver;

    Result setClockResult = Result::Error;

    if (pDriverControlClient != nullptr && pDriverControlClient->IsConnected())
    {
        // RDP explicitly sets the GPU's clock mode to ensure timing accuracy while collecting a trace.
        static const int kGPUIndex = 0;
        setClockResult = pDriverControlClient->SetDeviceClockMode(kGPUIndex, kTraceClockMode);
        if (setClockResult == Result::Success)
        {
            DbgMsg("Set/Reset clock mode for profiling.");
        }
    }
    else
    {
        DbgMsg("Didn't set/reset clock for profiling because DriverControlClient wasn't connected.");
    }

    if (setClockResult != Result::Success)
    {
        DbgMsg("Failed to set/reset GPU clocks for profiling.");
    }

    return setClockResult;
}

struct RGPTraceContext
{
    std::ofstream*    pRgpFile;
    DevDriver::uint64 numChunks;                 ///< The number of chunks received for the trace file.
    size_t            totalTraceSizeInBytes;     ///< The final total size of the RGP trace file in bytes.
};

void RGPChunkFunc(const DevDriver::RGPProtocol::TraceDataChunk* pChunk, void* pUserdata)
{
    RGPTraceContext* pTraceContext = reinterpret_cast<RGPTraceContext*>(pUserdata);
    std::ofstream*   pRgpFile      = pTraceContext->pRgpFile;

    pRgpFile->write(reinterpret_cast<const char*>(pChunk->data), pChunk->dataSize);

    pTraceContext->numChunks++;
    pTraceContext->totalTraceSizeInBytes += pChunk->dataSize;
}

bool RGPClientInProcessModel::CollectRgpTrace(
    DevDriver::RGPProtocol::RGPClient*                     pRgpClient,
    DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient,
    const RGPProfileParameters&                            profileParameters)
{
    using namespace DevDriver;

    std::ofstream rgpFile;
    RGPTraceContext clientTraceContext = {};
    clientTraceContext.pRgpFile = &rgpFile;

    DevDriver::RGPProtocol::BeginTraceInfo traceInfo = {};
    traceInfo.callbackInfo.chunkCallback = &RGPChunkFunc;
    traceInfo.callbackInfo.pUserdata = &clientTraceContext;
    traceInfo.parameters.numPreparationFrames = 4;
    traceInfo.parameters.flags.enableInstructionTokens = false;
    traceInfo.parameters.flags.allowComputePresents = false;

#ifdef GPUOPEN_SESSION_INTERFACE_CLEANUP_VERSION
    traceInfo.parameters.beginTag = profileParameters.beginTag;
    traceInfo.parameters.endTag = profileParameters.endTag;
    if (profileParameters.pBeginMarker != nullptr)
    {
        DevDriver::Platform::Strncpy(traceInfo.parameters.beginMarker, profileParameters.pBeginMarker, sizeof(traceInfo.parameters.beginMarker));
    }
    if (profileParameters.pEndMarker)
    {
        DevDriver::Platform::Strncpy(traceInfo.parameters.endMarker, profileParameters.pEndMarker, sizeof(traceInfo.parameters.endMarker));
    }
#endif

    // Set the GPU clock mode before starting a trace.
    Result setClocks = SetGPUClockMode(pDriverControlClient, DevDriver::DriverControlProtocol::DeviceClockMode::Peak);

    Result requestResult = pRgpClient->BeginTrace(traceInfo);

    Result restoredClocks = Result::Error;

    if (requestResult == Result::Success)
    {
        DbgMsg("Profiling began successfully.");

        uint32 numChunks = 0;
        uint64 traceSizeInBytes = 0;

        requestResult = pRgpClient->EndTrace(&numChunks, &traceSizeInBytes);

        // Revert the clock mode to the RDP default after tracing.
        if (setClocks == Result::Success)
        {
            restoredClocks = SetGPUClockMode(pDriverControlClient, DevDriver::DriverControlProtocol::DeviceClockMode::Default);
        }

        if (requestResult == Result::Success || requestResult == Result::Unavailable)
        {
            // Only try to write the trace file if the trace executed correctly.
            if (m_profileName.empty())
            {
                GenerateProfileName(m_profileName);
            }
            rgpFile.open(m_profileName.c_str(), std::ios::out | std::ios::binary);

            // Read chunks until we hit the end of the stream.
            do
            {
                requestResult = pRgpClient->ReadTraceDataChunk();
            } while (requestResult == Result::Success);

            if (requestResult == Result::EndOfStream)
            {
                // If the result is EndOfStream, it means that all of the trace chunks were transferred.
                // Just return Success to better indicate that the trace was successful.
                requestResult = Result::Success;

                if (rgpFile.is_open())
                {
                    rgpFile.close();
                }
                DbgMsg("RGP trace file captured.");
                SetProfileCaptured(true);
            }
        }

        return true;
    }
    else
    {
        DbgMsg("Failed to begin profile");

        // Looks like tracing failed- Attempt to revert the clock state to what it was originally.
        restoredClocks = SetGPUClockMode(pDriverControlClient, DevDriver::DriverControlProtocol::DeviceClockMode::Default);
        if (restoredClocks != Result::Success)
        {
            DbgMsg("Failed to restore GPU clocks to default after profiling.");
        }

        return false;
    }
}

bool RGPClientInProcessModel::ConnectProtocolClients(
    DevDriver::DevDriverClient*                             pClient,
    DevDriver::ClientId                                     clientId,
    DevDriver::RGPProtocol::RGPClient*&                     pRgpClientOut,
    DevDriver::DriverControlProtocol::DriverControlClient*& pDriverControlClientOut)
{
    using namespace DevDriver;
    using namespace RGPProtocol;
    using namespace DriverControlProtocol;

    bool bReturn = true;

    // Connect Driver Control Client
    pDriverControlClientOut = pClient->AcquireProtocolClient<Protocol::DriverControl>();
    if (nullptr == pDriverControlClientOut)
    {
        DbgMsg("Driver control client not available");
        bReturn = false;
    }
    else
    {
        DbgMsg("Driver control client is available");
    }

    if (pDriverControlClientOut->Connect(clientId) != Result::Success)
    {
        DbgMsg("Failed to connect DriverControlClient");
        bReturn = false;
    }
    else
    {
        DbgMsg("Driver control client is connected");
    }

    // Connect RGP Client
    pRgpClientOut = pClient->AcquireProtocolClient<Protocol::RGP>();
    if (nullptr == pRgpClientOut)
    {
        DbgMsg("RGP client not available");
        bReturn = false;
    }
    else
    {
        DbgMsg("RGP client is available");
    }

    if (pRgpClientOut->Connect(clientId) != DevDriver::Result::Success)
    {
        DbgMsg("Failed to connect rgp client");
        bReturn = false;
    }
    else
    {
        DbgMsg("RGP client connected");
    }

    return bReturn;
}

void RGPClientInProcessModel::DisconnectProtocolClients(
    DevDriver::DevDriverClient*                            pClient,
    DevDriver::RGPProtocol::RGPClient*                     pRgpClient,
    DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient)
{
    if (pClient == nullptr)
    {
        return;
    }

    if (pRgpClient != nullptr && pRgpClient->IsConnected())
    {
        pRgpClient->Disconnect();
        pClient->ReleaseProtocolClient(pRgpClient);
    }

    if (pDriverControlClient != nullptr && pDriverControlClient->IsConnected())
    {
        pDriverControlClient->Disconnect();
        pClient->ReleaseProtocolClient(pDriverControlClient);
    }
}

bool RGPClientInProcessModel::EnableRgpProfiling(DevDriver::RGPProtocol::RGPClient* pRgpClient)
{
    using namespace DevDriver;
    using namespace RGPProtocol;

    // Make sure profiling status starts as Available
    ProfilingStatus profilingStatus = ProfilingStatus::NotAvailable;
    if (pRgpClient->QueryProfilingStatus(&profilingStatus) != Result::Success)
    {
        DbgMsg("Failed to query rgp profiling status on client");
    }
    else
    {
        DbgMsg("Successfull to query rgp profiling status on client");
    }

    if (profilingStatus != ProfilingStatus::Available)
    {
        DbgMsg("RGP profiling status is not available");
    }
    else
    {
        DbgMsg("RGP profiling status is available");
    }

    Result result = pRgpClient->EnableProfiling();

    if (result == Result::Success)
    {
        DbgMsg("RGP profiling enabled");
        return true;
    }
    else
    {
        DbgMsg("Failed to enable RGP profiling");
        return false;
    }
}

bool RGPClientInProcessModel::ResumeDriverAndWaitForDriverInitilization(DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient)
{
    using namespace DevDriver;
    bool bReturn = true;

    if (pDriverControlClient->ResumeDriver() != Result::Success)
    {
        DbgMsg("Failed to resume driver");
        bReturn = false;
    }
    else
    {
        DbgMsg("Driver resumed");
    }

    DbgMsg("Waiting for driver initialization on client");
    Result result = pDriverControlClient->WaitForDriverInitialization(8000);
    if (result == Result::Success)
    {
        DbgMsg("Wait for driver initialization successful on client");
    }
    else if (result == Result::Unavailable)
    {
        DbgMsg("Wait for driver initialization not available on client");
        bReturn = false;
    }
    else if (result == Result::NotReady)
    {
        DbgMsg("Wait for driver initialization timed out on client");
        bReturn = false;
    }
    else
    {
        DbgMsg("Wait for driver initialization failed on client\n");
        bReturn = false;
    }

    return bReturn;
}

bool ProcessHaltedMessage(RGPClientInProcessModel* pContext, DevDriver::ClientId clientId)
{
    return pContext->ProcessHaltedMessage(clientId);
}

bool RGPClientInProcessModel::ProcessHaltedMessage(DevDriver::ClientId clientId)
{
    if (m_pClient->IsConnected())
    {
        bool resumed = false;

        using namespace DevDriver;
        using namespace RGPProtocol;
        using namespace DriverControlProtocol;

        RGPClient*            pRgpClient = nullptr;
        DriverControlClient*  pDriverControlClient = nullptr;

        ConnectProtocolClients(m_pClient, clientId, pRgpClient, pDriverControlClient);
        m_clientId = clientId;

        if (pRgpClient != nullptr)
        {
            EnableRgpProfiling(pRgpClient);
        }

        if (pDriverControlClient != nullptr)
        {
            resumed = ResumeDriverAndWaitForDriverInitilization(pDriverControlClient);
        }
        DisconnectProtocolClients(m_pClient, pRgpClient, pDriverControlClient);

        return resumed;
    }
    return false;
}

//-----------------------------------------------------------------------------
/// Validate if a capture is possible. Checks various capture parameters and
/// sees if the requested capture features are enabled in the driver or
/// interface.
/// If the user requests a particular feature and that feature isn't available
/// then a capture isn't possible since it doesn't do exactly what the user
/// wants, even though it may be possible to perform a capture without the
/// requested feature.
/// \param requestingFrameTerminators Is the user requesting frame terminators
/// \return true if a capture is possible, false otherwise
//-----------------------------------------------------------------------------
bool RGPClientInProcessModel::IsCaptureAllowed(bool requestingFrameTerminators)
{
    using namespace DevDriver;
    using namespace RGPProtocol;
    using namespace DriverControlProtocol;

    RGPClient*            pRgpClient = nullptr;
    DriverControlClient*  pDriverControlClient = nullptr;

    ConnectProtocolClients(m_pClient, m_clientId, pRgpClient, pDriverControlClient);

    bool userMarkerVersion = false;

    // check the protocol trigger marker version. Need to fail if the user has chosen to use
    // trigger markers but the DevDriverTools don't support it
#ifdef RGP_TRIGGER_MARKERS_VERSION
    if (pRgpClient->GetSessionVersion() >= RGP_TRIGGER_MARKERS_VERSION)
    {
        userMarkerVersion = true;
    }
#endif

    DisconnectProtocolClients(m_pClient, pRgpClient, pDriverControlClient);

    if (requestingFrameTerminators == true && userMarkerVersion == false)
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
/// Collect a trace. This function is run in the worker thread to do the
/// actual frame capture.
//-----------------------------------------------------------------------------
void RGPClientInProcessModel::CollectTrace()
{
    using namespace DevDriver;
    using namespace RGPProtocol;
    using namespace DriverControlProtocol;

    RGPClient*            pRgpClient = nullptr;
    DriverControlClient*  pDriverControlClient = nullptr;

    ConnectProtocolClients(m_pClient, m_clientId, pRgpClient, pDriverControlClient);

    if (pRgpClient != nullptr && pDriverControlClient != nullptr)
    {
        RGPProfileParameters profileParameters = {};
        profileParameters.beginTag = m_beginTag;
        profileParameters.endTag = m_endTag;
        profileParameters.pBeginMarker = m_beginMarker.c_str();
        profileParameters.pEndMarker = m_endMarker.c_str();
        CollectRgpTrace(pRgpClient, pDriverControlClient, profileParameters);
    }

    DisconnectProtocolClients(m_pClient, pRgpClient, pDriverControlClient);
}

//-----------------------------------------------------------------------------
/// Worker thread to wait until the driver halted message has been received
/// and resume the application
//-----------------------------------------------------------------------------
static void WorkerInit(void* pThreadParam)
{
    bool result = false;
    using namespace DevDriver;
    RGPWorkerThreadContext*  pThreadContext = reinterpret_cast<RGPWorkerThreadContext*>(pThreadParam);
    RGPClientInProcessModel* pContext = pThreadContext->m_pContext;
    DevDriverClient*         pClient = pThreadContext->m_pClient;

    MessageBuffer message = {};
    IMsgChannel* pMsgChannel = pClient->GetMessageChannel();
    const uint32 kLogDelayInMs = 100;

    while (pMsgChannel->IsConnected() && result == false && pContext->IsRequestingShutdown() == false)
    {
        DevDriver::Result status = pMsgChannel->Receive(message, kLogDelayInMs);
        while (status == DevDriver::Result::Success && !pContext->IsProfileCaptured())
        {
            if (message.header.protocolId == Protocol::System)
            {
                using namespace SystemProtocol;
                DevDriver::SystemProtocol::SystemMessage msg = static_cast<SystemMessage>(message.header.messageId);
                switch (msg)
                {
                case SystemMessage::Halted:
                    result = ProcessHaltedMessage(pContext, message.header.srcClientId);
                    break;
                default:
                    break;
                }
            }
            status = pMsgChannel->Receive(message, 0);
        }
    }
    g_workerThreadMutex.lock();
    g_workerState = STATE_IDLE;
    g_workerThreadMutex.unlock();
}

static void WorkerCapture(void* pThreadParam)
{
    using namespace DevDriver;
    RGPWorkerThreadContext*  pThreadContext = reinterpret_cast<RGPWorkerThreadContext*>(pThreadParam);
    RGPClientInProcessModel* pContext = pThreadContext->m_pContext;

    pContext->CollectTrace();
    g_workerThreadMutex.lock();
    g_workerState = STATE_IDLE;
    g_workerThreadMutex.unlock();
}

// Worker thread function. This is set up at init time, and WorkerInit is called then. Once a capture is triggered,
// the WorkerCapture() function is called
void RGPWorkerThreadFunc(void* pThreadParam)
{
    g_workerThreadMutex.lock();
    WorkerThreadState state = g_workerState;
    g_workerThreadMutex.unlock();

    while (state != STATE_FINISHED)
    {
        switch (state)
        {
        case STATE_INIT:
            WorkerInit(pThreadParam);
            break;

        case STATE_CAPTURING:
            WorkerCapture(pThreadParam);
            break;

        default:
            break;
        }
        g_workerThreadMutex.lock();
        state = g_workerState;
        g_workerThreadMutex.unlock();
    }
    g_workerThreadMutex.lock();
    g_workerState = STATE_DONE;
    g_workerThreadMutex.unlock();
}

bool RGPClientInProcessModel::SetTriggerMarkerParams(uint64_t beginTag, uint64_t endTag, const char* beginMarker, const char* endMarker)
{
    bool requestingFrameTerminators = false;

    if (beginTag != 0 && endTag != 0)
    {
        m_beginTag = beginTag;
        m_endTag = endTag;
        requestingFrameTerminators = true;
    }
    if (beginMarker != nullptr && endMarker != nullptr)
    {
        m_beginMarker = beginMarker;
        m_endMarker = endMarker;
        requestingFrameTerminators = true;
    }
    return requestingFrameTerminators;
}

bool RGPClientInProcessModel::TriggerCapture(const char* pszCaptureFileName)
{
    g_workerThreadMutex.lock();
    if (g_workerState == STATE_IDLE)
    {
        g_workerState = STATE_CAPTURING;
        g_workerThreadMutex.unlock();
        SetProfileCaptured(false);
        m_profileName = "";
        if (pszCaptureFileName != nullptr)
        {
            m_profileName = pszCaptureFileName;
        }
        return true;
    }
    g_workerThreadMutex.unlock();
    return false;
}

bool RGPClientInProcessModel::CreateWorkerThreadToResumeDriverAndCollectRgpTrace()
{
    m_threadContext.m_pContext = this;
    m_threadContext.m_pClient  = m_pClient;

    // reset the initial state
    g_workerThreadMutex.lock();
    g_workerState = STATE_INIT;
    g_workerThreadMutex.unlock();

    if (m_thread.Start(RGPWorkerThreadFunc, (void *)&m_threadContext) != DevDriver::Result::Success)
    {
        DbgMsg("Failed to create rgp worker thread");
        return false;
    }
    DbgMsg("Successfull to create rgp worker thread");

    if (!m_thread.IsJoinable())
    {
        DbgMsg("Rgp worker thread is not joinable");
        return false;
    }

#if 0
    if (m_thread.Join() != DevDriver::Result::Success)
    {
        DbgMsg("Failed to join rgp worker thread");
    }
#endif

    return true;
}
