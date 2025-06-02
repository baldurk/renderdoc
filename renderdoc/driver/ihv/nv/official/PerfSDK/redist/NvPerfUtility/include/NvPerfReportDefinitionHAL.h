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

#include <string.h>
#include "NvPerfInit.h"
#include "NvPerfReportDefinition.h"
#include "NvPerfReportDefinitionTU10X.h"
#include "NvPerfReportDefinitionTU11X.h"
#include "NvPerfReportDefinitionGA10X.h"
#include "NvPerfReportDefinitionGA10B.h"
#include "NvPerfReportDefinitionAD10X.h"
#include "NvPerfReportDefinitionGB10B.h"
#include "NvPerfReportDefinitionGB20X.h"

namespace nv { namespace perf {

    namespace PerRangeReport {

        inline ReportDefinition GetReportDefinition(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::PerRangeReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu11x::PerRangeReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") ||!strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::PerRangeReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10b::PerRangeReport::GetReportDefinition();
            }
            else if (false
                     || !strcmp(pChipName, "AD102")
                     || !strcmp(pChipName, "AD103")
                     || !strcmp(pChipName, "AD104")
                     || !strcmp(pChipName, "AD106")
                     || !strcmp(pChipName, "AD107")
            ) 
            {
                return ad10x::PerRangeReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "GB10B"))
            {
                return gb10b::PerRangeReport::GetReportDefinition();
            }
            else if (false
                     || !strcmp(pChipName, "GB202")
                     || !strcmp(pChipName, "GB203")
                     || !strcmp(pChipName, "GB205")
                     || !strcmp(pChipName, "GB206")
            )
            {
                return gb20x::PerRangeReport::GetReportDefinition();
            }        
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return {};
            }
        }

    } // namespace PerRangeReport

    namespace SummaryReport {

        inline ReportDefinition GetReportDefinition(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::SummaryReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu11x::SummaryReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") ||!strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::SummaryReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10b::SummaryReport::GetReportDefinition();
            }
            else if (false
                     || !strcmp(pChipName, "AD102")
                     || !strcmp(pChipName, "AD103")
                     || !strcmp(pChipName, "AD104")
                     || !strcmp(pChipName, "AD106")
                     || !strcmp(pChipName, "AD107")
            )
            {
                return ad10x::SummaryReport::GetReportDefinition();
            }
            else if (!strcmp(pChipName, "GB10B"))
            {
                return gb10b::SummaryReport::GetReportDefinition();
            }
            else if (false
                     || !strcmp(pChipName, "GB202")
                     || !strcmp(pChipName, "GB203")
                     || !strcmp(pChipName, "GB205")
                     || !strcmp(pChipName, "GB206")
            )
            {
                return gb20x::SummaryReport::GetReportDefinition();
            }        
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return {};
            }
        }

    } // namespace SummaryReport

} }
