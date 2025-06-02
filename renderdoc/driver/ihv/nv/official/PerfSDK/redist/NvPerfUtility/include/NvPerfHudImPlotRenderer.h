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

#include "NvPerfHudRenderer.h"

#include <imgui.h>
#include <implot.h>

namespace nv { namespace perf { namespace hud {

class HudImPlotRenderer : public HudRenderer
{
private:
    const Color m_defaultTextColor = Color::White();

private:
    static ImVec4 ImVec4FromColor(const Color& color)
    {
        auto vec = color.RgbaVecF();
        return ImVec4(vec[0], vec[1], vec[2], vec[3]);
    }

protected:
    bool RenderPanelBegin(const Panel& panel, bool *showContents) const override
    {
        auto color = panel.label.color.IsValid() ? panel.label.color.Abgr() : m_defaultTextColor.Abgr();

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        bool show = ImGui::CollapsingHeader(panel.label.text.c_str(), panel.defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        ImGui::PopStyleColor();
            
        if (showContents != nullptr)
        {
            *showContents = show;
        }

        return true;
    }

    bool RenderPanelEnd(const Panel&) const override
    {
        // nothing to do here
        return true;
    }

    bool RenderSeparator(const Separator&) const override
    {
        ImGui::Separator();
        return true;

    }

    bool RenderScalarTextBlock(const std::vector<const ScalarText*>& block) const override
    {
        // cell-spanning/merging is not possible in imgui
        //
        // column:    0      1  2 3            4 5 6  7
        //        label    100    milliseconds
        //        label   1234
        //        label     10    apples       / 20   apples
        //        label     10.0  %
        //        label      1.03 ms
        //        label     10    ns           /  2.0 ns

        std::string tableId;
        for (const ScalarText* pText : block)
        {
            tableId += pText->label.text + "+";
        }

        bool anyValueWithMax = false;
        for (const ScalarText* pText : block)
        {
            if (pText->showValue == ScalarText::ShowValue::ValueWithMax)
            {
                anyValueWithMax = true;
                break;
            }
        }
        int columnCount = anyValueWithMax ? 8 : 4;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0, ImGui::GetStyle().CellPadding.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0, ImGui::GetStyle().FramePadding.y));

        ImGuiTableFlags tableFlags = 0;
#if IMGUI_VERSION_NUM >= 18000
        tableFlags |= ImGuiTableFlags_SizingFixedFit;
#elif IMGUI_VERSION_NUM > 17900 // ImGui 1.80 Pre-release uses a different name for this flag
        tableFlags |= ImGuiTableFlags_ColumnsWidthFixed;
#else
#error ImGui versions 1.79 or below do not support tables
#endif

        if (ImGui::BeginTable(tableId.c_str(), columnCount, tableFlags))
        {
            for (size_t index = 0; index < block.size(); ++index)
            {
                const auto& scalarText = *block[index];

                auto showValue = scalarText.showValue;

                if (showValue == ScalarText::ShowValue::Hide)
                {
                    continue;
                }

                ImGui::TableNextRow();

                double value = scalarText.signal.valBuffer.Size() > 0 ? scalarText.signal.valBuffer.Front() : 0.0;
                double max = scalarText.signal.maxValue;
                auto color = scalarText.signal.color.IsValid() ? scalarText.signal.color.Abgr() : m_defaultTextColor.Abgr();
                auto unit = scalarText.signal.unit;
                auto description = scalarText.signal.description;
                auto tooltip = description;
                std::string decimalPlacesFormat = "%." + std::to_string(scalarText.decimalPlaces) + "lf";

                auto formatIntegerPart = [](double value) -> std::string
                {
                    if (std::isnan(value))
                    {
                        return "nan";
                    }
                    else if (std::isinf(value))
                    {
                        return "inf";
                    }

                    static const std::locale locale("en_US.UTF-8");
                    std::ostringstream os;
                    os.imbue(locale);
                    os.precision(0);
                    os.setf(std::ios::fixed);
                    os << value;
                    return os.str();
                };

                auto formatDecimalPlaces = [&](double value) -> std::string
                {
                    if (std::isnan(value) || std::isinf(value))
                    {
                        return std::string(); // if value is nan or inf, we don't want to print anything else in the decimalplaces column
                    }
                    double decimals = value - std::floor(value);
                    char buf[32] = "";
                    snprintf(buf, 32, decimalPlacesFormat.c_str(), decimals);
                    return std::string(&(buf[1]));
                };

                // draw label
                {
                    ImGui::TableSetColumnIndex(0);

                    auto labelColor = scalarText.label.color.IsValid() ? scalarText.label.color.Abgr() : m_defaultTextColor.Abgr();
                    auto labelText = scalarText.label.text + "   "; // manual spacing, because the table does none

                    ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
                    ImGui::TextUnformatted(labelText.c_str());
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered() && !tooltip.empty())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(tooltip.c_str());
                        ImGui::EndTooltip();
                    }
                }

                // draw number in front of decimal point
                {
                    ImGui::TableSetColumnIndex(1);

                    const std::string text = formatIntegerPart(value);

                    // https://stackoverflow.com/questions/58044749/how-to-right-align-text-in-imgui-columns
                    // https://twitter.com/ocornut/status/1401775254696083456
                    auto posX = (ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x);
                    if (posX > ImGui::GetCursorPosX())
                    {
                        ImGui::SetCursorPosX(posX);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::PopStyleColor();
                }

                // draw decimal places
                {
                    ImGui::TableSetColumnIndex(2);

                    const std::string text = formatDecimalPlaces(value);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::PopStyleColor();
                }

                // draw unit of JustValue/ValueWithMax or percent sign
                {
                    ImGui::TableSetColumnIndex(3);

                    const std::string text = unit.empty() || unit == MetricSignal::HideUnit() ? "" : " " + unit;
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::PopStyleColor();
                }

                if (showValue != ScalarText::ShowValue::ValueWithMax)
                {
                    continue;
                }

                // draw " / " for max
                {
                    ImGui::TableSetColumnIndex(4);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(" / ");
                    ImGui::PopStyleColor();
                }
                
                // draw integer portion of max
                {
                    ImGui::TableSetColumnIndex(5);

                    const std::string text = formatIntegerPart(max);

                    // https://stackoverflow.com/questions/58044749/how-to-right-align-text-in-imgui-columns
                    // https://twitter.com/ocornut/status/1401775254696083456
                    auto posX = (ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x);
                    if (posX > ImGui::GetCursorPosX())
                    {
                        ImGui::SetCursorPosX(posX);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::PopStyleColor();
                }

                // draw decimal places of max
                {
                    ImGui::TableSetColumnIndex(6);

                    const std::string text = formatDecimalPlaces(max);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::PopStyleColor();
                }

                // draw unit again after max
                {
                    ImGui::TableSetColumnIndex(7);

                    std::string text = unit.empty() || unit == MetricSignal::HideUnit() ? "" : " " + unit;
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndTable();

            ImGui::PopStyleVar(2);
        }

        return true;
    }

    bool RenderTimePlot(const TimePlot& plot) const override
    {
        float plotHeight = 50.0f + ImGui::GetFont()->FontSize * float(plot.signals.size());

        // Use only major ticks for seconds
        ImPlot::PushStyleVar(ImPlotStyleVar_MinorTickLen, ImPlot::GetStyle().MajorTickLen);

        std::string unit;
        if (plot.unit.empty() && plot.unit != MetricSignal::HideUnit())
        {
            if (plot.signals.size() > 0)
            {
                unit = plot.signals[0].unit;
            }
        }
        else
        {
            unit = plot.unit;
        }

        ImPlotFlags flags = ImPlotFlags_NoInputs | ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoFrame;
        std::string plotTitle;
        if (plot.label.text.empty())
        {
            flags |= ImPlotFlags_NoTitle;
            plotTitle = "TimePlot " + std::to_string((unsigned long)((uintptr_t)&plot)); // some unique ID to satisfy ImGui
        }
        else
        {
            plotTitle = plot.label.text;

            // Add the unit to the title of the plot
            if (!unit.empty() && unit != MetricSignal::HideUnit())
            {
                plotTitle += " (" + unit + ")";
            }
        }

        bool beginPlot = ImPlot::BeginPlot(plotTitle.c_str(), ImVec2(-1, plotHeight), flags);

        if (beginPlot)
        {
            // Setup legend
            // Note: Using the flags ImPlotLegendFlags_NoButtons, _NoHighlightItem or _NoHighlightAxis would disable IsLegendEntryHovered() which is needed for tooltips
            ImPlot::SetupLegend(ImPlotLocation_NorthWest);

            // Setup Axes
            ImPlotAxisFlags xAxisFlags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickLabels;
            ImPlotAxisFlags yAxisFlags = ImPlotAxisFlags_None;

            double valueMin = plot.valueMin;
            double valueMax = plot.valueMax;
            if (std::isnan(valueMin) && std::isnan(valueMax))
            {
                yAxisFlags |= ImPlotAxisFlags_AutoFit;
            }
            else if (!std::isnan(valueMin) && std::isnan(valueMax))
            {
                NV_PERF_LOG_ERR(20, "If minValue is specified, maxValue needs to be specified as well, but hasn't\n");
                ImPlot::PopStyleVar(1); // ImPlotStyleVar_MajorTickLen
                return false;
            }
            else if (std::isnan(valueMin) && !std::isnan(valueMax))
            {
                valueMin = 0.0;
            }

            ImPlot::SetupAxes(nullptr, nullptr, xAxisFlags, yAxisFlags);
            if (!std::isnan(valueMax))
            {
                ImPlot::SetupAxisLimits(ImAxis_Y1, valueMin, valueMax, ImGuiCond_Always);
            }

            // Draw Plots
            if (plot.signals.size() > 0)
            {
                // Determine the offset for only drawing data points within the desired timeWidth
                size_t dataOffset;
                double thresholdTimestamp = plot.pTimestampBuffer->Size() > 0 ? plot.pTimestampBuffer->Front() - plot.timeWidth : 0.0;
                for (dataOffset = 0; dataOffset < plot.pTimestampBuffer->Size(); ++dataOffset)
                {
                    if (plot.pTimestampBuffer->Get(dataOffset) >= thresholdTimestamp)
                    {
                        break;
                    }
                }

                // Setup X-Axis ticks with 10, 1, 0.1, 0.01, etc intervals
                if (plot.pTimestampBuffer->Size() > 0)
                {
                    double xMin = plot.pTimestampBuffer->Get(dataOffset);
                    double xMax = plot.pTimestampBuffer->Front();
                    double range = xMax - xMin;

                    if (range > 0)
                    {
                        auto decimalCeil = [](double number, double increment)
                        {
                            return std::ceil(number / increment) * increment;
                        };
                        auto decimalFloor = [](double number, double increment)
                        {
                            return std::floor(number / increment) * increment;
                        };

                        const double leeway = 1.0; // Could make it larger to avoid range 1.0 having only 1 tick in the whole range. But on the flipside you get up to 9*leeway ticks for 9.9 seconds.
                        double tickIncrement = pow(10, floor(log10(range / leeway)));
                        int nTicks = int(decimalFloor(xMax - xMin, tickIncrement) / tickIncrement) + 1;

                        double ticks[(size_t)(10 * leeway + 0.5) + 1];
                        double tick = decimalCeil(xMin, tickIncrement);
                        for (size_t index = 0; tick <= xMax && index < nTicks; tick += tickIncrement, index += 1)
                        {
                            ticks[index] = tick;
                        }
                        ImPlot::SetupAxisTicks(ImAxis_X1, ticks, nTicks);
                    }
                }

                // If not for dataOffset, this userdata and getter would not be needed.
                // But ImPlot's PlotLine and PlotShaded assume that the number of elements in the array is the same as its length modulo.
                // I.e. you cannot have an offset, and only draw 20 points from an array of 40 points. It needs to draw all 40 in this case.
                struct UserData
                {
                    size_t dataOffset;
                    const RingBuffer<double>& timestampBuffer;
                    const RingBuffer<double>& valBuffer;
                };

                auto getter = [](int idx, void* data_) -> ImPlotPoint {
                    const auto& data = *static_cast<const UserData*>(data_);
                    ImPlotPoint point(data.timestampBuffer.Get(idx + data.dataOffset), data.valBuffer.Get(idx + data.dataOffset));
                    return point;
                };
                auto getterWithoutValBuffer = [](int idx, void* data_) -> ImPlotPoint {
                    const auto& data = *static_cast<UserData*>(data_);
                    // Assumes the minimum y of stacked plots is 0.
                    // This might not always hold, but ImPlot does not let you query a dynamic minimum.
                    // That only works via -INFINITY with PlotLine and PlotShaded without custom getters
                    ImPlotPoint point(data.timestampBuffer.Get(idx + data.dataOffset), 0.0);
                    return point;
                };

                if (plot.chartType == TimePlot::ChartType::Overlay)
                {
                    for (const MetricSignal& signal : plot.signals)
                    {
                        if (signal.valBuffer.Size() == 0)
                        {
                            continue;
                        }

                        if (signal.color.IsValid())
                        {
                            ImPlot::SetNextLineStyle(ImVec4FromColor(signal.color));
                        }

                        UserData userData{ dataOffset, *plot.pTimestampBuffer, signal.valBuffer };

                        ImPlot::PlotLineG(signal.label.text.c_str(), getter, &userData, int(signal.valBuffer.Size() - dataOffset));

                        // Tooltip
                        if (ImPlot::IsLegendEntryHovered(signal.label.text.c_str()) && !signal.description.empty())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(signal.description.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
                else if (plot.chartType == TimePlot::ChartType::Stacked)
                {
                    for (size_t index = 0; index < plot.stackedSignals.size(); ++index)
                    {
                        const MetricSignal& signal = plot.stackedSignals[index];
                        UserData userData1{ dataOffset, *plot.pTimestampBuffer, signal.valBuffer };

                        if (signal.valBuffer.Size() == 0)
                        {
                            continue;
                        }

                        if (signal.color.IsValid())
                        {
                            ImPlot::SetNextLineStyle(ImVec4FromColor(signal.color));
                            ImPlot::SetNextFillStyle(ImVec4FromColor(signal.color));
                        }

                        if (index < plot.stackedSignals.size()-1)
                        {
                            const MetricSignal& nextSignal = plot.stackedSignals[index+1];
                            UserData userData2{ dataOffset, *plot.pTimestampBuffer, nextSignal.valBuffer };

                            ImPlot::PlotShadedG(signal.label.text.c_str(), getter, &userData1, getter, &userData2, int(signal.valBuffer.Size() - dataOffset));
                        }
                        else
                        {
                            ImPlot::PlotShadedG(signal.label.text.c_str(), getter, &userData1, getterWithoutValBuffer, &userData1, int(signal.valBuffer.Size() - dataOffset));
                        }

                        // Tooltip
                        if (ImPlot::IsLegendEntryHovered(signal.label.text.c_str()) && !signal.description.empty())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(signal.description.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
            }

            ImPlot::EndPlot();
            ImPlot::PopStyleVar(1); // ImPlotStyleVar_MajorTickLen
        }

        return true;
    }

public:
    HudImPlotRenderer() : HudRenderer(){}

    virtual ~HudImPlotRenderer() = default;

    static bool SetStyle()
    {
        if (!ImGui::GetCurrentContext())
        {
            NV_PERF_LOG_ERR(20, "ImGui not initialized. Cannot set style.\n");
            return false;
        }
        if (!ImPlot::GetCurrentContext())
        {
            NV_PERF_LOG_ERR(20, "ImPlot not initialized. Cannot set style.\n");
            return false;
        }

        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_WindowBg].w *= 0.6f;
        style.Colors[ImGuiCol_ScrollbarBg].w *= 0.7f;
        style.Colors[ImGuiCol_PopupBg].w *= 0.4f;
        style.Colors[ImGuiCol_Border].w *= 0.8f;
        style.Colors[ImGuiCol_FrameBg].w *= 0.8f;
        ImPlotStyle& plotStyle = ImPlot::GetStyle();
        plotStyle.Colors[ImPlotCol_PlotBg].w = 0.3f;
        plotStyle.Colors[ImPlotCol_PlotBorder].w = 0.3f;
        plotStyle.Colors[ImPlotCol_LegendBg].w = 0.3f;
        plotStyle.Colors[ImPlotCol_LegendBorder].w = 0.3f;

        return true;
    }
};

} } } // nv::perf::hud