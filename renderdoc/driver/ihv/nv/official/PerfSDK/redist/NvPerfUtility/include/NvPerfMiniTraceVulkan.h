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

#include "NvPerfInit.h"
#include "NvPerfScopeExitGuard.h"
#include "NvPerfVulkan.h"
#include "NvPerfMiniTrace.h"

namespace nv { namespace perf { namespace mini_trace {

    template <class TPredicateFunc>
    NVPA_Bool PredicateFuncInvoker(void* self, VkQueue queue)
    {
        auto& predicateFunc = *(TPredicateFunc*)self;
        const NVPA_Bool predicate = predicateFunc(queue);
        return predicate;
    }

    // The UserData is mutable and persistent across vkQueueSubmit()s.
    template <class TMarkerFunc>
    void MarkerFuncInvoker(void* self, VkQueue queue, uint8_t* pUserData, size_t userDataSize)
    {
        auto& markerFunc = *(TMarkerFunc*)self;
        markerFunc(queue, pUserData, userDataSize);
    }

    // Return 0 to skip the timestamp
    template <class TAddressFunc>
    uint64_t AddressFuncInvoker(void* self, VkQueue queue)
    {
        auto& addressFunc = *(TAddressFunc*)self;
        uint64_t gpuva = addressFunc(queue);
        return gpuva;
    }

    class MiniTraceVulkan
    {
    private:
        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;
        NVPW_VK_MiniTrace_DeviceState* m_pDeviceState;
        bool m_isInitialized;

    public:
        MiniTraceVulkan()
            : m_instance(VK_NULL_HANDLE)
            , m_physicalDevice(VK_NULL_HANDLE)
            , m_device(VK_NULL_HANDLE)
            , m_pDeviceState()
            , m_isInitialized()
        {
        }
        MiniTraceVulkan(const MiniTraceVulkan& trace) = delete;
        MiniTraceVulkan(MiniTraceVulkan&& trace)
            : m_instance(trace.m_instance)
            , m_physicalDevice(trace.m_physicalDevice)
            , m_device(trace.m_device)
            , m_pDeviceState(trace.m_pDeviceState)
            , m_isInitialized(trace.m_isInitialized)
        {
            trace.m_pDeviceState = nullptr;
            trace.m_isInitialized = false;
        }
        ~MiniTraceVulkan()
        {
            Reset();
        }
        MiniTraceVulkan& operator=(const MiniTraceVulkan& trace) = delete;
        MiniTraceVulkan& operator=(MiniTraceVulkan&& trace)
        {
            Reset();
            m_instance = trace.m_instance;
            m_physicalDevice = trace.m_physicalDevice;
            m_device = trace.m_device;
            m_pDeviceState = trace.m_pDeviceState;
            m_isInitialized = trace.m_isInitialized;
            trace.m_pDeviceState = nullptr;
            trace.m_isInitialized = false;
            return *this;
        }

        bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                       , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                       , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                       )
        {
            if (!InitializeNvPerf())
            {
                NV_PERF_LOG_ERR(10, "InitializeNvPerf failed\n");
                return false;
            }
            if (!VulkanLoadDriver(instance))
            {
                NV_PERF_LOG_ERR(10, "Could not load driver\n");
                return false;
            }
            if (!VulkanIsGpuSupported(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                     , pfnVkGetInstanceProcAddr
                                     , pfnVkGetDeviceProcAddr
#endif
                                     ))
            {
                return false;
            }

            NVPW_VK_MiniTrace_DeviceState_Create_Params createParams = { NVPW_VK_MiniTrace_DeviceState_Create_Params_STRUCT_SIZE };
            createParams.instance = instance;
            createParams.physicalDevice = physicalDevice;
            createParams.device = device;
#if defined(VK_NO_PROTOTYPES)
            createParams.pfnGetInstanceProcAddr = (void*)pfnVkGetInstanceProcAddr;
            createParams.pfnGetDeviceProcAddr = (void*)pfnVkGetDeviceProcAddr;
#else
            createParams.pfnGetInstanceProcAddr = (void*)vkGetInstanceProcAddr;
            createParams.pfnGetDeviceProcAddr = (void*)vkGetDeviceProcAddr;
#endif
            NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_DeviceState_Create(&createParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_VK_MiniTrace_DeviceState_Create failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            m_instance = instance;
            m_physicalDevice = physicalDevice;
            m_device = device;
            m_pDeviceState = createParams.pDeviceState;
            m_isInitialized = true;
            return true;
        }

        void Reset()
        {
            if (m_isInitialized)
            {
                NVPW_VK_MiniTrace_DeviceState_Destroy_Params destroyParams = { NVPW_VK_MiniTrace_DeviceState_Destroy_Params_STRUCT_SIZE };
                destroyParams.pDeviceState = m_pDeviceState;
                NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_DeviceState_Destroy(&destroyParams);
                if (nvpaStatus != NVPA_STATUS_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "NVPW_VK_MiniTrace_DeviceState_Destroy failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                }
                m_instance = VK_NULL_HANDLE;
                m_physicalDevice = VK_NULL_HANDLE;
                m_device = VK_NULL_HANDLE;
                m_pDeviceState = nullptr;
                m_isInitialized = false;
            }
        }

        bool IsInitialized() const
        {
            return m_isInitialized;
        }

        bool RegisterQueue(VkQueue queue)
        {
            NVPW_VK_MiniTrace_Queue_Register_Params queueRegisterParams = { NVPW_VK_MiniTrace_Queue_Register_Params_STRUCT_SIZE };
            queueRegisterParams.pDeviceState = m_pDeviceState;
            queueRegisterParams.queue = queue;
            NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_Queue_Register(&queueRegisterParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_VK_MiniTrace_Queue_Register failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        bool UnregisterQueue(VkQueue queue)
        {
            NVPW_VK_MiniTrace_Queue_Unregister_Params queueUnregisterParams = { NVPW_VK_MiniTrace_Queue_Unregister_Params_STRUCT_SIZE };
            queueUnregisterParams.queue = queue;
            NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_Queue_Unregister(&queueUnregisterParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_VK_MiniTrace_Queue_Unregister failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        // `TPredicateFunc` is expected to be in the form of bool(VkQueue)
        template <class TPredicateFunc>
        bool FrontEndTrigger(VkCommandBuffer commandBuffer, bool useComputeMethods, TPredicateFunc&& predicateFunc)
        {
            static_assert(std::is_trivially_destructible<TPredicateFunc>::value, "The PredicateFunc copied into the command buffer must have a trivial destructor.");
            static_assert(std::is_trivially_copyable<TPredicateFunc>::value, "The PredicateFunc copied into the command buffer must be trivially copyable.");

            NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Params frontEndTriggerParams = { NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Params_STRUCT_SIZE };
            frontEndTriggerParams.pDeviceState = m_pDeviceState;
            frontEndTriggerParams.commandBuffer = commandBuffer;
            frontEndTriggerParams.useComputeMethods = useComputeMethods;
            frontEndTriggerParams.predicateFuncInvoker = PredicateFuncInvoker<TPredicateFunc>;
            frontEndTriggerParams.pPredicateFunc = &predicateFunc;
            frontEndTriggerParams.predicateFuncSize = sizeof(predicateFunc);
            NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger(&frontEndTriggerParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        // `TMarkerFunc` is expected to be in the form of void(VkQueue, uint8_t* pUserData, size_t userDataSize)
        template <class TMarkerFunc>
        bool MarkerCpu(VkCommandBuffer commandBuffer, const uint8_t* pUserData, size_t userDataSize, TMarkerFunc&& markerFunc)
        {
            static_assert(std::is_trivially_destructible<TMarkerFunc>::value, "The MarkerFunc copied into the command buffer must have a trivial destructor.");
            static_assert(std::is_trivially_copyable<TMarkerFunc>::value, "The MarkerFunc copied into the command buffer must be trivially copyable.");

            NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Params markerCpuParams = { NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Params_STRUCT_SIZE };
            markerCpuParams.pDeviceState = m_pDeviceState;
            markerCpuParams.commandBuffer = commandBuffer;
            markerCpuParams.markerFuncInvoker = MarkerFuncInvoker<TMarkerFunc>;
            markerCpuParams.pMarkerFunc = &markerFunc;
            markerCpuParams.markerFuncSize = sizeof(markerFunc);
            markerCpuParams.pUserData = pUserData;
            markerCpuParams.userDataSize = userDataSize;
            NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu(&markerCpuParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        // `TAddressFunc` is expected to be in the form of uint64_t(VkQueue)
        template <class TAddressFunc>
        bool HostTimestamp(VkCommandBuffer commandBuffer, uint32_t payload, TAddressFunc&& addressFunc)
        {
            static_assert(std::is_trivially_destructible<TAddressFunc>::value, "The AddressFunc copied into the command buffer must have a trivial destructor.");
            static_assert(std::is_trivially_copyable<TAddressFunc>::value, "The AddressFunc copied into the command buffer must be trivially copyable.");

            NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Params hostTimestampParams = { NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Params_STRUCT_SIZE };
            hostTimestampParams.pDeviceState = m_pDeviceState;
            hostTimestampParams.commandBuffer = commandBuffer;
            hostTimestampParams.payload = payload;
            hostTimestampParams.addressFuncInvoker = AddressFuncInvoker<TAddressFunc>;
            hostTimestampParams.pAddressFunc = &addressFunc;
            hostTimestampParams.addressFuncSize = sizeof(addressFunc);
            NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp(&hostTimestampParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }
    };

    class MiniTracerVulkan
    {
    private:
        struct FrameResource
        {
            VkCommandBuffer commandBuffer;
            VkFence fence;
            uint64_t traceBufferGpuVA;
            uint32_t payload;

            FrameResource()
                : commandBuffer(VK_NULL_HANDLE)
                , fence(VK_NULL_HANDLE)
                , traceBufferGpuVA()
                , payload()
            {
            }
        };
        struct Buffer
        {
            VkBuffer buffer;
            VkDeviceMemory deviceMemory;

            Buffer()
                : buffer(VK_NULL_HANDLE)
                , deviceMemory(VK_NULL_HANDLE)
            {
            }
        };
        // Workaround for closure type not being trivially copyable on certain compilers
        class AddressFunc
        {
        private:
            uint64_t m_gpuVA;
        public:
            AddressFunc(uint64_t gpuVA)
                : m_gpuVA(gpuVA)
            {
            }
            uint64_t operator() (VkQueue)
            {
                return m_gpuVA;
            }
        };
    public:
        struct FrameData
        {
            uint64_t frameEndTime;
        };
    private:
        // determined at Initialize()
        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;
        MiniTraceVulkan m_trace;
        bool m_isInitialized;
        // determined at BeginSession()
        VkQueue m_queue;
        VkCommandPool m_commandPool;
        std::vector<FrameResource> m_frameResources;
        size_t m_putFrameIdx;
        size_t m_getFrameIdx;
        size_t m_numUnreadFrames;
        size_t m_perFrameBufferSize;
        Buffer m_traceBuffer;
        bool m_inSession;

        nv::perf::VulkanFunctions m_vkFunctions;

    private:
        static size_t CircularIncrement(size_t index, size_t max)
        {
            return (++index >= max) ? 0 : index;
        }
    public:
        MiniTracerVulkan()
            : m_instance(VK_NULL_HANDLE)
            , m_physicalDevice(VK_NULL_HANDLE)
            , m_device(VK_NULL_HANDLE)
            , m_isInitialized()
            , m_queue(VK_NULL_HANDLE)
            , m_commandPool(VK_NULL_HANDLE)
            , m_putFrameIdx()
            , m_getFrameIdx()
            , m_numUnreadFrames()
            , m_perFrameBufferSize()
            , m_inSession()
            , m_vkFunctions()
        {
        }
        MiniTracerVulkan(const MiniTracerVulkan& tracer) = delete;
        MiniTracerVulkan(MiniTracerVulkan&& tracer)
            : m_instance(tracer.m_instance)
            , m_physicalDevice(tracer.m_physicalDevice)
            , m_device(tracer.m_device)
            , m_trace(std::move(tracer.m_trace))
            , m_isInitialized(tracer.m_isInitialized)
            , m_queue(tracer.m_queue)
            , m_commandPool(tracer.m_commandPool)
            , m_frameResources(std::move(tracer.m_frameResources))
            , m_putFrameIdx(tracer.m_putFrameIdx)
            , m_getFrameIdx(tracer.m_getFrameIdx)
            , m_numUnreadFrames(tracer.m_numUnreadFrames)
            , m_perFrameBufferSize(tracer.m_perFrameBufferSize)
            , m_traceBuffer(std::move(tracer.m_traceBuffer))
            , m_inSession(tracer.m_inSession)
            , m_vkFunctions(tracer.m_vkFunctions)
        {
            tracer.m_isInitialized = false;
            tracer.m_inSession = false;
        }
        ~MiniTracerVulkan()
        {
            Reset();
        }
        MiniTracerVulkan& operator=(const MiniTracerVulkan& tracer) = delete;
        MiniTracerVulkan& operator=(MiniTracerVulkan&& tracer)
        {
            Reset();
            m_instance = tracer.m_instance;
            m_physicalDevice = tracer.m_physicalDevice;
            m_device = tracer.m_device;
            m_trace = std::move(tracer.m_trace);
            m_isInitialized = tracer.m_isInitialized;
            m_queue = tracer.m_queue;
            m_commandPool = tracer.m_commandPool;
            m_frameResources = std::move(tracer.m_frameResources);
            m_putFrameIdx = tracer.m_putFrameIdx;
            m_getFrameIdx = tracer.m_getFrameIdx;
            m_numUnreadFrames = tracer.m_numUnreadFrames;
            m_perFrameBufferSize = tracer.m_perFrameBufferSize;
            m_traceBuffer = std::move(tracer.m_traceBuffer);
            m_inSession = tracer.m_inSession;
            m_vkFunctions = tracer.m_vkFunctions;
            tracer.m_isInitialized = false;
            tracer.m_inSession = false;
            return *this;
        }

        bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                       , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                       , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                       )
        {
            bool success = m_trace.Initialize(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                             , pfnVkGetInstanceProcAddr
                                             , pfnVkGetDeviceProcAddr
#endif
                                             );
            if (!success)
            {
                return false;
            }
            m_instance = instance;
            m_physicalDevice = physicalDevice;
            m_device = device;
#if defined(VK_NO_PROTOTYPES)
            m_vkFunctions.Initialize(instance, device, pfnVkGetInstanceProcAddr, pfnVkGetDeviceProcAddr);
#else
            m_vkFunctions.Initialize();
#endif
            m_isInitialized = true;
            return true;
        }

        // Undo BeginSession + Initialize
        void Reset()
        {
            if (m_isInitialized)
            {
                EndSession();
                m_instance = VK_NULL_HANDLE;
                m_physicalDevice = VK_NULL_HANDLE;
                m_device = VK_NULL_HANDLE;
                m_vkFunctions.Reset();
                m_trace.Reset();
                m_isInitialized = false;
            }
        }

        bool IsInitialized() const
        {
            return m_isInitialized;
        }

        bool InSession() const
        {
            return m_inSession;
        }

        MiniTraceVulkan& GetMiniTrace()
        {
            return m_trace;
        }

        static void AppendDeviceRequiredExtensions(uint32_t vulkanApiVersion, std::vector<const char*>& deviceExtensionNames)
        {
            const uint32_t apiMajorMinor = VK_MAKE_VERSION(VK_VERSION_MAJOR(vulkanApiVersion), VK_VERSION_MINOR(vulkanApiVersion), 0);
            // on 1.2 and newer, we use "vkGetBufferDeviceAddress" which is a core feature
            switch (apiMajorMinor)
            {
#if defined(VK_VERSION_1_0)
                case VK_API_VERSION_1_0:
                {
                    deviceExtensionNames.push_back("VK_EXT_buffer_device_address");
                    deviceExtensionNames.push_back("VK_KHR_device_group");
                    break;
                }
#endif
#if defined(VK_VERSION_1_1)
                case VK_API_VERSION_1_1:
                {
                    deviceExtensionNames.push_back("VK_EXT_buffer_device_address");
                    deviceExtensionNames.push_back("VK_KHR_device_group");
                    break;
                }
#endif
                default:
                {
                    break;
                }
            }
        }

        bool BeginSession(VkQueue queue, uint32_t queueFamilyIndex, size_t maxFrameLatency)
        {
            if (!m_isInitialized)
            {
                NV_PERF_LOG_ERR(50, "Not initialized\n");
                return false;
            }
            if (m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Already in a session\n");
                return false;
            }

            VkResult vkResult = VK_SUCCESS;
            bool success = true;
            success = m_trace.RegisterQueue(queue);
            if (!success)
            {
                return false;
            }
            m_queue = queue;
            
            auto destroyResources = ScopeExitGuard([&]() {
                m_vkFunctions.pfnVkDestroyCommandPool(m_device, m_commandPool, nullptr);
                m_vkFunctions.pfnVkDestroyBuffer(m_device, m_traceBuffer.buffer, nullptr);
                m_vkFunctions.pfnVkFreeMemory(m_device, m_traceBuffer.deviceMemory, nullptr);
                for (FrameResource& frameResource : m_frameResources)
                {
                    m_vkFunctions.pfnVkFreeCommandBuffers(m_device, m_commandPool, 1, &frameResource.commandBuffer);
                    m_vkFunctions.pfnVkDestroyFence(m_device, frameResource.fence, nullptr);
                }
            });

            {
                VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
                commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
                commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                vkResult = m_vkFunctions.pfnVkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool);
                if (vkResult != VK_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "vkCreateCommandPool() failed\n");
                    return false;
                }
            }
            m_perFrameBufferSize = sizeof(NVPW_TimestampReport); // only supports a single timestamp per-frame today
            {
                {
                    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
                    bufferCreateInfo.size = m_perFrameBufferSize * (maxFrameLatency + 1);
                    bufferCreateInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
                    vkResult = m_vkFunctions.pfnVkCreateBuffer(m_device, &bufferCreateInfo, nullptr, &m_traceBuffer.buffer);
                    if (vkResult != VK_SUCCESS)
                    {
                        NV_PERF_LOG_ERR(50, "vkCreateBuffer() failed\n");
                        return false;
                    }
                }
                {
                    auto getMemoryTypeIndex = [&](uint32_t typeBits, VkMemoryPropertyFlags properties) {
                        VkPhysicalDeviceMemoryProperties memoryProperties;
                        m_vkFunctions.pfnVkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
                        for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++)
                        {
                            uint32_t currentTypeBit = 1 << memoryTypeIndex;
                            if (typeBits & currentTypeBit)
                            {
                                if ((memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & properties) == properties)
                                {
                                    return memoryTypeIndex;
                                }
                            }
                        }
                        return (uint32_t)(-1);
                    };

                    VkMemoryRequirements memReqs;
                    m_vkFunctions.pfnVkGetBufferMemoryRequirements(m_device, m_traceBuffer.buffer, &memReqs);
                    VkMemoryAllocateInfo memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
                    memAlloc.allocationSize = memReqs.size;
                    memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                    if (memAlloc.memoryTypeIndex == (uint32_t)(-1))
                    {
                        NV_PERF_LOG_ERR(50, "Unsupported memory type\n");
                        return false;
                    }

                    VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
                    memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
                    memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
                    memAlloc.pNext = &memoryAllocateFlagsInfo;

                    vkResult = m_vkFunctions.pfnVkAllocateMemory(m_device, &memAlloc, nullptr, &m_traceBuffer.deviceMemory);
                    if (vkResult != VK_SUCCESS)
                    {
                        NV_PERF_LOG_ERR(50, "vkAllocateMemory() failed\n");
                        return false;
                    }
                }
                vkResult = m_vkFunctions.pfnVkBindBufferMemory(m_device, m_traceBuffer.buffer, m_traceBuffer.deviceMemory, 0);
                if (vkResult != VK_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "vkBindBufferMemory() failed\n");
                    return false;
                }
            }

            const VkDeviceAddress baseGpuVA = [&]() {
                VkBufferDeviceAddressInfo bufferDeviceAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
                bufferDeviceAddressInfo.buffer = m_traceBuffer.buffer;

                PFN_vkGetBufferDeviceAddress pfnVkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)m_vkFunctions.pfnVkGetDeviceProcAddr(m_device, "vkGetBufferDeviceAddress");
                if (pfnVkGetBufferDeviceAddress)
                {
                    return pfnVkGetBufferDeviceAddress(m_device, &bufferDeviceAddressInfo);
                }

                PFN_vkGetBufferDeviceAddressEXT pfnVkGetBufferDeviceAddressEXT = (PFN_vkGetBufferDeviceAddressEXT)m_vkFunctions.pfnVkGetDeviceProcAddr(m_device, "vkGetBufferDeviceAddressEXT");
                if (pfnVkGetBufferDeviceAddressEXT)
                {
                    return pfnVkGetBufferDeviceAddressEXT(m_device, &bufferDeviceAddressInfo);
                }
                NV_PERF_LOG_ERR(50, "No available API to get buffer device address\n");
                return (VkDeviceAddress)0;
            }();
            if (!baseGpuVA)
            {
                NV_PERF_LOG_ERR(50, "GetBufferDeviceAddress() failed\n");
                return false;
            }

            m_frameResources.resize(maxFrameLatency + 1);
            for (size_t frameIdx = 0; frameIdx < m_frameResources.size(); ++frameIdx)
            {
                FrameResource& frameResource = m_frameResources[frameIdx];
                {
                    VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
                    commandBufferAllocateInfo.commandPool = m_commandPool;
                    commandBufferAllocateInfo.commandBufferCount = 1;
                    vkResult = m_vkFunctions.pfnVkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &frameResource.commandBuffer);
                    if (vkResult != VK_SUCCESS)
                    {
                        NV_PERF_LOG_ERR(50, "vkAllocateCommandBuffers() failed\n");
                        return false;
                    }
                }
                {
                    VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
                    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                    vkResult = m_vkFunctions.pfnVkCreateFence(m_device, &fenceCreateInfo, nullptr, &frameResource.fence);
                    if (vkResult != VK_SUCCESS)
                    {
                        NV_PERF_LOG_ERR(50, "vkCreateFence() failed\n");
                        return false;
                    }
                }
                frameResource.traceBufferGpuVA = baseGpuVA + m_perFrameBufferSize * frameIdx;
                frameResource.payload = 10000;
            }
            destroyResources.Dismiss();
            m_inSession = true;
            return true;
        }

        void EndSession()
        {
            if (m_inSession)
            {
                m_vkFunctions.pfnVkQueueWaitIdle(m_queue);
                m_trace.UnregisterQueue(m_queue);
                m_queue = VK_NULL_HANDLE;
                for (FrameResource& frameResource : m_frameResources)
                {
                    m_vkFunctions.pfnVkFreeCommandBuffers(m_device, m_commandPool, 1, &frameResource.commandBuffer);
                    m_vkFunctions.pfnVkDestroyFence(m_device, frameResource.fence, nullptr);
                }
                m_frameResources.clear();
                m_vkFunctions.pfnVkDestroyCommandPool(m_device, m_commandPool, nullptr);
                m_commandPool = VK_NULL_HANDLE;
                m_vkFunctions.pfnVkDestroyBuffer(m_device, m_traceBuffer.buffer, nullptr);
                m_vkFunctions.pfnVkFreeMemory(m_device, m_traceBuffer.deviceMemory, nullptr);
                m_traceBuffer = Buffer();
                m_putFrameIdx = 0;
                m_getFrameIdx = 0;
                m_numUnreadFrames = 0;
                m_perFrameBufferSize = 0;
                m_inSession = false;
            }
        }

        bool OnFrameEnd()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session\n");
                return false;
            }
            if (m_numUnreadFrames == m_frameResources.size())
            {
                NV_PERF_LOG_ERR(50, "Maximum number of unread frames is reached\n");
                return false;
            }

            bool success = true;
            FrameResource& frameResource = m_frameResources[m_putFrameIdx];
            VkResult vkResult = m_vkFunctions.pfnVkGetFenceStatus(m_device, frameResource.fence);
            if (vkResult == VK_ERROR_DEVICE_LOST)
            {
                NV_PERF_LOG_ERR(50, "Device is lost\n");
                return false;
            }
            if (vkResult == VK_NOT_READY)
            {
                NV_PERF_LOG_ERR(50, "Cannot recycle the frame that has not completed, \"maxFrameLatency\" specified is not suffient to cover the latency\n");
                return false;
            }

            VkCommandBuffer& commandBuffer = frameResource.commandBuffer;
            vkResult = m_vkFunctions.pfnVkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
            if (vkResult != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "vkResetCommandBuffer() failed, VkResult = %d\n", vkResult);
                return false;
            }

            VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkResult = m_vkFunctions.pfnVkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
            if (vkResult != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "vkBeginCommandBuffer() failed, VkResult = %d\n", vkResult);
                return false;
            }

            // capture by reference is okay here because ECL is in the same scope
            success = m_trace.HostTimestamp(commandBuffer, frameResource.payload++, AddressFunc(frameResource.traceBufferGpuVA));
            if (!success)
            {
                return false;
            }

            vkResult = m_vkFunctions.pfnVkEndCommandBuffer(commandBuffer);
            if (vkResult != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "vkEndCommandBuffer() failed, VkResult = %d\n", vkResult);
                return false;
            }

            vkResult = m_vkFunctions.pfnVkResetFences(m_device, 1, &frameResource.fence);
            if (vkResult != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "vkResetFences() failed, VkResult = %d\n", vkResult);
                return false;
            }

            VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            vkResult = m_vkFunctions.pfnVkQueueSubmit(m_queue, 1, &submitInfo, frameResource.fence);
            if (vkResult != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "vkQueueSubmit() failed, VkResult = %d\n", vkResult);
                return false;
            }

            m_putFrameIdx = CircularIncrement(m_putFrameIdx, m_frameResources.size());
            ++m_numUnreadFrames;
            return true;
        }

        bool GetOldestFrameData(bool& isDataReady, FrameData& frameData)
        {
            isDataReady = false;
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session\n");
                return false;
            }

            if (!m_numUnreadFrames)
            {
                return true; // no data to consume, not an error
            }
            FrameResource& oldestFrameResource = m_frameResources[m_getFrameIdx];
            VkResult vkResult = m_vkFunctions.pfnVkGetFenceStatus(m_device, oldestFrameResource.fence);
            if (vkResult == VK_ERROR_DEVICE_LOST)
            {
                NV_PERF_LOG_ERR(50, "Device is lost\n");
                return false;
            }
            if (vkResult == VK_NOT_READY)
            {
                return true; // no data to consume - not finished executing on GPU, not an error
            }
            isDataReady = true;
            {
                // only read back the range belongs to the current frame
                NVPW_TimestampReport* pReport = nullptr;
                vkResult = m_vkFunctions.pfnVkMapMemory(m_device, m_traceBuffer.deviceMemory, m_perFrameBufferSize * m_getFrameIdx, m_perFrameBufferSize, 0, (void**)&pReport);
                auto unmap = ScopeExitGuard([&]() {
                    m_vkFunctions.pfnVkUnmapMemory(m_device, m_traceBuffer.deviceMemory);
                });
                frameData = FrameData{ pReport->timestamp };
            }
            return true;
        }

        bool ReleaseOldestFrame()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session\n");
                return false;
            }
            if (!m_numUnreadFrames)
            {
                return false;
            }
            m_getFrameIdx = CircularIncrement(m_getFrameIdx, m_frameResources.size());
            --m_numUnreadFrames;
            return true;
        }
    };

    class APITracerVulkan
    {
    private:
        struct TraceData
        {
            std::string name;
            // Initialize to -1 to indicate that the value is not set.
            size_t startSlotInVkPool = size_t(-1);
            size_t endSlotInVkPool = size_t(-1);
            size_t nestingLevel = 0;
        };
        VkDevice m_device;
        VkQueryPool m_queryPool;
        VulkanFunctions m_vkFunctions;

        size_t m_nextSlot = 0;
        size_t m_maxSlots = 0;

        std::vector<TraceData> m_traceData;
        std::vector<uint64_t> m_timestamps;
    public:
        bool ResolveQueries(std::vector<APITraceData>& traceData)
        {
            if (m_traceData.size() == 0)
            {
                return true;
            }
            m_timestamps.resize(m_nextSlot);
            VkResult result = m_vkFunctions.pfnVkGetQueryPoolResults(m_device, m_queryPool, 0u, (uint32_t)m_timestamps.size(), m_timestamps.size() * sizeof(uint64_t), m_timestamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            if (result != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "vkGetQueryPoolResults failed\n");
                return false;
            }
            traceData.reserve(traceData.size() + m_traceData.size());
            for (size_t index = 0; index < m_traceData.size(); ++index)
            {
                uint64_t startTimestamp = 0;
                uint64_t endTimestamp = 0;
                if (m_traceData[index].startSlotInVkPool < m_timestamps.size())
                {
                    startTimestamp = m_timestamps[m_traceData[index].startSlotInVkPool];
                }
                if (m_traceData[index].endSlotInVkPool < m_timestamps.size())
                {
                    endTimestamp = m_timestamps[m_traceData[index].endSlotInVkPool];
                }
                traceData.emplace_back(APITraceData{ m_traceData[index].name, startTimestamp, endTimestamp, m_traceData[index].nestingLevel });
            }
            return true;
        }
        void ClearData()
        {
            m_nextSlot = 0;
            m_traceData.clear();
        }
        void ResetQueries(VkCommandBuffer cmd)
        {
            m_vkFunctions.pfnVkCmdResetQueryPool(cmd, m_queryPool, 0u, (uint32_t)m_maxSlots);
        }
        bool BeginRange(VkCommandBuffer cmd, const char* name, size_t nestingLevel, size_t& index)
        {
            size_t slot = m_nextSlot;
            if (slot >= m_maxSlots)
            {
                NV_PERF_LOG_ERR(50, "No more slots\n");
                return false;
            }
            m_traceData.emplace_back(TraceData{name, slot, size_t(-1), nestingLevel});
            index = m_traceData.size() - 1;
            ++m_nextSlot;
            m_vkFunctions.pfnVkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, (uint32_t)slot);
            return true;
        }
        bool EndRange(VkCommandBuffer cmd, size_t index)
        {
            if (index >= m_traceData.size())
            {
                NV_PERF_LOG_ERR(50, "Invalid index\n");
                return false;
            }
            size_t slot = m_nextSlot;
            if (slot >= m_maxSlots)
            {
                NV_PERF_LOG_ERR(50, "No more slots\n");
                return false;
            }
            m_traceData[index].endSlotInVkPool = slot;
            ++m_nextSlot;
            m_vkFunctions.pfnVkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, (uint32_t)slot);
            return true;
        }
        bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, size_t numRanges
#if defined(VK_NO_PROTOTYPES)
                          , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                          , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
        )
        {
#if defined(VK_NO_PROTOTYPES)
            m_vkFunctions.Initialize(instance, device, pfnVkGetInstanceProcAddr, pfnVkGetDeviceProcAddr);
#else
            m_vkFunctions.Initialize();
#endif
            size_t maxSlots = 2 * numRanges;
            VkQueryPoolCreateInfo queryPoolCreateInfo = {};
            queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryPoolCreateInfo.queryCount = (uint32_t)maxSlots;
            VkResult result = m_vkFunctions.pfnVkCreateQueryPool(device, &queryPoolCreateInfo, nullptr, &m_queryPool);
            if (result != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "vkCreateQueryPool failed\n");
                return false;
            }
            m_maxSlots = maxSlots;
            m_device = device;
            return true;
        }

        void Destroy()
        {
            if (m_queryPool)
            {
                m_vkFunctions.pfnVkDestroyQueryPool(m_device, m_queryPool, nullptr);
                m_queryPool = VK_NULL_HANDLE;
            }
        }

        ~APITracerVulkan()
        {
            Destroy();
        }
    };
}}} // nv::perf::minitrace
