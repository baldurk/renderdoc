//=============================================================================
/// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  a class to provide service to initialize dev driver protocols to
///          capture rgp trace from within the profiled application.
//=============================================================================
#ifndef RGP_CLIENT_IN_PROCESS_MODEL_H_
#define RGP_CLIENT_IN_PROCESS_MODEL_H_

#include <../listener/listenerCore.h>
#include <devDriverServer.h>
#include <devDriverClient.h>
#include <protocols/driverControlClient.h>

class RGPClientInProcessModel;

typedef struct
{
    RGPClientInProcessModel*    m_pContext;
    DevDriver::DevDriverClient* m_pClient;
} RGPWorkerThreadContext;

struct RGPProfileParameters
{
    uint64_t beginTag;
    uint64_t endTag;
    const char* pBeginMarker;
    const char* pEndMarker;
};

class RGPClientInProcessModel
{
public:
    RGPClientInProcessModel();
    ~RGPClientInProcessModel();

    bool Init(bool rgpEnabled);
    void Finish();

    bool IsProfileCaptured() const { return m_profileCaptured; }
    bool IsRequestingShutdown() const { return m_requestingShutdown; }
    const char* GetProfileName() const { return m_profileName.c_str(); }

    bool SetTriggerMarkerParams(uint64_t beginTag, uint64_t endTag, const char* beginMarker, const char* endMarker);

    bool TriggerCapture(const char* pszCaptureFileName);
    bool IsCaptureAllowed(bool requestingFrameTerminators);
    void CollectTrace();

    bool ProcessHaltedMessage(DevDriver::ClientId clientId);

private:
    RGPClientInProcessModel(const RGPClientInProcessModel& c);              // disable
    RGPClientInProcessModel& operator=(const RGPClientInProcessModel& c);   // disable

    bool InitDriverProtocols();
    void DeInitDriverProtocols();

    bool CreateWorkerThreadToResumeDriverAndCollectRgpTrace();

    void GenerateProfileName(std::string& profileName);
    void SetProfileCaptured(bool bCaptured) { m_profileCaptured = bCaptured; }

    bool ConnectProtocolClients(
        DevDriver::DevDriverClient*                             pClient,
        DevDriver::ClientId                                     clientId,
        DevDriver::RGPProtocol::RGPClient*&                     pRgpClientOut,
        DevDriver::DriverControlProtocol::DriverControlClient*& pDriverControlClientOut);

    void DisconnectProtocolClients(
        DevDriver::DevDriverClient*                            pClient,
        DevDriver::RGPProtocol::RGPClient*                     pRgpClient,
        DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient);

    DevDriver::Result SetGPUClockMode(
        DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient,
        DevDriver::DriverControlProtocol::DeviceClockMode      kTraceClockMode);

    bool EnableRgpProfiling(DevDriver::RGPProtocol::RGPClient* pRgpClient);

    bool ResumeDriverAndWaitForDriverInitilization(DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient);

    bool CollectRgpTrace(
        DevDriver::RGPProtocol::RGPClient*                     pRgpClient,
        DevDriver::DriverControlProtocol::DriverControlClient* pDriverControlClient,
        const RGPProfileParameters&                            profileParameters);

    DevDriver::ListenerCore                                 m_listenerCore;
    DevDriver::DevDriverClient*                             m_pClient;

    DevDriver::Platform::Thread                             m_thread;
    RGPWorkerThreadContext                                  m_threadContext;

    std::string                                             m_profileName;          ///< The name of the last saved profile

    DevDriver::ClientId                                     m_clientId;             ///< The current client Id
    bool                                                    m_profileCaptured;      ///< Has a profile been captured
    bool                                                    m_finished;             ///< Has Finished() been called. Ensure it's only called once
    std::atomic<bool>                                       m_requestingShutdown;   ///< The application is requesting shutdown, so exit worker thread loops

    uint64_t                                                m_beginTag;             ///< The begin tag name
    uint64_t                                                m_endTag;               ///< The end tag name
    std::string                                             m_beginMarker;          ///< The begin marker name
    std::string                                             m_endMarker;            ///< The end marker name
};

#endif // RGP_CLIENT_IN_PROCESS_MODEL_H_
