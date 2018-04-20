/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All rights reserved.
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
/**
***********************************************************************************************************************
* @file  listenerCore.h
* @brief Class declaration for ListenerCore
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include <vector>
#include <mutex>
#include "routerCore.h"
#include "msgChannel.h"
#include "ddPlatform.h"
#include "listenerServer.h"
#include "ddListenerURIService.h"

namespace DevDriver
{
    // Flags for configuring the listener core's behavior
    union ListenerConfigFlags
    {
        struct
        {
            // @Todo: Rename enableUWP support to enableKmd
            uint32 enableUWP    : 1;  // Enables support for applications inside the Universal Windows Platform
                                      // environment
            uint32 enableServer : 1;  // Enables the built-in listener server which allows the listener to
                                      // communicate at an application protocol level with other clients on
                                      // the bus
            uint32 reserved     : 30; // Reserved for future usage
        };
        uint32     value;
    };

    // An address and port pair that the listener can listen for connections on
    struct ListenerBindAddress
    {
        char   hostAddress[kMaxStringLength]; // Network host address
        uint32 port;                          // Network port
    };

    // Creation information for the listener core object
    struct ListenerCreateInfo
    {
        char                     description[kMaxStringLength]; // Description string used to identify the listener on the message bus
        ListenerConfigFlags      flags;                         // Configuration flags
        ListenerServerCreateInfo serverCreateInfo;              // Creation information for the built in listener server
        ListenerBindAddress*     pAddressesToBind;              // A list of addresses to lister for connections on
        uint32                   numAddresses;                  // The number of entries in pAddressesToBind
        AllocCb                  allocCb;                       // An allocation callback that is used to manage memory allocations
    };

    // Logs a message to the console.
    // Also logs a message into the logging protocol if the logging server is enabled on the listener.
    void LogMessage(LogLevel logLevel, const char* format, ...);

    // Listener Core
    // Designed to be a self contained class that manages all of the complexity of routing packets between clients on the message bus.
    // Allows for limited configuration through the ListenerCreateInfo struct and otherwise behaves in a specific manner depending on
    // the underlying platform.
    class ListenerCore
    {
    public:
        // Constructor
        ListenerCore();

        // Destructor
        ~ListenerCore();

        // Initialization
        Result Initialize(ListenerCreateInfo& createInfo);

        // Destruction
        void Destroy();

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
        // Set the developer mode enabled client status flag
        bool SetDeveloperModeEnabled(bool developerModeEnabled);

        // Get the developer mode enabled client status flag
        bool GetDeveloperModeEnabled();

        // Set the halt on connect client status flag
        bool SetHaltOnConnect(bool haltOnConnect);

        // Get the halt on connect client status flag
        bool GetHaltOnConnect();
#endif

        // Constructs a vector of the currently connected clients and returns it
        // This function has to acquire an internal lock so it should not be considered a "cheap" function
        std::vector<ClientInfo> GetConnectedClientList();

        // Returns a list of the currently managed transports.
        const std::vector<std::shared_ptr<IListenerTransport>>& GetManagedTransports() const { return m_managedTransports; }

        // Returns the client manager pointer.
        const IClientManager* GetClientManager() const { return m_pClientManager; }

        // Get the listener server associated with the listener core
        // This will be null if the listener server was not enabled during initialization
        ListenerServer* GetServer() { return m_pServer; }
        const ListenerServer* GetServer() const { return m_pServer; }

        // Returns the ListenerCreateInfo struct that was used to initialize the listener core
        const ListenerCreateInfo& GetCreateInfo() const { return m_createInfo; }

    private:
        ListenerCreateInfo                                      m_createInfo;         // Creation information provided during initialization
        RouterCore                                              m_routerCore;         // The underlying router core object
        std::vector<std::shared_ptr<IListenerTransport>>        m_managedTransports;  // A vector of all transports that are managed by the listener
        std::mutex                                              m_routerMutex;        // A mutex used to make access to the router thread safe
        IClientManager*                                         m_pClientManager;     // Pointer to the current client manager object
        bool                                                    m_started;            // True if the listener has been started, false otherwise
                                                                                      // (Used for internal resource cleanup logic)
        IMsgChannel*                                            m_pMsgChannel;        // Pointer to the underlying message channel object
        ListenerServer*                                         m_pServer;            // Pointer to the current listener server object if it exists
        ListenerURIService                                      m_listenerURIService; // A URI service that provides listener specific information
};

} // DevDriver
