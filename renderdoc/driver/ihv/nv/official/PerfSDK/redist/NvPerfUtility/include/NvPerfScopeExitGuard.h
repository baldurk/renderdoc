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

#include <utility>

namespace nv { namespace perf {

    /// Stores the lambda to be executed at scope-exit by-value.
    template <class TFn>
    class ScopeExitGuardObject
    {
        TFn m_fn;
        bool m_active;

    private:
        ScopeExitGuardObject(const ScopeExitGuardObject<TFn>&);
        ScopeExitGuardObject<TFn>& operator=(const ScopeExitGuardObject<TFn>&);

    public:
        ~ScopeExitGuardObject()
        {
            if (m_active)
            {
                m_fn();
            }
        }
        ScopeExitGuardObject(TFn&& fn)
            : m_fn(std::move(fn))
            , m_active(true)
        {
        }
        ScopeExitGuardObject(ScopeExitGuardObject<TFn>&& rhs)
            : m_fn(std::move(rhs.m_fn))
            , m_active(rhs.m_active)
        {
            rhs.m_active = false;
        }
        void Dismiss()
        {
            m_active = false;
        }
    };

    template <class TFn>
    ScopeExitGuardObject<TFn> ScopeExitGuard(TFn&& fn)
    {
        return ScopeExitGuardObject<TFn>(std::move(fn));
    }

}}
