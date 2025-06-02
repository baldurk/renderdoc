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

namespace nv { namespace perf { namespace hud {
    class HudTextRenderer : public HudRenderer{
    private:
        std::function<void(const char* format, va_list vlist)> m_printFn;
        bool m_enableConsoleOutput = false;
        std::string m_columnSeparator = ",";
        size_t m_maxIntegerLength = 8;
        size_t m_decimalPlaces = 2;
        size_t m_frameCount = 0;

    private:
        static std::string AddLeftPadding(const std::string& text, size_t length)
        {
            std::string alignedText = text;
            if (text.size() <= length)
            {
                alignedText.insert(0, length - text.size(), ' ');
            }
            else
            {
                NV_PERF_LOG_ERR(20, "The default max integer length is not enough, please SetMaxIntegerLength().\n");
            }
            return alignedText;
        }

        static std::string AddRightPadding(const std::string& text, size_t length)
        {
            std::string alignedText = text;
            if (text.size() <= length)
            {
                alignedText.insert(text.size(), length - text.size(), ' ');
            }
            else
            {
                NV_PERF_LOG_ERR(20, "The default max integer length is not enough, please SetMaxIntegerLength().\n");
            }
            return alignedText;
        }

        static std::string FormatValue(double value, size_t decimalPlaces)
        {
            if (std::isnan(value))
            {
                return "nan";
            }
            else if (std::isinf(value))
            {
                return "inf";
            }
            std::string decimalPlacesFormat = "%." + std::to_string(decimalPlaces) + "f";
            char buf[32] = "";
            snprintf(buf, 32, decimalPlacesFormat.c_str(), value);
            std::string formatValue = std::string(&(buf[0]));
            return std::string(&(buf[0]));
        }

        virtual bool RenderFrameSeparator() override
        {
            Print("\n===Frame%d===\n", m_frameCount);
            ++m_frameCount;
            return true;
        }

        virtual bool RenderPanelBegin(const Panel& panel, bool* showContents) const override
        {
            Print("%s\n", panel.name.c_str());
            return true;
        }

        virtual bool RenderPanelEnd(const Panel&) const override
        {
            Print("%s", "---------------------------------\n");
            return true;
        }

        virtual bool RenderScalarTextBlock(const std::vector<const ScalarText*>& block) const override
        {
            size_t maxScalarTextLength = 0;
            for (const auto* pScalarText : block)
            {
                const auto& scalarText = *pScalarText;
                maxScalarTextLength = (std::max)(scalarText.label.text.length(), maxScalarTextLength);
            }
            for (const auto* pScalarText : block)
            {
                const auto& scalarText = *pScalarText;
                auto showValue = scalarText.showValue;

                if (showValue == ScalarText::ShowValue::Hide)
                {
                    continue;
                }
                double value = scalarText.signal.valBuffer.Size() > 0 ? scalarText.signal.valBuffer.Front() : 0.0;
                double max = scalarText.signal.maxValue;
                auto unit = scalarText.signal.unit;
                auto description = scalarText.signal.description;
                auto tooltip = description;
                const std::string unitText = unit.empty() || unit == MetricSignal::HideUnit() ? "" : " " + unit;
                
                const size_t maxValueLength = m_maxIntegerLength + m_decimalPlaces + 1; // 1=.
                if (showValue != ScalarText::ShowValue::ValueWithMax)
                {
                    std::string text = AddRightPadding(scalarText.label.text, maxScalarTextLength) + ": " +
                        AddLeftPadding(FormatValue(value, m_decimalPlaces), maxValueLength) + unitText;
                    Print("%s\n", text.c_str());
                }
                else
                {
                    std::string text = AddRightPadding(scalarText.label.text, maxScalarTextLength) + ": " +
                        AddRightPadding(FormatValue(value, m_decimalPlaces), maxValueLength) + unitText
                        + "(max:" + AddLeftPadding(FormatValue(max, m_decimalPlaces), maxValueLength) + unitText + ")";
                    Print("%s\n", text.c_str());
                }
            }
            return true;
        }

        virtual bool RenderTimePlot(const TimePlot& plot) const override
        {
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

            if (!plot.label.text.empty())
            {
                std::string plotTitle = plot.label.text;

                // Add the unit to the title of the plot
                if (!unit.empty() && unit != MetricSignal::HideUnit())
                {
                    plotTitle += " (" + unit + ")";
                }
                Print("%s\n", plotTitle.c_str());
            }

            if (plot.signals.size() > 0)
            {
                // Determine the offset for only drawing data points within the desired timeWidth
                size_t dataOffset = 0;
                double thresholdTimestamp = plot.pTimestampBuffer->Size() > 0 ? plot.pTimestampBuffer->Front() - plot.timeWidth : 0.0;
                for (dataOffset = 0; dataOffset < plot.pTimestampBuffer->Size(); ++dataOffset)
                {
                    if (plot.pTimestampBuffer->Get(dataOffset) >= thresholdTimestamp)
                    {
                        break;
                    }
                }

                const size_t maxValueLength = m_maxIntegerLength + m_decimalPlaces + 1; // 1=.
                auto PrintSignals = [&](const std::vector<MetricSignal>& signals, const RingBuffer<double>& timestampBuffer, size_t dataOffset) {
                    std::vector<size_t> columnLengths(1 + signals.size(), maxValueLength);
                    const std::string timeLabel = "Timestamp";
                    columnLengths[0] = (std::max)(columnLengths[0], timeLabel.length());
                    std::string labelTexts = AddRightPadding(timeLabel, columnLengths[0]);
                    for(size_t signalIndex = 0; signalIndex < signals.size(); signalIndex++)
                    {
                        const MetricSignal& signal = signals[signalIndex];
                        columnLengths[signalIndex + 1] = (std::max)(columnLengths[signalIndex + 1], signal.label.text.length());
                        labelTexts += " " + m_columnSeparator + " " + AddRightPadding(signal.label.text, columnLengths[signalIndex + 1]);
                    }
                    labelTexts += " " + m_columnSeparator;
                    Print("%s\n", labelTexts.c_str());
                    std::string valueTexts = "";
                    for (size_t startIndex = dataOffset; startIndex < timestampBuffer.Size(); startIndex++)
                    {
                        std::string formatTime = FormatValue(timestampBuffer.Get(startIndex), m_decimalPlaces);
                        valueTexts += AddLeftPadding(formatTime, columnLengths[0]);
                        for (size_t signalIndex = 0; signalIndex < signals.size(); signalIndex++)
                        {
                            const MetricSignal& signal = signals[signalIndex];
                            if (startIndex < signal.valBuffer.Size())
                            {
                                std::string formatVal = FormatValue(signal.valBuffer.Get(startIndex), m_decimalPlaces);
                                valueTexts += " " + m_columnSeparator + " " + AddLeftPadding(formatVal, columnLengths[signalIndex + 1]);
                            }
                        }
                        valueTexts += " " + m_columnSeparator;
                        Print("%s\n", valueTexts.c_str());
                        valueTexts = "";
                    }
                };

                if (plot.chartType == TimePlot::ChartType::Overlay)
                {
                    PrintSignals(plot.signals, *plot.pTimestampBuffer, dataOffset);
                }
                else if (plot.chartType == TimePlot::ChartType::Stacked)
                {
                    PrintSignals(plot.stackedSignals, *plot.pTimestampBuffer, dataOffset);
                }
            }
            return true;
        }

    protected:
        virtual void Print(const char* format, ...) const
        {
            if (m_enableConsoleOutput)
            {
                va_list list;
                va_start(list, format);
                m_printFn(format, list);
                va_end(list);
            }
        }

    public:
        HudTextRenderer() : HudRenderer(){}

        void SetConsoleOutput(std::function<void(const char*, va_list vlist)> printFn)
        {
            m_printFn = printFn;
            m_enableConsoleOutput = true;
        }

        void SetColumnSeparator(const std::string& separator)
        {
            m_columnSeparator = separator;
        }

        void SetDecimalPlaces(size_t decimalPlaces)
        {
            m_decimalPlaces = decimalPlaces;
        }

        void SetMaxIntegerLength(size_t maxIntegerLength)
        {
            m_maxIntegerLength = maxIntegerLength;
        }
	};
}}}