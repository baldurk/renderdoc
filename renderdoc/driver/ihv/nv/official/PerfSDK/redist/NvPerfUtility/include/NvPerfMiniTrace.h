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
#include <ryml_all.hpp>
#include <string>
#include <stdint.h>
namespace nv { namespace perf { namespace mini_trace {
    struct APITraceData
    {
        std::string name;
        uint64_t startTimestamp = 0;
        uint64_t endTimestamp = 0;
        size_t nestingLevel = 0;
    };
    inline std::string SerializeAPITraceDataToYaml(const std::vector<APITraceData> &data)
    {
        ryml::Tree tree;
        ryml::NodeRef root = tree.rootref();
        root |= ryml::MAP;
        ryml::NodeRef traces = root["traces"];
        traces |= ryml::SEQ;
        for (size_t index = 0; index < data.size(); ++index)
        {
            ryml::NodeRef node = traces.append_child();
            node |= ryml::MAP;
            node["name"] << data[index].name;
            node["start"] << data[index].startTimestamp;
            node["end"] << data[index].endTimestamp;
            node["nestingLevel"] << data[index].nestingLevel;
        }
        return ryml::emitrs<std::string>(tree);
    }
    template<typename ValueType>
    bool LoadFromYamlNode(ryml::NodeRef node, ryml::csubstr key, ValueType& value)
    {
        ryml::NodeRef child = node.find_child(key);
        if (child.valid() && child.is_keyval())
        {
            child >> value;
            return true;
        }
        return false;
    }

    inline std::vector<APITraceData> DeserializeAPITraceDataFromYaml(const std::string &yaml)
    {
        std::vector<APITraceData> data;
        std::vector<APITraceData> emptyData;
        ryml::Tree tree = ryml::parse_in_arena(ryml::csubstr(yaml.data(), yaml.size()));

        ryml::NodeRef root = tree.rootref();
        if (!root.valid() || !root.is_map())
        {
            NV_PERF_LOG_ERR(10, "Failed to parse yaml\n");
            return emptyData;
        }

        ryml::NodeRef traces = root.find_child("traces");
        if (!traces.valid() || !traces.is_seq())
        {
            NV_PERF_LOG_ERR(10, "Sequence \"traces\" is not found in the yaml\n");
            return emptyData;
        }
        for (ryml::NodeRef node : traces.children())
        {
            APITraceData trace;
            if (!LoadFromYamlNode(node, "name", trace.name))
            {
                NV_PERF_LOG_ERR(10, "Failed to parse \"name\" in the yaml\n");
                return emptyData;
            }
            if (!LoadFromYamlNode(node, "start", trace.startTimestamp))
            {
                NV_PERF_LOG_ERR(10, "Failed to parse \"start\" in the yaml\n");
                return emptyData;
            }
            
            if (!LoadFromYamlNode(node, "end", trace.endTimestamp))
            {
                NV_PERF_LOG_ERR(10, "Failed to parse \"end\" in the yaml\n");
                return emptyData;
            }
            
            if (!LoadFromYamlNode(node, "nestingLevel", trace.nestingLevel))
            {
                NV_PERF_LOG_ERR(10, "Failed to parse \"nestingLevel\" in the yaml\n");
                return emptyData;
            }

            data.emplace_back(std::move(trace));
        }
        return data;
    }
}}} // namespace nv::perf::mini_trace