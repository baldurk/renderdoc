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

#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define METRICS_CONFIG_VERSION_MAJOR 1
#define METRICS_CONFIG_VERSION_MINOR 0
#define METRICS_CONFIG_VERSION_PATCH 0

namespace nv { namespace perf {

    struct DomainScheduleHints
    {
        struct Hint
        {
            std::string rawCounterName;
        };
        std::string domain;
        std::vector<Hint> hints;
    };

    namespace detail {

        // version: <version>         # semantic version, ie: major.minor.patch
        // gpu: <gpu litter>          # just for DFD, files are already GPU versioned
        // userMetrics:               # type:list - format
        //   - <namespace.newMetricName>:  # type:map - <newMetricName> is user metric name
        //     counter:
        //       expr:
        //       sustainedRate:       # optional <str> is a sustained rate expression, converted to user metrics as: <newMetricName>:SetSustainedRate(<str>)
        //     ratio:
        //       expr:
        //       maxRate: <str>
        //     throughput: [ <subCounter>, <subThroughput> ] # type:list - at least 1 item is required, can be any combination
        //     constant:
        //       val:
        //       hwunit:
        //       dimunits:
        //     conversionFactor:
        //       multiplier:
        //       dimunits:
        //     description: <str>      # optional. <str> is a string description of the metric, converted to user metrics as: <newMetricName>:SetDescription(<str>)
        //     additionalScript: <src> # <src> is a user metrics formatted script that is concatenated before other fields to the final script.
        //
        // metricGroups:              # type:list
        //   - <groupName>:           # type:map
        //     metrics:               # type:list
        //       - <metricName>       # <metricName> must be a name in the Perfworks metrics DB
        //     schedulingHints:       # type:list
        //       - <domain>:          # type:map - <domain> must be a RawCounterDomain SCD or CDG
        //         - <rawCounterName>
        //     userMetrics:           # type:list - format same as userMetrics above, these are userMetric definitions specific to a metricGroup
        //
        // triageGroup:
        //   name: <triageGroupName>  # for DFD - should match the file name
        //   groups:                  # Optional: metric groups in all variations
        //     - <metricGroupName>    # <metricGroupName> must be a name from metricGroups
        //   variations:              # Optional: type: list
        //     - <variationName>:     # <variationName> is name of variation
        //       groups:              # metric groups in this variation
        //         - <metricGroupName> # <metricGroupName> is name of a group from metricGroups
        //       predicate: {         # type: map - group is selected if all predicates are true
        //      }

        enum class UserMetricType
        {
            Counter,
            Ratio,
            Throughput,
            Constant,
            ConversionFactor
        };

        struct UserMetricBase
        {
            UserMetricType type;
            std::string name;
            std::string description;
            std::string additionalScript;

            virtual ~UserMetricBase() = default;

            virtual std::string ToScript() const
            {
                std::stringstream ss;
                if (!additionalScript.empty())
                {
                    ss << additionalScript << "\n";
                }
                return ss.str(); 
            }
        };

        struct UserMetricCounter : public UserMetricBase
        {
            std::string expr;
            std::string sustainedRate; // make it a string so that it can accept an expression. This will be more future proof than a double.

            virtual ~UserMetricCounter() override = default;

            virtual std::string ToScript() const override
            {
                std::stringstream ss;
                if (description.empty())
                {
                    // Specially handle this so that the assignee can inheirt the description from the counter in the expression.
                    // This is useful in aliasing 'Namespace.A = CounterMetric(A)', where you don't want to repeat explicitly setting the description.
                    // If the RHS is a anoyomous counter("Namespace.A = B + C"), then the RHS will have no description, so this is effectively
                    // the same as 'Namespace.A = CounterMetric(A, "")', which is fine.
                    ss << name << " = CounterMetric(" << expr << ")\n";
                }
                else
                {
                    ss << name << " = CounterMetric(" << expr << ", \"" << description << "\")\n";
                }
                if (!sustainedRate.empty())
                {
                    ss << name << ":SetSustainedRate(" << sustainedRate << ")\n";
                }
                ss << UserMetricBase::ToScript();
                return ss.str();
            }
        };

        struct UserMetricRatio : public UserMetricBase
        {
            std::string expr;
            std::string maxRate; // make it a string so that it can accept an expression(e.g. "128*1024")

            virtual ~UserMetricRatio() override = default;

            virtual std::string ToScript() const override
            {
                std::stringstream ss;
                ss << name << " = RatioMetric(" << expr << ", " << maxRate << ", \"" << description << "\")\n";
                ss << UserMetricBase::ToScript();
                return ss.str();
            }
        };

        struct UserThroughputMetric : public UserMetricBase
        {
            std::vector<std::string> countersOrSubThroughputs;

            virtual ~UserThroughputMetric() override = default;

            virtual std::string ToScript() const override
            {
                std::stringstream ss;
                ss << name << " = ThroughputMetric({ ";
                for (size_t ii = 0; ii < countersOrSubThroughputs.size(); ++ii)
                {
                    if (ii)
                    {
                        ss << ", ";
                    }
                    ss << countersOrSubThroughputs[ii];
                }
                ss << " }, \"" << description << "\")\n";
                ss << UserMetricBase::ToScript();
                return ss.str();
            }
        };

        struct UserConstantMetric : public UserMetricBase
        {
            std::string val;
            std::string hwunit;
            std::string dimunits;

            virtual ~UserConstantMetric() override = default;

            virtual std::string ToScript() const override
            {
                std::stringstream ss;
                ss << name << " = ConstantCounterMetric(" << hwunit << ", " << val << ", " << dimunits << ")\n";
                ss << UserMetricBase::ToScript();
                return ss.str();
            }
        };

        struct UserConversionFactorMetric : public UserMetricBase
        {
            std::string multiplier;
            std::string dimunits;

            virtual ~UserConversionFactorMetric() override = default;
            virtual std::string ToScript() const override
            {
                std::stringstream ss;
                ss << name << " = ConversionFactorMetric(" << multiplier << ", " << dimunits << ")\n";
                ss << UserMetricBase::ToScript();
                return ss.str();
            }
        };

        struct MetricGroup
        {
            std::string groupName;
            std::vector<std::string> metrics;
            std::vector<DomainScheduleHints> schedulingHints;
            std::vector<std::unique_ptr<UserMetricBase>> userMetrics;
        };

        struct TriageGroup
        {
            struct Variation
            {
                std::string name;
                std::vector<const MetricGroup*> subGroups;
                std::unordered_map<std::string, std::string> predicates;
            };

            std::string name;
            std::vector<const MetricGroup*> commonSubGroups;
            std::vector<Variation> variations;
        };

        struct MetricConfigObjectData
        {
            uint8_t versionMajor;
            uint8_t versionMinor;
            uint8_t versionPatch;
            std::string gpu;
            std::vector<std::unique_ptr<UserMetricBase>> globalUserMetrics;
            std::vector<MetricGroup> metricGroups;
            TriageGroup triageGroup;
        };

        // Helper function to safely parse YAML without throwing exceptions
        template <typename ParseFunc, typename... Args>
        bool SafeParse(ParseFunc parseFunc, Args&&... args)
        {
            try
            {
                return parseFunc(std::forward<Args>(args)...);
            }
            catch (const std::exception& e)
            {
                NV_PERF_LOG_ERR(20, "Parsing error: %s\n", e.what());
                return false;
            }
            catch (...)
            {
                NV_PERF_LOG_ERR(20, "Unknown parsing error\n");
                return false;
            }
        }

        template <typename TNode>
        inline std::string NodeToString(const TNode& node)
        {
            std::ostringstream ss;
            ss << node;
            return ss.str();
        }

        inline bool ParseUserMetric(const ryml::NodeRef& userMetricNode, std::unique_ptr<UserMetricBase>& pUserMetric)
        {
            if (!userMetricNode.valid())
            {
                return false;
            }

            if (!userMetricNode.is_map())
            {
                NV_PERF_LOG_ERR(20, "User metric must be a map: %s\n", NodeToString(userMetricNode).c_str());
                return false;
            }

            if (!userMetricNode.has_key())
            {
                NV_PERF_LOG_ERR(20, "User metric node doesn't have a valid key: %s\n", NodeToString(userMetricNode).c_str());
                return false;
            }
            const std::string metricName = NodeToString(userMetricNode.key());
            const auto counterNode = userMetricNode.find_child("counter");
            const auto ratioNode = userMetricNode.find_child("ratio");
            const auto throughputNode = userMetricNode.find_child("throughput");
            const auto constantNode = userMetricNode.find_child("constant");
            const auto conversionFactorNode = userMetricNode.find_child("conversionFactor");

            {
                size_t numValidMetricTypes = 0;
                numValidMetricTypes += counterNode.valid() ?            1 : 0;
                numValidMetricTypes += ratioNode.valid() ?              1 : 0;
                numValidMetricTypes += throughputNode.valid() ?         1 : 0;
                numValidMetricTypes += constantNode.valid() ?           1 : 0;
                numValidMetricTypes += conversionFactorNode.valid() ?   1 : 0;
                if (numValidMetricTypes != 1u)
                {
                    NV_PERF_LOG_ERR(20, "User metric must be exactly one of the known types: %s\n", metricName.c_str());
                    return false;
                }
            }

            if (counterNode.valid())
            {
                if (!counterNode.is_map())
                {
                    NV_PERF_LOG_ERR(20, "Counter metric must be a map: %s\n", NodeToString(counterNode).c_str());
                    return false;
                }

                const auto exprNode = counterNode.find_child("expr");
                if (!exprNode.valid() || !exprNode.has_val())
                {
                    NV_PERF_LOG_ERR(20, "Counter metric must have an expression: %s\n", metricName.c_str());
                    return false;
                }

                pUserMetric = std::make_unique<UserMetricCounter>();
                auto& counter = static_cast<UserMetricCounter&>(*pUserMetric);
                counter.expr = NodeToString(exprNode.val());

                const auto sustainedRateNode = counterNode.find_child("sustainedRate");
                counter.sustainedRate = (sustainedRateNode.valid() && sustainedRateNode.has_val()) ? NodeToString(sustainedRateNode.val()) : "";
            }
            else if (ratioNode.valid())
            {
                if (!ratioNode.is_map())
                {
                    NV_PERF_LOG_ERR(20, "Ratio metric must be a map: %s\n", NodeToString(ratioNode).c_str());
                    return false;
                }

                const auto exprNode = ratioNode.find_child("expr");
                const auto maxRateNode = ratioNode.find_child("maxRate");
                if (!exprNode.valid() || !exprNode.has_val() || !maxRateNode.valid() || !maxRateNode.has_val())
                {
                    NV_PERF_LOG_ERR(20, "Ratio metric must have an expr and max rate: %s\n", metricName.c_str());
                    return false;
                }

                pUserMetric = std::make_unique<UserMetricRatio>();
                auto& ratio = static_cast<UserMetricRatio&>(*pUserMetric);
                ratio.expr = NodeToString(exprNode.val());
                ratio.maxRate = NodeToString(maxRateNode.val());
            }
            else if (throughputNode.valid())
            {
                if (!throughputNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "Throughput metric must be a list: %s\n", metricName.c_str());
                    return false;
                }

                pUserMetric = std::make_unique<UserThroughputMetric>();
                auto& throughput = static_cast<UserThroughputMetric&>(*pUserMetric);
                for (const auto& counterOrSubthroughput : throughputNode)
                {
                    if (!counterOrSubthroughput.has_val())
                    {
                        NV_PERF_LOG_ERR(20, "Throughput metric must have valid counters or sub-throughputs: %s\n", metricName.c_str());
                        return false;
                    }
                    throughput.countersOrSubThroughputs.push_back(NodeToString(counterOrSubthroughput.val()));
                }
                if (throughput.countersOrSubThroughputs.empty())
                {
                    NV_PERF_LOG_ERR(20, "Throughput metric must have at least one counter or sub-throughput: %s\n", metricName.c_str());
                    return false;
                }
            }
            else if (constantNode.valid())
            {
                if (!constantNode.is_map())
                {
                    NV_PERF_LOG_ERR(20, "Constant metric must be a map: %s\n", NodeToString(constantNode).c_str());
                    return false;
                }

                const auto valNode = constantNode.find_child("val");
                const auto hwunitNode = constantNode.find_child("hwunit");
                const auto dimunitsNode = constantNode.find_child("dimunits");
                if (!valNode.valid() || !valNode.has_val() || !hwunitNode.valid() || !hwunitNode.has_val() || !dimunitsNode.valid() || !dimunitsNode.has_val())
                {
                    NV_PERF_LOG_ERR(20, "Constant metric must have a val, hwunit and dimunits: %s\n", metricName.c_str());
                    return false;
                }

                pUserMetric = std::make_unique<UserConstantMetric>();
                auto& constant = static_cast<UserConstantMetric&>(*pUserMetric);
                constant.val = NodeToString(valNode.val());
                constant.hwunit = NodeToString(hwunitNode.val());
                constant.dimunits = NodeToString(dimunitsNode.val());
            }
            else if (conversionFactorNode.valid())
            {
                if (!conversionFactorNode.is_map())
                {
                    NV_PERF_LOG_ERR(20, "Conversion factor metric must be a map: %s\n", NodeToString(conversionFactorNode).c_str());
                    return false;
                }

                const auto multiplierNode = conversionFactorNode.find_child("multiplier");
                const auto dimunitsNode = conversionFactorNode.find_child("dimunits");
                if (!multiplierNode.valid() || !multiplierNode.has_val() || !dimunitsNode.valid() || !dimunitsNode.has_val())
                {
                    NV_PERF_LOG_ERR(20, "Conversion factor metric must have a multiplier and dimunits: %s\n", metricName.c_str());
                    return false;
                }

                pUserMetric = std::make_unique<UserConversionFactorMetric>();
                auto& conversionFactor = static_cast<UserConversionFactorMetric&>(*pUserMetric);
                conversionFactor.multiplier = NodeToString(multiplierNode.val());
                conversionFactor.dimunits = NodeToString(dimunitsNode.val());
            }
            else
            {
                assert(!"Should not reach here");
                NV_PERF_LOG_ERR(20, "Unknown user metric type: %s\n", metricName.c_str());
                return false;
            }

            assert(pUserMetric);
            pUserMetric->name = metricName;
            const auto descriptionNode = userMetricNode.find_child("description");
            if (descriptionNode.valid() && descriptionNode.has_val())
            {
                pUserMetric->description = NodeToString(descriptionNode.val());
            }
            const auto additionalScriptNode = userMetricNode.find_child("additionalScript");
            if (additionalScriptNode.valid() && additionalScriptNode.has_val())
            {
                pUserMetric->additionalScript = NodeToString(additionalScriptNode.val());
            }

            return true;
        }

        inline bool ParseMetricGroup(const ryml::NodeRef& metricGroupNode, detail::MetricGroup& metricGroup)
        {
            if (!metricGroupNode.valid() || !metricGroupNode.is_map())
            {
                NV_PERF_LOG_ERR(20, "Metric group must be a valid map\n");
                return false;
            }

            if (!metricGroupNode.has_key())
            {
                NV_PERF_LOG_ERR(20, "Metric group node doesn't have a valid key\n");
                return false;
            }
            const std::string groupName = NodeToString(metricGroupNode.key());
            const auto metricsNode = metricGroupNode.find_child("metrics");
            const auto userMetricsNode = metricGroupNode.find_child("userMetrics");
            if (!metricsNode.valid() && !userMetricsNode.valid())
            {
                NV_PERF_LOG_ERR(20, "Metric group must have metrics or user metrics: %s\n", groupName.c_str());
                return false;
            }

            metricGroup.groupName = groupName;
            if (metricsNode.valid())
            {
                if (!metricsNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "Metrics must be a list: %s\n", NodeToString(metricsNode).c_str());
                    return false;
                }

                for (const auto& metricNode : metricsNode)
                {
                    if (!metricNode.has_val())
                    {
                        NV_PERF_LOG_ERR(20, "Each metric must be a valid string: %s\n", NodeToString(metricNode).c_str());
                        return false;
                    }
                    metricGroup.metrics.push_back(NodeToString(metricNode.val()));
                }
            }

            const auto schedulingHintsNode = metricGroupNode.find_child("schedulingHints");
            if (schedulingHintsNode.valid())
            {
                if (!schedulingHintsNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "Scheduling hints must be a list: %s\n", NodeToString(schedulingHintsNode).c_str());
                    return false;
                }

                for (const auto& perDomainHintsPseudoNode : schedulingHintsNode.children())
                {
                    if (!perDomainHintsPseudoNode.is_map())
                    {
                        NV_PERF_LOG_ERR(20, "Per domain hints must be a map: %s\n", NodeToString(perDomainHintsPseudoNode).c_str());
                        return false;
                    }

                    if (perDomainHintsPseudoNode.num_children() != 1)
                    {
                        NV_PERF_LOG_ERR(20, "Per domain hints must have exactly one key: %s\n", NodeToString(perDomainHintsPseudoNode).c_str());
                        return false;
                    }

                    const auto perDomainHintsNode = perDomainHintsPseudoNode.first_child();
                    if (!perDomainHintsNode.is_seq())
                    {
                        NV_PERF_LOG_ERR(20, "Per domain hints must be a list: %s\n", NodeToString(perDomainHintsNode).c_str());
                        return false;
                    }
                    if (!perDomainHintsNode.has_key())
                    {
                        NV_PERF_LOG_ERR(20, "Per domain hints node doesn't have a valid key: %s\n", NodeToString(perDomainHintsNode).c_str());
                        return false;
                    }

                    DomainScheduleHints perDomainHints;
                    perDomainHints.domain = NodeToString(perDomainHintsNode.key());
                    for (const auto& hintNode : perDomainHintsNode.children())
                    {
                        if (!hintNode.has_val())
                        {
                            NV_PERF_LOG_ERR(20, "Each hint must be a valid string: %s\n", NodeToString(hintNode).c_str());
                            return false;
                        }
                        DomainScheduleHints::Hint hint;
                        hint.rawCounterName = NodeToString(hintNode.val());
                        perDomainHints.hints.emplace_back(std::move(hint));
                    }
                    metricGroup.schedulingHints.emplace_back(std::move(perDomainHints));
                }
            }

            if (userMetricsNode.valid())
            {
                if (!userMetricsNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "User metrics must be a list: %s\n", groupName.c_str());
                    return false;
                }

                for (const ryml::NodeRef& userMetricPseudoNode : userMetricsNode.children())
                {
                    if (!userMetricPseudoNode.is_map())
                    {
                        NV_PERF_LOG_ERR(20, "User metric must be a map: %s\n", NodeToString(userMetricPseudoNode).c_str());
                        return false;
                    }

                    if (userMetricPseudoNode.num_children() != 1)
                    {
                        NV_PERF_LOG_ERR(20, "User metric must have exactly one key: %s\n", NodeToString(userMetricPseudoNode).c_str());
                        return false;
                    }

                    const auto userMetricNode = userMetricPseudoNode.first_child();
                    std::unique_ptr<detail::UserMetricBase> pMetric;
                    if (!detail::ParseUserMetric(userMetricNode, pMetric))
                    {
                        NV_PERF_LOG_ERR(20, "Failed to parse user metric: %s\n", NodeToString(userMetricNode).c_str());
                        return false;
                    }
                    metricGroup.userMetrics.emplace_back(std::move(pMetric));
                }
            }

            return true;
        }

        inline bool ParseTriageGroup(const ryml::NodeRef& triageGroupNode, const std::vector<MetricGroup>& metricGroups, TriageGroup& triageGroup)
        {
            if (!triageGroupNode.valid() || !triageGroupNode.is_map())
            {
                NV_PERF_LOG_ERR(20, "Triage group must be a valid map\n");
                return false;
            }

            const auto nameNode = triageGroupNode.find_child("name");
            if (!nameNode.valid() || !nameNode.has_val())
            {
                NV_PERF_LOG_ERR(20, "Triage group must have a name\n");
                return false;
            }
            triageGroup.name = NodeToString(nameNode.val());

            const auto groupsNode = triageGroupNode.find_child("groups");
            const auto variationsNode = triageGroupNode.find_child("variations");
            if (!groupsNode.valid() && !variationsNode.valid())
            {
                NV_PERF_LOG_ERR(20, "Triage group must have groups or variations\n");
                return false;
            }

            auto getMetricGroup = [&](const std::string& metricGroupName) -> const MetricGroup* {
                auto it = std::find_if(metricGroups.begin(), metricGroups.end(), [&](const MetricGroup& metricGroup) {
                    return metricGroup.groupName == metricGroupName;
                });
                return it != metricGroups.end() ? &(*it) : nullptr;
            };

            if (groupsNode.valid())
            {
                if (!groupsNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "Groups must be a list\n");
                    return false;
                }

                for (const auto& groupNode : groupsNode)
                {
                    if (!groupNode.has_val())
                    {
                        NV_PERF_LOG_ERR(20, "Group must be a string\n");
                        return false;
                    }

                    const std::string groupName = NodeToString(groupNode.val());
                    const MetricGroup* pMetricGroup = getMetricGroup(groupName);
                    if (!pMetricGroup)
                    {
                        NV_PERF_LOG_ERR(20, "Unrecognized group name in groups: %s\n", groupName.c_str());
                        return false;
                    }
                    triageGroup.commonSubGroups.push_back(pMetricGroup);
                }
            }

            if (variationsNode.valid())
            {
                if (!variationsNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "Variations must be a list\n");
                    return false;
                }

                for (const auto& variationPseudoNode : variationsNode)
                {
                    if (!variationPseudoNode.is_map())
                    {
                        NV_PERF_LOG_ERR(20, "Variation must be a map\n");
                        return false;
                    }

                    if (variationPseudoNode.num_children() != 1)
                    {
                        NV_PERF_LOG_ERR(20, "Variation must have exactly one key: %s\n", NodeToString(variationPseudoNode).c_str());
                        return false;
                    }

                    const auto variationNode = variationPseudoNode.first_child();
                    if (!variationNode.is_map())
                    {
                        NV_PERF_LOG_ERR(20, "Variation must be a map: %s\n", NodeToString(variationNode).c_str());
                        return false;
                    }
                    if (!variationNode.has_key())
                    {
                        NV_PERF_LOG_ERR(20, "Variation node doesn't have a valid key: %s\n", NodeToString(variationNode).c_str());
                        return false;
                    }
                    TriageGroup::Variation variation;
                    variation.name = NodeToString(variationNode.key());
                    // groups
                    {
                        const auto groupsNode = variationNode.find_child("groups");
                        if (!groupsNode.valid())
                        {
                            NV_PERF_LOG_ERR(20, "Variation must have groups: %s\n", variation.name.c_str());
                            return false;
                        }

                        if (!groupsNode.is_seq())
                        {
                            NV_PERF_LOG_ERR(20, "Groups must be a list: %s\n", variation.name.c_str());
                            return false;
                        }

                        for (const auto& groupNode : groupsNode)
                        {
                            if (!groupNode.has_val())
                            {
                                NV_PERF_LOG_ERR(20, "Group must be a string: %s\n", variation.name.c_str());
                                return false;
                            }

                            const std::string groupName = NodeToString(groupNode.val());
                            const MetricGroup* pMetricGroup = getMetricGroup(groupName);
                            if (!pMetricGroup)
                            {
                                NV_PERF_LOG_ERR(20, "Unrecognized group name in variations: %s\n", groupName.c_str());
                                return false;
                            }
                            variation.subGroups.push_back(pMetricGroup);
                        }
                    }
                    // predicate
                    {
                        const auto predicatesNode = variationNode.find_child("predicate");
                        if (!predicatesNode.valid())
                        {
                            NV_PERF_LOG_ERR(20, "Variation must have a predicate: %s\n", variation.name.c_str());
                            return false;
                        }

                        if (!predicatesNode.is_map())
                        {
                            NV_PERF_LOG_ERR(20, "Predicate must be a map: %s\n", variation.name.c_str());
                            return false;
                        }

                        for (const auto& predicateNode : predicatesNode.children())
                        {
                            if (!predicateNode.is_keyval())
                            {
                                NV_PERF_LOG_ERR(20, "Predicate must be a key-value pair: %s\n", variation.name.c_str());
                                return false;
                            }
                            const std::string predicateName = NodeToString(predicateNode.key());
                            const std::string predicateValue = NodeToString(predicateNode.val());
                            variation.predicates[predicateName] = predicateValue;
                        }
                    }

                    triageGroup.variations.emplace_back(std::move(variation));
                }
            }

            return true;
        }

        inline bool IsVersionCompatible(uint8_t major, uint8_t minor)
        {
            if (major != METRICS_CONFIG_VERSION_MAJOR)
            {
                NV_PERF_LOG_ERR(20, "Incompatible major version. Config version: %d, library version: %d\n", major, METRICS_CONFIG_VERSION_MAJOR);
                return false;
            }
            if (minor > METRICS_CONFIG_VERSION_MINOR)
            {
                NV_PERF_LOG_WRN(50, "Minor version mismatch. Config version: %d, library version: %d\n", minor, METRICS_CONFIG_VERSION_MINOR);
                // allowable
            }
            return true;
        }

        inline bool InitializeMetricConfigObjectDataFromYaml_Core(const std::string& yaml, MetricConfigObjectData& data)
        {
            ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yaml));
            const auto rootNode = tree.rootref();

            if (!rootNode.valid() || !rootNode.is_map())
            {
                NV_PERF_LOG_ERR(20, "Invalid yaml\n");
                return false;
            }

            const auto versionNode = rootNode.find_child("version");
            const auto gpuNode = rootNode.find_child("gpu");
            if (!versionNode.valid() || !gpuNode.valid())
            {
                NV_PERF_LOG_ERR(20, "Config must have version and gpu\n");
                return false;
            }

            const auto userMetricsNode = rootNode.find_child("userMetrics");
            const auto metricGroupsNode = rootNode.find_child("metricGroups");
            if (!userMetricsNode.valid() && !metricGroupsNode.valid())
            {
                NV_PERF_LOG_ERR(20, "Config must have userMetrics or metricGroups\n");
                return false;
            }

            const auto triageGroupNode = rootNode.find_child("triageGroup");
            if (triageGroupNode.valid() && !metricGroupsNode.valid())
            {
                NV_PERF_LOG_ERR(20, "Triage group must have metricGroups\n");
                return false;
            }

            // parse version
            {
                if (!versionNode.has_val())
                {
                    NV_PERF_LOG_ERR(20, "Version must have a value\n");
                    return false;
                }
                std::istringstream versionStream(NodeToString(versionNode.val()));
                std::string versionPart;
                std::getline(versionStream, versionPart, '.');
                data.versionMajor = static_cast<uint8_t>(std::stoi(versionPart));
                std::getline(versionStream, versionPart, '.');
                data.versionMinor = static_cast<uint8_t>(std::stoi(versionPart));
                std::getline(versionStream, versionPart, '.');
                data.versionPatch = static_cast<uint8_t>(std::stoi(versionPart));
            }
            if (!IsVersionCompatible(data.versionMajor, data.versionMinor))
            {
                return false;
            }

            if (!gpuNode.has_val())
            {
                NV_PERF_LOG_ERR(20, "GPU must have a value\n");
                return false;
            }
            data.gpu = NodeToString(gpuNode.val());

            if (userMetricsNode.valid())
            {
                if (!userMetricsNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "User metrics must be a list\n");
                    return false;
                }

                for (const ryml::NodeRef& userMetricPseudoNode : userMetricsNode.children())
                {
                    if (!userMetricPseudoNode.is_map())
                    {
                        NV_PERF_LOG_ERR(20, "User metric must be a map: %s\n", NodeToString(userMetricPseudoNode).c_str());
                        return false;
                    }

                    if (userMetricPseudoNode.num_children() != 1)
                    {
                        NV_PERF_LOG_ERR(20, "User metric must have exactly one key: %s\n", NodeToString(userMetricPseudoNode).c_str());
                        return false;
                    }

                    const auto userMetricNode = userMetricPseudoNode.first_child();
                    std::unique_ptr<UserMetricBase> pMetric;
                    if (!ParseUserMetric(userMetricNode, pMetric))
                    {
                        NV_PERF_LOG_ERR(20, "Failed to parse user metric\n");
                        return false;
                    }
                    data.globalUserMetrics.emplace_back(std::move(pMetric));
                }
            }

            if (metricGroupsNode.valid())
            {
                if (!metricGroupsNode.is_seq())
                {
                    NV_PERF_LOG_ERR(20, "Metric groups must be a list\n");
                    return false;
                }

                for (const ryml::NodeRef& metricGroupPseudoNode : metricGroupsNode.children())
                {
                    if (!metricGroupPseudoNode.is_map())
                    {
                        NV_PERF_LOG_ERR(20, "Metric group's format is not expected: %s\n", NodeToString(metricGroupPseudoNode).c_str());
                        return false;
                    }

                    if (metricGroupPseudoNode.num_children() != 1)
                    {
                        NV_PERF_LOG_ERR(20, "Metric group's format is not expected: %s\n", NodeToString(metricGroupPseudoNode).c_str());
                        return false;
                    }

                    const auto metricGroupNode = metricGroupPseudoNode.first_child();
                    MetricGroup metricGroup;
                    if (!ParseMetricGroup(metricGroupNode, metricGroup))
                    {
                        NV_PERF_LOG_ERR(20, "Failed to parse metric group\n");
                        return false;
                    }
                    data.metricGroups.emplace_back(std::move(metricGroup));
                }
            }

            if (triageGroupNode.valid())
            {
                if (!ParseTriageGroup(triageGroupNode, data.metricGroups, data.triageGroup))
                {
                    NV_PERF_LOG_ERR(20, "Failed to parse triage group\n");
                    return false;
                }
            }

            return true;
        }

        inline bool InitializeMetricConfigObjectDataFromYaml(const std::string& yaml, MetricConfigObjectData& data)
        {
            return SafeParse(InitializeMetricConfigObjectDataFromYaml_Core, yaml, data);
        }

    } // namespace detail

    struct MetricsAndSchedulingHints
    {
        std::vector<std::string> metrics;
        std::vector<DomainScheduleHints> schedulingHints;
    };

    class MetricConfigObject
    {
    private:
        std::unique_ptr<detail::MetricConfigObjectData> m_pData;

    public:
        MetricConfigObject() = default;
        MetricConfigObject(const MetricConfigObject&) = delete;
        MetricConfigObject(MetricConfigObject&&) = default;
        MetricConfigObject& operator=(const MetricConfigObject&) = delete;
        MetricConfigObject& operator=(MetricConfigObject&&) = default;
        ~MetricConfigObject() = default;

        // This will also clear any existing data if it has been initialized before.
        bool InitializeFromYaml(const std::string& yaml)
        {
            auto pData = std::make_unique<detail::MetricConfigObjectData>();
            if (!InitializeMetricConfigObjectDataFromYaml(yaml, *pData))
            {
                return false;
            }
            m_pData = std::move(pData);
            return true;
        }

        // A convenience function to load the yaml from a file and then call InitializeFromYaml.
        bool InitializeFromFile(const std::string& filePath)
        {
            std::ifstream file(filePath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file: " << filePath << std::endl;
                return false;
            }

            std::stringstream ss;
            ss << file.rdbuf();
            return InitializeFromYaml(ss.str());
        }

        bool IsLoaded() const
        {
            return (!m_pData);
        }

        std::string GetTriageGroupName() const
        {
            if (!m_pData)
            {
                return "";
            }
            return m_pData->triageGroup.name;
        }

        std::vector<std::string> GetAllVariations() const
        {
            std::vector<std::string> variationNames;
            if (!m_pData)
            {
                return variationNames;
            }

            const auto& variations = m_pData->triageGroup.variations;
            variationNames.reserve(variations.size());
            for (const auto& variation : variations)
            {
                variationNames.push_back(variation.name);
            }
            return variationNames;
        }

        // Generate a script string of all user metrics defined in the config file.
        std::string GenerateScriptForAllNamespacedUserMetrics() const
        {
            if (!m_pData)
            {
                return "";
            }

            // This assumes all user metrics are already namespaced.
            std::stringstream ss;
            ss << m_pData->triageGroup.name << " = CreateNamespace()\n\n";
            ss << "-- Global Metrics\n";
            for (const auto& pMetric : m_pData->globalUserMetrics)
            {
                ss << pMetric->ToScript();
            }
            for (const auto& metricGroup : m_pData->metricGroups)
            {
                ss << "\n-- " << metricGroup.groupName << "\n";
                ss << m_pData->triageGroup.name << "." << metricGroup.groupName << " = CreateNamespace()\n";
                for (const auto& pMetric : metricGroup.userMetrics)
                {
                    ss << pMetric->ToScript();
                }
            }

            return ss.str();
        }

        // Generate a script string for user metrics of a specific variation. In addition, it will include "global"(common) user metrics and metrics defined in common subgroups.
        // If variationName is empty, it will generate script for all "global"(common) user metrics and metrics defined in common subgroups.
        // +---------------------+----------------+----------------+
        // |                     | Variation Name | Variation Name |
        // |                     | is Empty       | is Not Empty   |
        // +---------------------+----------------+----------------+
        // | Common Metrics      | Yes            | Yes            |
        // +---------------------+----------------+----------------+
        // | Common Groups       | Yes            | Yes            |
        // +---------------------+----------------+----------------+
        // | Groups in Variation | No             | Yes            |
        // +---------------------+----------------+----------------+
        std::string GenerateScriptForNameSpacedUserMetrics(const std::string& variationName) const
        {
            if (!m_pData)
            {
                return "";
            }

            std::stringstream ss;
            ss << m_pData->triageGroup.name << " = CreateNamespace()\n\n";
            ss << "-- Global Metrics\n";
            for (const auto& pMetric : m_pData->globalUserMetrics)
            {
                ss << pMetric->ToScript();
            }
            for (const auto* pMetricGroup : m_pData->triageGroup.commonSubGroups)
            {
                ss << "\n-- " << pMetricGroup->groupName << "\n";
                ss << m_pData->triageGroup.name << "." << pMetricGroup->groupName << " = CreateNamespace()\n";
                for (const auto& pMetric : pMetricGroup->userMetrics)
                {
                    ss << pMetric->ToScript();
                }
            }

            if (variationName.empty())
            {
                // In case of empty variation name, all the needed metrics are already added, so we're done here.
                return ss.str();
            }

            const auto& variations = m_pData->triageGroup.variations;
            const auto it = std::find_if(variations.begin(), variations.end(), [&](const detail::TriageGroup::Variation& variation) {
                return variation.name == variationName;
            });
            if (it == variations.end())
            {
                std::cerr << "Variation name not found: " << variationName << std::endl;
                return "";
            }

            const auto& variation = *it;
            for (const auto* pSubGroup : variation.subGroups)
            {
                ss << "\n-- " << pSubGroup->groupName << "\n";
                ss << m_pData->triageGroup.name << "." << pSubGroup->groupName << " = CreateNamespace()\n";
                for (const auto& pMetric : pSubGroup->userMetrics)
                {
                    ss << pMetric->ToScript();
                }
            }

            return ss.str();
        }

        // Get all common groups + groups associated with a variation(if variationName is not empty).
        // For each group, get its metrics and scheduling hints. The return value is an union of all groups.
        MetricsAndSchedulingHints GetMetricsAndScheduleHints(const std::string& variationName) const
        {
            MetricsAndSchedulingHints metricsAndSchedulingHints;
            if (!m_pData)
            {
                return metricsAndSchedulingHints;
            }

            auto addSubGroup = [&](const detail::MetricGroup* pMetricGroup) {
                metricsAndSchedulingHints.metrics.insert(metricsAndSchedulingHints.metrics.end(), pMetricGroup->metrics.begin(), pMetricGroup->metrics.end());
                for (const auto& pUserMetric : pMetricGroup->userMetrics)
                {
                    metricsAndSchedulingHints.metrics.push_back(pUserMetric->name);
                }
                metricsAndSchedulingHints.schedulingHints.insert(metricsAndSchedulingHints.schedulingHints.end(), pMetricGroup->schedulingHints.begin(), pMetricGroup->schedulingHints.end());
            };

            for (const auto* pCommonSubGroup : m_pData->triageGroup.commonSubGroups)
            {
                addSubGroup(pCommonSubGroup);
            }

            if (variationName.empty())
            {
                return metricsAndSchedulingHints;
            }

            const auto& variations = m_pData->triageGroup.variations;
            const auto it = std::find_if(variations.begin(), variations.end(), [&](const detail::TriageGroup::Variation& variation) {
                return variation.name == variationName;
            });
            if (it == variations.end())
            {
                std::cerr << "Variation name not found: " << variationName << std::endl;
                return MetricsAndSchedulingHints();
            }

            const auto& variation = *it;
            for (const auto* pSubGroup : variation.subGroups)
            {
                addSubGroup(pSubGroup);
            }

            return metricsAndSchedulingHints;
        }
    };

}}
