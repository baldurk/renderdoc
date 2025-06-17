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

#include <utility>
#include <vector>
#include <memory>
#include <unordered_map>
#include "NvPerfMetricsEvaluator.h"

namespace nv { namespace perf {

    typedef std::unordered_map<std::string, NVPW_RawCounterDomain> RawCounterSchedulingHints; // raw counter name -> domain

    class CounterDataBuilder
    {
    private:
        NVPA_CounterDataBuilder* m_pCounterDataBuilder; // owned

    protected:
        void MoveAssign(CounterDataBuilder&& rhs)
        {
            Reset();
            m_pCounterDataBuilder = rhs.m_pCounterDataBuilder;
            rhs.m_pCounterDataBuilder = nullptr;
        }

    public:
        ~CounterDataBuilder()
        {
            Reset();
        }
        
        CounterDataBuilder(const CounterDataBuilder& rhs) = delete;
        CounterDataBuilder& operator=(const CounterDataBuilder& rhs) = delete;
        
        CounterDataBuilder() : m_pCounterDataBuilder(nullptr)
        {
        }
        CounterDataBuilder(CounterDataBuilder&& rhs) : m_pCounterDataBuilder(nullptr)
        {
            MoveAssign(std::forward<CounterDataBuilder>(rhs));
        }
        CounterDataBuilder& operator=(CounterDataBuilder&& rhs)
        {
            MoveAssign(std::forward<CounterDataBuilder>(rhs));
            return *this;
        }

        operator NVPA_CounterDataBuilder*() const
        {
            return m_pCounterDataBuilder;
        }

        NVPA_CounterDataBuilder* Get() const
        {
            return m_pCounterDataBuilder;
        }

        bool Initialize(const char* pChipName)
        {
            if (m_pCounterDataBuilder)
            {
                return false; // already initialized, call Reset() first
            }

            NVPW_CounterDataBuilder_Create_Params counterDataBuilderParams = { NVPW_CounterDataBuilder_Create_Params_STRUCT_SIZE };
            counterDataBuilderParams.pChipName = pChipName;
            NVPA_Status nvpaStatus = NVPW_CounterDataBuilder_Create(&counterDataBuilderParams);

            m_pCounterDataBuilder = counterDataBuilderParams.pCounterDataBuilder;

            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        void Reset()
        {
            if (!m_pCounterDataBuilder)
            {
                return;
            }

            NVPW_CounterDataBuilder_Destroy_Params counterDataBuilderParams = { NVPW_CounterDataBuilder_Destroy_Params_STRUCT_SIZE };
            counterDataBuilderParams.pCounterDataBuilder = m_pCounterDataBuilder;
            NVPW_CounterDataBuilder_Destroy(&counterDataBuilderParams);
            m_pCounterDataBuilder = nullptr;
        }

        bool AddRawCounters(size_t count, NVPW_RawCounterRequest* pRequests)
        {
            if (!m_pCounterDataBuilder)
            {
                return false;
            }

            NVPW_CounterDataBuilder_AddRawCounters_Params counterDataBuilderAddRawCountersParams = { NVPW_CounterDataBuilder_AddRawCounters_Params_STRUCT_SIZE };
            counterDataBuilderAddRawCountersParams.pCounterDataBuilder = m_pCounterDataBuilder;
            counterDataBuilderAddRawCountersParams.rawCounterRequestStructSize = NVPW_RAW_COUNTER_REQUEST_STRUCT_SIZE;
            counterDataBuilderAddRawCountersParams.numRawCounterRequests = count;
            counterDataBuilderAddRawCountersParams.pRawCounterRequests = pRequests;

            NVPA_Status nvpaStatus = NVPW_CounterDataBuilder_AddRawCounters(&counterDataBuilderAddRawCountersParams);

            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        bool GetCounterDataPrefixSize(size_t* pSize) const
        {
            if (!m_pCounterDataBuilder)
            {
                return false;
            }

            NVPW_CounterDataBuilder_GetCounterDataPrefix_Params getCounterDataPrefixParams = { NVPW_CounterDataBuilder_GetCounterDataPrefix_Params_STRUCT_SIZE };
            getCounterDataPrefixParams.bytesAllocated = 0;
            getCounterDataPrefixParams.pBuffer = nullptr;
            getCounterDataPrefixParams.pCounterDataBuilder = m_pCounterDataBuilder;
            NVPA_Status nvpaStatus = NVPW_CounterDataBuilder_GetCounterDataPrefix(&getCounterDataPrefixParams);

            if (nvpaStatus)
            {
                return false;
            }

            *pSize = getCounterDataPrefixParams.bytesCopied;

            return true;
        }

        bool GetCounterDataPrefix(size_t bufferSize, uint8_t* pBuffer) const
        {
            if (!m_pCounterDataBuilder)
            {
                return false;
            }

            NVPW_CounterDataBuilder_GetCounterDataPrefix_Params getCounterDataPrefixParams = { NVPW_CounterDataBuilder_GetCounterDataPrefix_Params_STRUCT_SIZE };
            getCounterDataPrefixParams.bytesAllocated = bufferSize;
            getCounterDataPrefixParams.pBuffer = pBuffer;
            getCounterDataPrefixParams.pCounterDataBuilder = m_pCounterDataBuilder;

            NVPA_Status nvpaStatus = NVPW_CounterDataBuilder_GetCounterDataPrefix(&getCounterDataPrefixParams);

            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }
    };

    class RawCounterConfigBuilder
    {
    private:
        NVPW_RawCounterConfig* m_pRawCounterConfig; // owned

    protected:
        void MoveAssign(RawCounterConfigBuilder&& rhs)
        {
            Reset();
            m_pRawCounterConfig = rhs.m_pRawCounterConfig;

            rhs.m_pRawCounterConfig = nullptr;
        }

    public:
        ~RawCounterConfigBuilder()
        {
            Reset();
        }
        RawCounterConfigBuilder() : m_pRawCounterConfig(nullptr)
        {
        }
        RawCounterConfigBuilder(RawCounterConfigBuilder&& rhs) : m_pRawCounterConfig(nullptr)
        {
            MoveAssign(std::forward<RawCounterConfigBuilder>(rhs));
        }
        RawCounterConfigBuilder& operator=(RawCounterConfigBuilder&& rhs)
        {
            MoveAssign(std::forward<RawCounterConfigBuilder>(rhs));
            return *this;
        }

        operator NVPW_RawCounterConfig*() const
        {
            return m_pRawCounterConfig;
        }

        NVPW_RawCounterConfig* Get() const
        {
            return m_pRawCounterConfig;
        }

        // Transfer ownership of pRawCounterConfig
        bool Initialize(NVPW_RawCounterConfig* pRawCounterConfig)
        {
            if (!pRawCounterConfig)
            {
                return false;
            }
            if (m_pRawCounterConfig)
            {
                return false; // already initialized, call Reset() first
            }
            
            m_pRawCounterConfig = pRawCounterConfig;

            return true;
        }

        void Reset()
        {
            if (!m_pRawCounterConfig)
            {
                return;
            }

            NVPW_RawCounterConfig_Destroy_Params destroyParams = { NVPW_RawCounterConfig_Destroy_Params_STRUCT_SIZE };
            destroyParams.pRawCounterConfig = m_pRawCounterConfig;
            NVPW_RawCounterConfig_Destroy(&destroyParams);

            m_pRawCounterConfig = nullptr;
        }

        // Returns an empty vector on error
        std::vector<NVPW_RawCounterDomain> GetAllAvailableSingularCounterDomains() const
        {
            NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains_Params getAllAvailableRawCounterDomainsParams = { NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains_Params_STRUCT_SIZE };
            getAllAvailableRawCounterDomainsParams.pRawCounterConfig = m_pRawCounterConfig;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains(&getAllAvailableRawCounterDomainsParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(10, "NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains() failed!");
                return std::vector<NVPW_RawCounterDomain>();
            }

            std::vector<NVPW_RawCounterDomain> counterDomains(getAllAvailableRawCounterDomainsParams.numAvailableDomains);
            getAllAvailableRawCounterDomainsParams.pAvailableDomains = counterDomains.data();
            nvpaStatus = NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains(&getAllAvailableRawCounterDomainsParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(10, "NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains() failed!");
                return std::vector<NVPW_RawCounterDomain>();
            }
            return counterDomains;
        }

        // Returns an empty vector on error
        std::vector<uint32_t> GetAllAvailableCooperativeDomainGroups() const
        {
            NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups_Params getAllAvailableCooperativeDomainGroupsParams = { NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups_Params_STRUCT_SIZE };
            getAllAvailableCooperativeDomainGroupsParams.pRawCounterConfig = m_pRawCounterConfig;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups(&getAllAvailableCooperativeDomainGroupsParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(10, "NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups() failed!");
                return std::vector<uint32_t>();
            }
            std::vector<uint32_t> cdgs(getAllAvailableCooperativeDomainGroupsParams.numAvailableDomains);
            getAllAvailableCooperativeDomainGroupsParams.pAvailableDomains = cdgs.data();
            nvpaStatus = NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups(&getAllAvailableCooperativeDomainGroupsParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(10, "NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups() failed!");
                return std::vector<uint32_t>();
            }
            return cdgs;
        }

        // The input domain can be either a singular counter domain or a cooperative domain group.
        // TODO: use std::optional once we are allowed to use C++17.
        bool IsCooperativeDomainGroup(uint32_t domain, bool& isCDG) const
        {
            NVPW_RawCounterConfig_IsCooperativeDomainGroup_Params isCooperativeDomainGroupParams = { NVPW_RawCounterConfig_IsCooperativeDomainGroup_Params_STRUCT_SIZE };
            isCooperativeDomainGroupParams.pRawCounterConfig = m_pRawCounterConfig;
            isCooperativeDomainGroupParams.domain = domain;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_IsCooperativeDomainGroup(&isCooperativeDomainGroupParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                return false;
            }
            isCDG = !!isCooperativeDomainGroupParams.isCdg;
            return true;
        }

        // Returns -1 on error. The input domain can be either a singular counter domain or a cooperative domain group.
        size_t GetNumRawCounters(uint32_t counterDomain) const
        {
            NVPW_RawCounterConfig_GetNumRawCounters_Params getNumRawCountersParams = { NVPW_RawCounterConfig_GetNumRawCounters_Params_STRUCT_SIZE };
            getNumRawCountersParams.pRawCounterConfig = m_pRawCounterConfig;
            getNumRawCountersParams.domain = counterDomain;
            const NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetNumRawCounters(&getNumRawCountersParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_RawCounterConfig_GetNumRawCounters() failed! domain = %s", RawCounterDomainToCStr(counterDomain));
                return size_t(-1);
            }
            return getNumRawCountersParams.numRawCounters;
        }

        // Returns an empty string on error. The input domain can be either a singular counter domain or a cooperative domain group.
        const char* GetRawCounterName(uint32_t counterDomain, size_t rawCounterIndex) const
        {
            NVPW_RawCounterConfig_GetRawCounterName_Params getRawCounterNameParams = { NVPW_RawCounterConfig_GetRawCounterName_Params_STRUCT_SIZE };
            getRawCounterNameParams.pRawCounterConfig = m_pRawCounterConfig;
            getRawCounterNameParams.domain = counterDomain;
            getRawCounterNameParams.rawCounterIndex = rawCounterIndex;
            const NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetRawCounterName(&getRawCounterNameParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(80, "NVPW_RawCounterConfig_GetRawCounterName() failed!");
                return "";
            }
            return getRawCounterNameParams.pRawCounterName;
        }

        // Returns an empty vector on error. The returned domains include both singular counter domains and cooperative domain groups.
        std::vector<uint32_t> GetCounterSupportedCounterDomains(const char* pRawCounterName) const
        {
            NVPW_RawCounterConfig_GetRawCounterProperties_Params getRawCounterPropertiesParams = { NVPW_RawCounterConfig_GetRawCounterProperties_Params_STRUCT_SIZE };
            getRawCounterPropertiesParams.pRawCounterConfig = m_pRawCounterConfig;
            getRawCounterPropertiesParams.pRawCounterName = pRawCounterName;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetRawCounterProperties(&getRawCounterPropertiesParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(80, "NVPW_RawCounterConfig_GetRawCounterProperties() failed!");
                return {};
            }
            std::vector<NVPW_RawCounterDomain> supportedScds(getRawCounterPropertiesParams.numSupportedDomains);
            std::vector<uint32_t> supportedCdgs(getRawCounterPropertiesParams.numSupportedCdgDomains);
            getRawCounterPropertiesParams.pSupportedDomains = supportedScds.data();
            getRawCounterPropertiesParams.pSupportedCdgDomains = supportedCdgs.data();
            nvpaStatus = NVPW_RawCounterConfig_GetRawCounterProperties(&getRawCounterPropertiesParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(80, "NVPW_RawCounterConfig_GetRawCounterProperties() failed!");
                return {};
            }

            std::vector<uint32_t> supportedCounterDomains;
            supportedCounterDomains.insert(supportedCounterDomains.end(), supportedScds.begin(), supportedScds.end());
            supportedCounterDomains.insert(supportedCounterDomains.end(), supportedCdgs.begin(), supportedCdgs.end());
            return supportedCounterDomains;
        }

        // Input domains must be singular counter domains. Cooperative domain groups as logical domains are considered in pass groups when all member domains are in the pass group.
        bool BeginPassGroups(const NVPW_RawCounterDomain* pDomains, size_t numDomains)
        {
            if (!m_pRawCounterConfig)
            {
                return false;
            }

            NVPW_RawCounterConfig_BeginPassGroup_Params beginPassGroupParams = { NVPW_RawCounterConfig_BeginPassGroup_Params_STRUCT_SIZE };
            beginPassGroupParams.pRawCounterConfig = m_pRawCounterConfig;
            beginPassGroupParams.numDomains = numDomains;
            beginPassGroupParams.pDomains = pDomains;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_BeginPassGroup(&beginPassGroupParams);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        bool BeginPassGroupsAll()
        {
            const std::vector<NVPW_RawCounterDomain> allScds = GetAllAvailableSingularCounterDomains();
            if (allScds.empty())
            {
                return false;
            }
            return BeginPassGroups(allScds.data(), allScds.size());
        }

        // Input domains must be singular counter domains. Cooperative domain groups as logical domains are considered not in pass groups when any member domain is not in the pass group.
        bool EndPassGroups(const NVPW_RawCounterDomain* pDomains, size_t numDomains)
        {
            if (!m_pRawCounterConfig)
            {
                return false;
            }

            NVPW_RawCounterConfig_EndPassGroup_Params endPassGroupParams = { NVPW_RawCounterConfig_EndPassGroup_Params_STRUCT_SIZE };
            endPassGroupParams.pRawCounterConfig = m_pRawCounterConfig;
            endPassGroupParams.numDomains = numDomains;
            endPassGroupParams.pDomains = pDomains;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_EndPassGroup(&endPassGroupParams);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        bool EndPassGroupsAll()
        {
            const std::vector<NVPW_RawCounterDomain> allScds = GetAllAvailableSingularCounterDomains();
            if (allScds.empty())
            {
                return false;
            }
            return EndPassGroups(allScds.data(), allScds.size());
        }

        bool AddRawCounter(const NVPW_RawCounterRequest& rawCounterRequest)
        {
            if (!m_pRawCounterConfig)
            {
                return false;
            }

            NVPW_RawCounterConfig_AddRawCounters_Params addRawCounterParams = { NVPW_RawCounterConfig_AddRawCounters_Params_STRUCT_SIZE };
            addRawCounterParams.pRawCounterConfig = m_pRawCounterConfig;
            addRawCounterParams.rawCounterRequestStructSize = NVPW_RAW_COUNTER_REQUEST_STRUCT_SIZE;
            addRawCounterParams.numRawCounterRequests = 1;
            addRawCounterParams.pRawCounterRequests = &rawCounterRequest;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_AddRawCounters(&addRawCounterParams);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        bool AddRawCounters(size_t count, NVPW_RawCounterRequest* pRequests)
        {
            if (!m_pRawCounterConfig)
            {
                return false;
            }

            NVPW_RawCounterConfig_AddRawCounters_Params addRawCounterParams = { NVPW_RawCounterConfig_AddRawCounters_Params_STRUCT_SIZE };
            addRawCounterParams.pRawCounterConfig = m_pRawCounterConfig;
            addRawCounterParams.rawCounterRequestStructSize = NVPW_RAW_COUNTER_REQUEST_STRUCT_SIZE;
            addRawCounterParams.numRawCounterRequests = count;
            addRawCounterParams.pRawCounterRequests = pRequests;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_AddRawCounters(&addRawCounterParams);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        bool GenerateConfigImage()
        {
            if (!m_pRawCounterConfig)
            {
                return false;
            }

            NVPW_RawCounterConfig_GenerateConfigImage_Params generateConfigImageParams = { NVPW_RawCounterConfig_GenerateConfigImage_Params_STRUCT_SIZE };
            generateConfigImageParams.pRawCounterConfig = m_pRawCounterConfig;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GenerateConfigImage(&generateConfigImageParams);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        // Returns zero on error
        size_t GetConfigImageSize() const
        {
            if (!m_pRawCounterConfig)
            {
                return 0;
            }

            NVPW_RawCounterConfig_GetConfigImage_Params getConfigImageParams = { NVPW_RawCounterConfig_GetConfigImage_Params_STRUCT_SIZE };
            getConfigImageParams.pRawCounterConfig = m_pRawCounterConfig;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetConfigImage(&getConfigImageParams);
            if (nvpaStatus)
            {
                return 0;
            }
            return getConfigImageParams.bytesCopied;
        }

        bool GetConfigImage(size_t bufferSize, uint8_t* pBuffer) const
        {
            if (!m_pRawCounterConfig)
            {
                return false;
            }

            NVPW_RawCounterConfig_GetConfigImage_Params getConfigImageParams = { NVPW_RawCounterConfig_GetConfigImage_Params_STRUCT_SIZE };
            getConfigImageParams.pRawCounterConfig = m_pRawCounterConfig;
            getConfigImageParams.bytesAllocated = bufferSize;
            getConfigImageParams.pBuffer = pBuffer;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetConfigImage(&getConfigImageParams);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        size_t GetNumPasses() const
        {
            if (!m_pRawCounterConfig)
            {
                return 0;
            }

            NVPW_RawCounterConfig_GetNumPasses_Params getNumPassesParams = { NVPW_RawCounterConfig_GetNumPasses_Params_STRUCT_SIZE };
            getNumPassesParams.pRawCounterConfig = m_pRawCounterConfig;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_GetNumPasses(&getNumPassesParams);
            if (nvpaStatus)
            {
                return 0;
            }
            return getNumPassesParams.numPasses;
        }

        // Retruns "" on error. The input domain can be either a singular counter domain or a cooperative domain group.
        static const char* RawCounterDomainToCStr(uint32_t rawCounterDomain)
        {
            NVPW_RawCounterConfig_RawCounterDomainToString_Params rawCounterDomainToStringParams = { NVPW_RawCounterConfig_RawCounterDomainToString_Params_STRUCT_SIZE };
            rawCounterDomainToStringParams.domain = rawCounterDomain;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_RawCounterDomainToString(&rawCounterDomainToStringParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(80, "NVPW_RawCounterConfig_RawCounterDomainToString failed! domain = %u\n", rawCounterDomain);
                return "";
            }
            return rawCounterDomainToStringParams.pDomainName;
        }

        // Returns "NVPW_RAW_COUNTER_DOMAIN_INVALID" on error
        static NVPW_RawCounterDomain ToRawCounterDomain(const char* pDomainName)
        {
            NVPW_RawCounterConfig_StringToRawCounterDomain_Params stringToRawCounterDomainParams = { NVPW_RawCounterConfig_StringToRawCounterDomain_Params_STRUCT_SIZE };
            stringToRawCounterDomainParams.pDomainName = pDomainName;
            NVPA_Status nvpaStatus = NVPW_RawCounterConfig_StringToRawCounterDomain(&stringToRawCounterDomainParams);
            if (nvpaStatus)
            {
                return NVPW_RAW_COUNTER_DOMAIN_INVALID;
            }
            return static_cast<NVPW_RawCounterDomain>(stringToRawCounterDomainParams.domain);
        }
    };

    class MetricsConfigBuilder
    {
    protected:
        NVPW_MetricsEvaluator* m_pMetricsEvaluator;         // not owned
        CounterDataBuilder m_counterDataBuilder;            // owned
        RawCounterConfigBuilder m_rawCounterConfigBuilder;  // owned

    protected:
        void MoveAssign(MetricsConfigBuilder&& rhs)
        {
            Reset();
            m_pMetricsEvaluator = rhs.m_pMetricsEvaluator;
            m_counterDataBuilder = std::move(rhs.m_counterDataBuilder);
            m_rawCounterConfigBuilder = std::move(rhs.m_rawCounterConfigBuilder);

            rhs.m_pMetricsEvaluator = nullptr;
        }

    public:
        ~MetricsConfigBuilder()
        {
            Reset();
        }
        MetricsConfigBuilder()
            : m_pMetricsEvaluator()
        {
        }
        MetricsConfigBuilder(MetricsConfigBuilder&& rhs)
            : m_pMetricsEvaluator()
        {
            MoveAssign(std::forward<MetricsConfigBuilder>(rhs));
        }
        MetricsConfigBuilder& operator=(MetricsConfigBuilder&& rhs)
        {
            MoveAssign(std::forward<MetricsConfigBuilder>(rhs));
            return *this;
        }

        CounterDataBuilder& GetCounterDataBuilder()
        {
            return m_counterDataBuilder;
        }

        const CounterDataBuilder& GetCounterDataBuilder() const
        {
            return m_counterDataBuilder;
        }

        RawCounterConfigBuilder& GetRawCounterConfigBuilder()
        {
            return m_rawCounterConfigBuilder;
        }

        const RawCounterConfigBuilder& GetRawCounterConfigBuilder() const
        {
            return m_rawCounterConfigBuilder;
        }

        void Reset()
        {
            m_rawCounterConfigBuilder.Reset();
            m_counterDataBuilder.Reset();

            m_pMetricsEvaluator = nullptr;
        }

        bool Initialize(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_RawCounterConfig* pRawCounterConfig, const char* pChipName)
        {
            Reset(); // destroy any existing objects
            m_pMetricsEvaluator = pMetricsEvaluator;

            if (!m_counterDataBuilder.Initialize(pChipName))
            {
                return false;
            }

            if (!m_rawCounterConfigBuilder.Initialize(pRawCounterConfig))
            {
                return false;
            }
            if (!m_rawCounterConfigBuilder.BeginPassGroupsAll())
            {
                return false;
            }

            return true;
        }

        // If the raw counter appears in "rawCounterSchedulingHints", it will be scheduled in the domain specified by the hint;
        // otherwise, it will be scheduled to the default domain.
        bool AddMetrics(
            const NVPW_MetricEvalRequest* pMetricEvalRequests,
            size_t numMetricEvalRequests,
            bool keepInstances = true,
            const RawCounterSchedulingHints& rawCounterSchedulingHints = {})
        {
            NVPA_Status nvpaStatus = NVPA_STATUS_SUCCESS;
            NVPW_MetricsEvaluator_GetMetricRawDependencies_Params getMetricRawDependenciesParams = { NVPW_MetricsEvaluator_GetMetricRawDependencies_Params_STRUCT_SIZE };
            getMetricRawDependenciesParams.pMetricsEvaluator = m_pMetricsEvaluator;
            getMetricRawDependenciesParams.pMetricEvalRequests = pMetricEvalRequests;
            getMetricRawDependenciesParams.numMetricEvalRequests = numMetricEvalRequests;
            getMetricRawDependenciesParams.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
            getMetricRawDependenciesParams.metricEvalRequestStrideSize = sizeof(NVPW_MetricEvalRequest);
            nvpaStatus = NVPW_MetricsEvaluator_GetMetricRawDependencies(&getMetricRawDependenciesParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(50, "NVPW_MetricsEvaluator_GetMetricRawDependencies failed\n");
                return false;
            }

            std::vector<const char*> rawCounterDependencies(getMetricRawDependenciesParams.numRawDependencies);
            std::vector<const char*> optionalRawCounterDependencies(getMetricRawDependenciesParams.numOptionalRawDependencies);
            getMetricRawDependenciesParams.ppRawDependencies = rawCounterDependencies.data();
            getMetricRawDependenciesParams.ppOptionalRawDependencies = optionalRawCounterDependencies.data();
            nvpaStatus = NVPW_MetricsEvaluator_GetMetricRawDependencies(&getMetricRawDependenciesParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(50, "NVPW_MetricsEvaluator_GetMetricRawDependencies failed\n");
                return false;
            }

            auto addRawCounter = [&](const char* const pRawCounterName, bool emitError) {
                NVPW_RawCounterRequest rawCounterRequest = {};
                rawCounterRequest.pRawCounterName = pRawCounterName;
                rawCounterRequest.keepInstances = keepInstances;
                rawCounterRequest.domain = NVPW_RAW_COUNTER_DOMAIN_INVALID; // default domain
                if (rawCounterSchedulingHints.find(pRawCounterName) != rawCounterSchedulingHints.end())
                {
                    rawCounterRequest.domain = rawCounterSchedulingHints.at(pRawCounterName);
                }

                if (!m_counterDataBuilder.AddRawCounters(1, &rawCounterRequest))
                {
                    const char* pString = "NVPW_CounterDataBuilder_AddRawCounters failed for raw counter: %s\n";
                    if (emitError)
                    {
                        NV_PERF_LOG_ERR(50, pString, pRawCounterName);
                    }
                    else
                    {
                        NV_PERF_LOG_WRN(50, pString, pRawCounterName);
                    }
                    return false;
                }

                if (!m_rawCounterConfigBuilder.AddRawCounter(rawCounterRequest))
                {
                    const char* pString = "NVPW_RawCounterConfig_AddRawCounters failed for raw counter: %s\n";
                    if (emitError)
                    {
                        NV_PERF_LOG_ERR(50, pString, pRawCounterName);
                    }
                    else
                    {
                        NV_PERF_LOG_WRN(50, pString, pRawCounterName);
                    }
                    return false;
                }
                return true;
            };

            for (const char* const pRawCounterName : rawCounterDependencies)
            {
                if (!addRawCounter(pRawCounterName, true))
                {
                    return false;
                }
            }
            for (const char* const pRawCounterName : optionalRawCounterDependencies)
            {
                addRawCounter(pRawCounterName, false); // fail in scheduling an optional raw counter may not be considered a failure
            }

            return true;
        }

        bool AddMetric(const char* pMetricName)
        {
            NVPW_MetricEvalRequest metricEvalRequest{};
            if (!ToMetricEvalRequest(m_pMetricsEvaluator, pMetricName, metricEvalRequest))
            {
                NV_PERF_LOG_ERR(50, "ToMetricEvalRequest failed for metric: %s\n", pMetricName);
                return false;
            }
            if (!AddMetrics(&metricEvalRequest, 1))
            {
                NV_PERF_LOG_ERR(50, "AddMetrics failed for metric: %s\n", pMetricName);
                return false;
            }
            return true;
        }

        bool AddMetrics(const char* const pMetricNames[], size_t numMetrics)
        {
            bool success = true;
            for (size_t metricIdx = 0; metricIdx < numMetrics; ++metricIdx)
            {
                const bool addMetricSuccess = AddMetric(pMetricNames[metricIdx]);
                if (!addMetricSuccess)
                {
                    success = false;
                }
            }
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool PrepareConfigImage()
        {
            if (!m_rawCounterConfigBuilder.EndPassGroupsAll())
            {
                return false;
            }

            if (!m_rawCounterConfigBuilder.GenerateConfigImage())
            {
                return false;
            }

            // Start a new PassGroup so that subsequent AddMetrics() calls will succeed.
            // This will not result in optimal scheduling, but it obeys the principle of least surprise.
            if (!m_rawCounterConfigBuilder.BeginPassGroupsAll())
            {
                return false;
            }

            return true;
        }

        // Returns the buffer size needed for the ConfigImage, or zero on error.
        size_t GetConfigImageSize() const
        {
            return m_rawCounterConfigBuilder.GetConfigImageSize();
        }

        // Copies the generated ConfigImage into pBuffer.
        bool GetConfigImage(size_t bufferSize, uint8_t* pBuffer) const
        {
            return m_rawCounterConfigBuilder.GetConfigImage(bufferSize, pBuffer);
        }

        // Returns the buffer size needed for the CounterDataPrefix, or zero on error.
        size_t GetCounterDataPrefixSize() const
        {
            size_t size = 0;

            if (!m_counterDataBuilder.GetCounterDataPrefixSize(&size))
            {
                return (size_t)0;
            }

            return size;
        }

        // Copies the generated CounterDataPrefix into pBuffer.
        bool GetCounterDataPrefix(size_t bufferSize, uint8_t* pBuffer) const
        {
            return m_counterDataBuilder.GetCounterDataPrefix(bufferSize, pBuffer);
        }

        // Returns the number of replay passes for this metrics config.
        size_t GetNumPasses() const
        {
            return m_rawCounterConfigBuilder.GetNumPasses();
        }
    };
}}
