/*
* Copyright 2021-2025 NVIDIA Corporation.  All rights reserved.
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

#include <cstring>

#include "NvPerfInit.h"
#include "NvPerfHudConfigurationsTU10X.h"
#include "NvPerfHudConfigurationsTU11X.h"
#include "NvPerfHudConfigurationsGA10X.h"
#include "NvPerfHudConfigurationsGA10B.h"
#include "NvPerfHudConfigurationsAD10X.h"
#include "NvPerfHudConfigurationsGB10B.h"
#include "NvPerfHudConfigurationsGB20X.h"

namespace nv { namespace perf { namespace hud {

    namespace HudConfigurations {

        inline size_t GetHudConfigurationsSize(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::HudConfigurations::GetHudConfigurationsSize();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu11x::HudConfigurations::GetHudConfigurationsSize();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") || !strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::HudConfigurations::GetHudConfigurationsSize();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10b::HudConfigurations::GetHudConfigurationsSize();
            }
            else if (false
                     || !strcmp(pChipName, "AD102")
                     || !strcmp(pChipName, "AD103")
                     || !strcmp(pChipName, "AD104")
                     || !strcmp(pChipName, "AD106")
                     || !strcmp(pChipName, "AD107")
                )
            {
                return ad10x::HudConfigurations::GetHudConfigurationsSize();
            }
            else if (false
                || !strcmp(pChipName, "GB10B")
                )
            {
                return gb10b::HudConfigurations::GetHudConfigurationsSize();
            }
            else if (false
                || !strcmp(pChipName, "GB202")
                || !strcmp(pChipName, "GB203")
                || !strcmp(pChipName, "GB205")
                || !strcmp(pChipName, "GB206")
                )
            {
                return gb20x::HudConfigurations::GetHudConfigurationsSize();
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return 0;
            }
        }

        inline const char** GetHudConfigurationsFileNames(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::HudConfigurations::GetHudConfigurationsFileNames();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu11x::HudConfigurations::GetHudConfigurationsFileNames();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") ||!strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::HudConfigurations::GetHudConfigurationsFileNames();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10b::HudConfigurations::GetHudConfigurationsFileNames();
            }
            else if (false
                     || !strcmp(pChipName, "AD102")
                     || !strcmp(pChipName, "AD103")
                     || !strcmp(pChipName, "AD104")
                     || !strcmp(pChipName, "AD106")
                     || !strcmp(pChipName, "AD107")
                )
            {
                return ad10x::HudConfigurations::GetHudConfigurationsFileNames();
            }
            else if (false
                || !strcmp(pChipName, "GB10B")
                )
            {
                return gb10b::HudConfigurations::GetHudConfigurationsFileNames();
            }
            else if (false
                || !strcmp(pChipName, "GB202")
                || !strcmp(pChipName, "GB203")
                || !strcmp(pChipName, "GB205")
                || !strcmp(pChipName, "GB206")
                )
            {
                return gb20x::HudConfigurations::GetHudConfigurationsFileNames();
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return nullptr;
            }
        }

        inline const char** GetHudConfigurations(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::HudConfigurations::GetHudConfigurations();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu11x::HudConfigurations::GetHudConfigurations();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") || !strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::HudConfigurations::GetHudConfigurations();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10b::HudConfigurations::GetHudConfigurations();
            }
            else if (false
                     || !strcmp(pChipName, "AD102")
                     || !strcmp(pChipName, "AD103")
                     || !strcmp(pChipName, "AD104")
                     || !strcmp(pChipName, "AD106")
                     || !strcmp(pChipName, "AD107")
                )
            {
                return ad10x::HudConfigurations::GetHudConfigurations();
            }
            else if (false
                || !strcmp(pChipName, "GB10B")
                )
            {
                return gb10b::HudConfigurations::GetHudConfigurations();
            }
            else if (false
                || !strcmp(pChipName, "GB202")
                || !strcmp(pChipName, "GB203")
                || !strcmp(pChipName, "GB205")
                || !strcmp(pChipName, "GB206")
                )
            {
                return gb20x::HudConfigurations::GetHudConfigurations();
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return nullptr;
            }
        }

    } // namespace HudConfigurations

} } }
