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

#include <cmath>

#include <array>
#include <iomanip>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <ostream>
#include <regex>
#include <string>
#include <sstream>
#include <vector>

#include "NvPerfInit.h"
#include "NvPerfCounterConfiguration.h"
#include "NvPerfCounterData.h"
#include "NvPerfHudConfigurationsHAL.h"
#include "NvPerfMetricsConfigLoader.h"
#include "NvPerfMetricsEvaluator.h"
#include "NvPerfPeriodicSamplerCommon.h"

namespace nv { namespace perf { namespace hud {

    class Widget;

    std::unique_ptr<Widget> WidgetFromYaml(const ryml::NodeRef& node, bool* pValid, const std::string& chipName, bool* pSkipped);

    // RYML helpers ///////////////////////////////////////////////////////////

    inline std::string ToString(const ryml::NodeRef& node, size_t bufferSize = 30)
    {
        std::ostringstream stream;
        stream << node;
        std::string buffer = stream.str();
        if (buffer.size() > bufferSize)
        {
            buffer.erase(bufferSize, std::string::npos);
        }
        return buffer;
    }

    inline bool BoolFromYaml(const ryml::NodeRef& node, bool default_, bool* pValid = nullptr)
    {
        if (pValid)
        {
            *pValid = false;
        }

        if (!node.valid())
        {
            return default_;
        }

        if (!node.is_keyval())
        {
            NV_PERF_LOG_WRN(50, "Failed parsing boolean from \"%s\". Using default.\n", ToString(node).c_str());
            return default_;
        }

        bool value = false;

        try
        {
            node >> value;
        }
        catch (std::runtime_error&)
        {
            // error.what() is useless: ":0:0 (0B): ERROR: could not deserialize value"
            NV_PERF_LOG_WRN(50, "Failed parsing boolean from \"%s\". Using default.\n", ToString(node).c_str());
            return default_;
        }

        if (pValid)
        {
            *pValid = true;
        }
        return value;
    }

    // Color //////////////////////////////////////////////////////////////////

    class Color
    {
    private:
        uint32_t m_rgba;
    public:
        Color() : m_rgba(Color::Invalid().Rgba()) {}
        explicit Color(uint32_t rgba) : m_rgba(rgba) {}

        uint32_t Rgba() const
        {
            return m_rgba;
        }
        uint32_t Abgr() const
        {
            uint32_t r = (m_rgba >> 24) & 0xff;
            uint32_t g = (m_rgba >> 16) & 0xff;
            uint32_t b = (m_rgba >>  8) & 0xff;
            uint32_t a = (m_rgba >>  0) & 0xff;
            return (a << 24) + (b << 16) + (g << 8) + (r << 0);
        }
        std::array<float, 4> RgbaVecF() const
        {
            uint32_t r = (m_rgba >> 24) & 0xff;
            uint32_t g = (m_rgba >> 16) & 0xff;
            uint32_t b = (m_rgba >> 8) & 0xff;
            uint32_t a = (m_rgba >> 0) & 0xff;

            std::array<float, 4> vec;
            vec[0] = float(r) / float(0xff);
            vec[1] = float(g) / float(0xff);
            vec[2] = float(b) / float(0xff);
            vec[3] = float(a) / float(0xff);
            return vec;
        }

        bool IsValid() const
        {
            return m_rgba != Color::Invalid().Rgba();
        }

        bool operator==(const Color& other) const
        {
            return m_rgba == other.m_rgba;
        }

        bool operator!=(const Color& other) const
        {
            return m_rgba != other.m_rgba;
        }

        static Color Rgba(uint32_t rgba)
        {
            return Color(rgba);
        }

        static Color FromYaml(const ryml::NodeRef& node, const Color& default_ = Color::Invalid(), bool* pValid = nullptr)
        {
            if (pValid)
            {
                *pValid = false;
            }

            if (!node.valid())
            {
                return default_;
            }

            if (!node.is_keyval())
            {
                NV_PERF_LOG_WRN(50, "Failed parsing color from %s. Using default.\n", ToString(node).c_str());
                return default_;
            }

            std::string colorString;
            node >> colorString;
            if (colorString.rfind("0x", 0) != 0 || !(colorString.size() == 8 || colorString.size() == 10) || !node.val().is_unsigned_integer())
            {
                NV_PERF_LOG_WRN(50, "Failed parsing color from \"%s\". Using default.\n", ToString(node).c_str());
                return default_;
            }

            uint32_t rgbaColor = 0;
            node >> rgbaColor;

            // add missing alpha component
            if (colorString.size() == 8)
            {
                rgbaColor = (rgbaColor << 8) + 0xff;
            }

            if (pValid)
            {
                *pValid = true;
            }
            return Color::Rgba(rgbaColor);
        }

        static Color White     () { return Color::Rgba(0xffffffffU); }
        static Color Black     () { return Color::Rgba(0x000000ffU); }
        static Color Red       () { return Color::Rgba(0xff0000ffU); }
        static Color Green     () { return Color::Rgba(0x00ff00ffU); }
        static Color Blue      () { return Color::Rgba(0x0000ffffU); }
        static Color Yellow    () { return Color::Rgba(0xffff00ffU); }
        static Color Pink      () { return Color::Rgba(0xff00ffffU); }
        static Color Turquoise () { return Color::Rgba(0x00ffffffU); }
        static Color Invalid   () { return Color::Rgba(0x00000000U); }
    };

    inline std::ostream& operator<<(std::ostream& os, const Color& color)
    {
        std::ios_base::fmtflags flags(os.flags());
        os << std::hex << color.Rgba();
        os.flags(flags);
        return os;
    }

    // StyledText /////////////////////////////////////////////////////////////
    
    class StyledText
    {
    public:
        std::string text;
        Color color;

        StyledText() = default;
        StyledText(const std::string& text_) : text(text_), color() {}
        StyledText(const std::string& text_, const Color& color_) : text(text_), color(color_) {}

        static StyledText FromYaml(const ryml::NodeRef& node, const StyledText& default_ = StyledText(), bool* pValid = nullptr)
        {
            if (pValid)
            {
                *pValid = false;
            }

            if (!node.valid())
            {
                return default_;
            }

            std::string text;
            Color color;
            auto defaultColor = default_.color;

            // Support both
            //     label:
            //       - text: ScalarText 1
            //         color : 0xff0000
            // and
            //     label: ScalarText 1
            if (node.is_seq())
            {
                if (node.num_children() > 1)
                {
                    NV_PERF_LOG_WRN(50, "Failed parsing StyledText from \"%s\". Using default.\n", ToString(node).c_str());
                    return default_;
                }
                auto textNode  = node[0].find_child("text");
                auto colorNode = node[0].find_child("color");

                if (!textNode.valid() || !textNode.is_keyval())
                {
                    NV_PERF_LOG_WRN(50, "Missing or invalid text. Using default.\n");
                    return default_;
                }
                textNode >> text;

                if (colorNode.valid())
                {
                    bool valid;
                    color = Color::FromYaml(colorNode, default_.color, &valid);
                    if (!valid)
                    {
                        NV_PERF_LOG_WRN(50, "Failed parsing color from \"%s\". Using default.\n", ToString(colorNode).c_str());
                    }
                }
                else
                {
                    color = default_.color;
                }
            }
            else
            {
                if (!node.is_keyval())
                {
                    NV_PERF_LOG_WRN(50, "Failed parsing StyledText from \"%s\". Using default.\n", ToString(node).c_str());
                    return default_;
                }
                node >> text;
                color = defaultColor;

            }
            if (pValid)
            {
                *pValid = true;
            }
            return StyledText(text, color);
        }
    };

    inline std::ostream& operator<<(std::ostream& os, const StyledText& text)
    {
        std::ios_base::fmtflags flags(os.flags());
        os << "StyledText(" << text.text << ", " << std::hex << std::setfill('0') << std::setw(8) << text.color << ")";
        os.flags(flags);
        return os;
    }

    // RingBuffer /////////////////////////////////////////////////////////////

    template <typename TValue>
    class RingBuffer
    {
    public:
        using ContainerType  = std::vector<TValue>;
        using ValueType      = typename ContainerType::value_type;
        using ConstPointer   = typename ContainerType::const_pointer;
        using ConstReference = typename ContainerType::const_reference;
        using SizeType       = typename ContainerType::size_type;

    private:
        ContainerType m_data;
        SizeType m_maxSize;
        SizeType m_writeIndex;

    public:
        RingBuffer() : m_data(), m_maxSize(0), m_writeIndex(0) {}
        RingBuffer(SizeType maxSize_) : m_data(), m_maxSize(maxSize_), m_writeIndex(0)
        {
            m_data.reserve(maxSize_);
        }

        SizeType WriteIndex() const
        {
            return m_writeIndex;
        }

        SizeType Size() const
        {
            return m_data.size();
        }

        SizeType MaxSize() const
        {
            return m_maxSize;
        }

        ConstPointer Data() const
        {
            return m_data.data();
        }

        ConstReference Get(SizeType index) const
        {
    #if !defined(NDEBUG)
            if (index >= m_maxSize)
            {
                NV_PERF_LOG_ERR(20, "Out of bounds access\n");
                std::abort();
            }
    #endif

            if (Size() == m_maxSize)
            {
                if (m_writeIndex + index < m_maxSize)
                {
                    return m_data[m_writeIndex + index];
                }
                else
                {
                    return m_data[(m_writeIndex + index) % m_maxSize];
                }
            }
            else
            {
                return m_data[index];
            }
        }

        ConstReference Front() const
        {
    #if !defined(NDEBUG)
            if (Size() == 0)
            {
                NV_PERF_LOG_ERR(20, "Size() == 0\n");
                std::abort();
            }
    #endif

            if (m_writeIndex > 0)
            {
                return m_data[m_writeIndex - 1];
            }
            else
            {
                return m_data[m_data.size()-1];
            }
        }

        ConstReference Back() const
        {
    #if !defined(NDEBUG)
            if (Size() == 0)
            {
                NV_PERF_LOG_ERR(20, "Size() == 0\n");
                std::abort();
            }
    #endif

            if (Size() < m_maxSize)
            {
                return m_data[0];
            }
            else
            {
                return m_data[m_writeIndex];
            }
        }

        void Push(ConstReference value)
        {
            if (Size() < m_maxSize)
            {
                m_data.emplace_back(value);
            }
            else
            {
                m_data[m_writeIndex] = value;
            }

            if (m_writeIndex < m_maxSize - 1)
            {
                ++m_writeIndex;
            }
            else
            {
                m_writeIndex  = 0;
            }
        }

        void Print(std::ostream& os, bool printValues = false, const std::string indent = std::string()) const
        {
            auto precision = os.precision();
            os.precision(2);

            os << indent << "RingBuffer(" << m_maxSize << ", " << m_writeIndex;
            if (printValues)
            {
                os << "{";
                for (SizeType index = 0; index < Size(); ++index)
                {
                    os << std::fixed << Get(index);
                    if (index < Size() - 1)
                    {
                        os << ", ";
                    }
                }
                os << "}";
            }

            os <<  ")" << std::endl;
            os.precision(precision);
        }
    };

    // MetricSignal ///////////////////////////////////////////////////////////

    class MetricSignal
    {
    public:
        static std::string HideUnit()
        {
            return "-";
        }

        StyledText label;             // tooltip and TimePlot legend
        std::string description;      // for tooltip
        std::string metric;           // the base name, which can be queried for metadata through LOP, and automatically converted to value and %
        Color color;                  
        double maxValue;              // default from LOP (if needed for ScalarText::ShowValue::ValueWithMax), but can be overridden
        double multiplier;
        std::string unit;             // default from LOP, but can be overwritten
                                  
        size_t metricIndex;           // set by HudDataModel::Initialize()
        size_t metricIndexMaxValue;   // set by HudDataModel::Initialize()
        size_t maxNumSamples;         // set by HudDataModel::Initialize()
        RingBuffer<double> valBuffer; // set by HudDataModel::Initialize()

        MetricSignal() : label(), description(), metric(), color(), maxValue(std::numeric_limits<double>::quiet_NaN()), multiplier(1.0), unit(), metricIndex(0), metricIndexMaxValue((size_t)~0), maxNumSamples(0), valBuffer() {}
        MetricSignal(
            StyledText label_
            , std::string description_
            , std::string metric_
            , const Color& color_
            , double maxValue_
            , double multiplier_
            , std::string unit_)
            : label(label_)
            , description(description_)
            , metric(metric_)
            , color(color_)
            , maxValue(maxValue_)
            , multiplier(multiplier_)
            , unit(unit_)
            , metricIndex(0)
            , metricIndexMaxValue((size_t)~0)
            , maxNumSamples(0)
            , valBuffer(0)
        {
        }

        static MetricSignal FromYaml(const ryml::NodeRef& node, bool* pValid, const std::string& chipName, bool* pSkipped)
        {
            if (pValid)
            {
                *pValid = false;
            }
            if (pSkipped)
            {
                *pSkipped = false;
            }

            if (!node.valid())
            {
                NV_PERF_LOG_ERR(20, "Invalid root node\n");
                return MetricSignal();
            }

            StyledText label;
            std::string description;
            std::string metric;
            Color color;
            double maxValue   = std::numeric_limits<double>::quiet_NaN();
            double multiplier = std::numeric_limits<double>::quiet_NaN();
            std::string unit;
            std::vector<std::string> dedicatedChips;

            if (node.is_keyval() || node.is_val()) // keyval from ScalarText can be a minimal entry in a dashed list
            {
                node >> metric;
            }
            else if (node.is_seq()) // more complex entry from ScalarText needs to be redirected
            {
                if (node.num_children() != 1 || !node.child(0).valid() || !node.child(0).is_map())
                {
                    NV_PERF_LOG_ERR(20, "Failed parsing MetricSignal from \"%s\"\n", ToString(node).c_str());
                    return MetricSignal();
                }

                return MetricSignal::FromYaml(node.child(0), pValid, chipName, pSkipped);
            }
            else
            {
                if (!node.is_map())
                {
                    NV_PERF_LOG_ERR(20, "Failed parsing MetricSignal from \"%s\"\n", ToString(node).c_str());
                    return MetricSignal();
                }

                auto labelNode       = node.find_child("label");
                auto descriptionNode = node.find_child("description");
                auto metricNode      = node.find_child("metric");
                auto colorNode       = node.find_child("color");
                auto maxNode         = node.find_child("max");
                auto multiplierNode  = node.find_child("multiplier");
                auto unitNode        = node.find_child("unit");
                auto dedicatedChipsNode = node.find_child("dedicatedChips");

                if (labelNode.valid())
                {
                    bool valid;
                    label = StyledText::FromYaml(labelNode, StyledText(), &valid);
                    if (!valid)
                    {
                        NV_PERF_LOG_ERR(20, "Failed parsing label from \"%s\"\n", ToString(labelNode).c_str());
                        return MetricSignal();
                    }
                }

                if (descriptionNode.valid())
                {
                    if (!descriptionNode.is_keyval())
                    {
                        NV_PERF_LOG_ERR(20, "Failed parsing description from \"%s\"\n", ToString(descriptionNode).c_str());
                        return MetricSignal();
                    }
                    descriptionNode >> description;
                }

                if (!metricNode.valid() || !metricNode.is_keyval())
                {
                    NV_PERF_LOG_ERR(20, "Missing or invalid metric\n");
                    return MetricSignal();
                }
                metricNode >> metric;
                
                color = Color::FromYaml(colorNode);

                if (maxNode.valid())
                {
                    if (!maxNode.is_keyval() || !maxNode.val().is_number())
                    {
                        NV_PERF_LOG_ERR(20, "Failed parsing max from \"%s\"\n", ToString(maxNode).c_str());
                        return MetricSignal();
                    }
                    maxNode >> maxValue;
                    if (maxValue <= 0)
                    {
                        NV_PERF_LOG_WRN(50, "max %lf needs to be greater than 0. Using default.\n", maxValue);
                        maxValue = std::numeric_limits<double>::quiet_NaN();
                    }
                }

                if (multiplierNode.valid())
                {
                    if (!multiplierNode.is_keyval() || !multiplierNode.val().is_number())
                    {
                        NV_PERF_LOG_ERR(20, "Failed parsing multiplier from \"%s\"\n", ToString(multiplierNode).c_str());
                        return MetricSignal();
                    }
                    multiplierNode >> multiplier;
                    if (multiplier <= 0)
                    {
                        NV_PERF_LOG_WRN(50, "Multiplier %lf needs to be greater than 0. Using default.\n", multiplier);
                        multiplier = std::numeric_limits<double>::quiet_NaN();
                    }
                }

                if (unitNode.valid())
                {
                    if (!unitNode.is_keyval())
                    {
                        NV_PERF_LOG_ERR(20, "Failed parsing unit from \"%s\"\n", ToString(unitNode).c_str());
                        return MetricSignal();
                    }
                    unitNode >> unit;

                    if (unit.empty()) // if unit is set, but empty it should be hidden
                    {
                        unit = MetricSignal::HideUnit();
                    }
                }

                if (dedicatedChipsNode.valid())
                {
                    if (!dedicatedChipsNode.is_seq())
                    {
                        NV_PERF_LOG_WRN(100, "Invalid dedicatedChips\n");
                    }
                    else
                    {
                        for (const ryml::NodeRef& dedicatedChipNode : dedicatedChipsNode.children())
                        {
                            if (!dedicatedChipNode.valid() || !dedicatedChipNode.is_val())
                            {
                                NV_PERF_LOG_WRN(100, "Missing or invalid dedicatedChip\n");
                                continue;
                            }
                            std::string dedicatedChip;
                            dedicatedChipNode >> dedicatedChip;
                            dedicatedChips.emplace_back(dedicatedChip);
                        }
                    }
                }
            }

            if (!std::regex_match(metric, std::regex("^[A-Za-z][A-Za-z0-9._]+$")))
            {
                NV_PERF_LOG_ERR(20, "Invalid metric \"%s\"\n", metric.c_str());
                return MetricSignal();
            }
            if (pSkipped)
            {
                if (!dedicatedChips.empty())
                {
                    if (std::find(dedicatedChips.begin(), dedicatedChips.end(), chipName) == dedicatedChips.end())
                    {
                        NV_PERF_LOG_INF(20, "Skip undedicated metric on %s: %s\n", chipName.c_str(), metric.c_str());
                        *pSkipped = true;
                    }
                }
            }

            if (pValid)
            {
                *pValid = true;
            }

            return MetricSignal(label, description, metric, color, maxValue, multiplier, unit);
        }

        // resets valBuffer
        void SetMaxNumSamples(size_t count)
        {
            maxNumSamples = count;
            valBuffer = RingBuffer<double>(maxNumSamples);
        }

        void SetMetricIndex(size_t index)
        {
            metricIndex = index;
        }

        void SetMetricIndexMaxValue(size_t index)
        {
            metricIndexMaxValue = index;
        }

        void AddSample(double value)
        {
            valBuffer.Push(value * (std::isnan(multiplier) ? 1.0 : multiplier));
        }

        void SetMaxValue(double value)
        {
            maxValue = value * (std::isnan(multiplier) ? 1.0 : multiplier);
        }

        void Print(std::ostream& os, bool printValues = false, const std::string indent = std::string()) const
        {

            auto precision = os.precision();
            std::ios_base::fmtflags flags(os.flags());
            os << indent << "MetricSignal("
                << label << ", "
                << description << ", "
                << metric << ", "
                << std::hex << color << std::dec << ", "
                << std::setprecision(1) << maxValue << ", "
                << multiplier << ", "
                << unit << ", "
                << metricIndex << ", "
                << metricIndexMaxValue << ", "
                << maxNumSamples << ", " << std::endl;
            os.flags(flags);
            os.precision(precision);

            valBuffer.Print(os, printValues, indent + "  ");

            os << indent << ")" << std::endl;
        }
    };

    // Widgets ////////////////////////////////////////////////////////////////

    class Widget
    {
    public:
        enum class Type : uint32_t
        {
            Invalid,
            Panel,
            Separator,
            ScalarText,
            TimePlot,
            Count
        };

        const Type type;

        Widget() = delete;
        virtual ~Widget() {}
    protected:
        Widget(Type type_) : type(type_) {}
        Widget(const Widget& other) = default;
        Widget(Widget&& other) = default;
        Widget& operator=(const Widget&)
        {
            return *this;
        }
        Widget& operator=(Widget&&)
        {
            return *this;
        }
    public:
        virtual std::unique_ptr<Widget> Clone() const = 0;

        virtual void Print(std::ostream& os, const std::string indent = std::string()) const
        {
            os << indent << "Widget(" << (const void*)this << ")" << std::endl;
        }
    };

    class Panel : public Widget
    {
    public:
        std::string name;
        StyledText label;
        bool defaultOpen; // set when adding the Panel into the HudConfiguration, not at creation
        std::vector<std::unique_ptr<Widget>> widgets;

        Panel() : Widget(Widget::Type::Panel), name(), label(), defaultOpen(true), widgets() {}

        Panel(const Panel& other) : Widget(Widget::Type::Panel)
        {
            name        = other.name;
            label       = other.label;
            defaultOpen = other.defaultOpen;
            for (const auto& pWidget : other.widgets)
            {
                widgets.emplace_back(pWidget->Clone());
            }
        }
        Panel(Panel&& other) = default;
        Panel(const std::string& name_, const StyledText& label_, bool defaultOpen_, std::vector<std::unique_ptr<Widget>>&& widgets_)
            : Widget(Widget::Type::Panel)
        {
            name        = name_;
            label       = label_;
            defaultOpen = defaultOpen_;
            widgets     = std::move(widgets_);
        }
        Panel& operator=(const Panel& other)
        {
            name        = other.name;
            label       = other.label;
            defaultOpen = other.defaultOpen;
            widgets.clear();
            for (const auto& pWidget : other.widgets)
            {
                widgets.emplace_back(pWidget->Clone());
            }
            return *this;
        }
        Panel& operator=(Panel&& other) = default;

        ~Panel()
        {
            name        = std::string();
            label       = StyledText();
            defaultOpen = false;
            widgets.clear();
        }

        static Panel FromYaml(const ryml::NodeRef& node, bool* pValid, const std::string& chipName)
        {
            if (pValid)
            {
                *pValid = false;
            }
            
            auto nameNode        = node.find_child("name");
            auto labelNode       = node.find_child("label");
            auto widgetsNode     = node.find_child("widgets");
            
            if (!nameNode.valid() || !nameNode.is_keyval())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid name\n");
                return Panel();
            }
            std::string name;
            nameNode >> name;
            
            if (name.empty())
            {
                NV_PERF_LOG_ERR(20, "Name cannot be empty\n");
                return Panel();
            }

            StyledText label = StyledText::FromYaml(labelNode, StyledText(name));
            
            if (!widgetsNode.valid() || !widgetsNode.is_seq())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid widgets\n");
                return Panel();
            }

            std::vector<std::unique_ptr<Widget>> widgets;
            for (const ryml::NodeRef& widgetNode : widgetsNode.children())
            {
                bool valid;
                bool skipped;
                std::unique_ptr<Widget> widget = WidgetFromYaml(widgetNode, &valid, chipName, &skipped);
                if (!valid)
                {
                    NV_PERF_LOG_ERR(20, "Missing or invalid widget\n");
                    return Panel();
                }
                if (!skipped)
                {
                    widgets.emplace_back(std::move(widget));
                }
            }

            if (pValid)
            {
                *pValid = true;
            }
            auto panel = Panel(name, label, true, std::move(widgets));
            return panel;
        }

        virtual std::unique_ptr<Widget> Clone() const override
        {
            auto panel = std::make_unique<Panel>(*this);
            return panel;
        }

        void Print(std::ostream& os, const std::string indent = std::string()) const override
        {
            os << indent << "Panel(" << name << ", " << label << ", " << defaultOpen << std::endl;
            for (const auto& widget : widgets)
            {
                if (widget)
                {
                    widget->Print(os, indent + "  ");
                }
                else
                {
                    os << indent + "  " << "Widget(nullptr)" << std::endl;
                }
            }
            os << indent << ")" << std::endl;
        }
    };


    class ScalarText : public Widget
    {
    public:

        enum class ShowValue : uint32_t
        {
            JustValue,
            ValueWithMax,
            Hide,
            Count
        };

        StyledText label;
        MetricSignal signal;
        int decimalPlaces;
        ShowValue showValue;

        ScalarText() : Widget(Widget::Type::ScalarText), label(), signal(), decimalPlaces(1), showValue(ShowValue::JustValue) {}
        ScalarText(const StyledText& label_, MetricSignal metric_, int decimalPlaces_, ShowValue showValue_)
            : Widget(Widget::Type::ScalarText), label(label_), signal(metric_), decimalPlaces(decimalPlaces_), showValue(showValue_) {}

        static ScalarText FromYaml(const ryml::NodeRef& node, bool* pValid, const std::string& chipName, bool* pSkipped)
        {
            if (pValid)
            {
                *pValid = false;
            }
            if (pSkipped)
            {
                *pSkipped = false;
            }

            auto labelNode         = node.find_child("label");
            auto metricNode        = node.find_child("metric");
            auto decimalPlacesNode = node.find_child("decimalPlaces");
            auto showValueNode     = node.find_child("showValue");

            bool valid;
            StyledText label = StyledText::FromYaml(labelNode, StyledText(), &valid);
            if (!valid)
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid label\n");
                return ScalarText();
            }

            MetricSignal metric = MetricSignal::FromYaml(metricNode, &valid, chipName, pSkipped);
            if (!valid)
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid metric\n");
                return ScalarText();
            }

            const int defaultDecimalPlaces = 1;
            int decimalPlaces = defaultDecimalPlaces;
            if (decimalPlacesNode.valid())
            {
                if (decimalPlacesNode.is_keyval() && decimalPlacesNode.val().is_unsigned_integer())
                {
                    decimalPlacesNode >> decimalPlaces;
                }
                else
                {
                    NV_PERF_LOG_WRN(50, "Failed parsing decimalPlaces from \"%s\". Using default.\n", ToString(decimalPlacesNode).c_str());
                }
            }

            ShowValue showValue;
            if (!showValueNode.valid())
            {
                showValue = ShowValue::JustValue;
            }
            else
            {
                std::string showValueString;
                showValueNode >> showValueString;

                if (showValueString == "JustValue")
                {
                    showValue = ShowValue::JustValue;
                }
                else if (showValueString == "ValueWithMax")
                {
                    showValue = ShowValue::ValueWithMax;
                }
                else if (showValueString == "Hide")
                {
                    showValue = ShowValue::Hide;
                }
                else
                {
                    NV_PERF_LOG_WRN(50, "Failed parsing ShowValue from \"%s\". Using default.\n", ToString(showValueNode).c_str());
                    showValue = ShowValue::JustValue;
                }
            }

            if (pValid)
            {
                *pValid = true;
            }
            return ScalarText(label, metric, decimalPlaces, showValue);
        }

        virtual std::unique_ptr<Widget> Clone() const override
        {
            auto scalarText = std::make_unique<ScalarText>(*this);
            return scalarText;
        }

        void Print(std::ostream& os, const std::string indent = std::string()) const override
        {
            os << indent << "ScalarText(" << label << std::endl;
            signal.Print(os, false, indent + "  ");
            os << indent + "  " << decimalPlaces << ", " << static_cast<std::underlying_type<ShowValue>::type>(showValue) << ", " << std::endl;
            os << indent << ")" << std::endl;
        }
    };

    class Separator : public Widget
    {
    public:
        Separator() : Widget(Widget::Type::Separator) {}

        static Separator FromYaml(const ryml::NodeRef& node, bool* pValid)
        {
            if (pValid)
            {
                *pValid = false;
            }

            if (!node.valid() || node.has_val() || node.is_seq() || (node.is_map() && node.num_children() > 1))
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid Separator\n");
                return Separator();
            }

            if (pValid)
            {
                *pValid = true;
            }
            return Separator();
        }

        virtual std::unique_ptr<Widget> Clone() const override
        {
            auto separator = std::make_unique<Separator>(*this);
            return separator;
        }

        void Print(std::ostream& os, const std::string indent = std::string()) const override
        {
            os << indent << "Separator()" << std::endl;
        }
    };

    class TimePlot : public Widget
    {
    public:

        enum class ChartType : uint32_t
        {
            Overlay,
            Stacked,
            Count
        };

        static std::string HideUnit()
        {
            return MetricSignal::HideUnit();
        }

        StyledText label;
        std::string unit;
        ChartType chartType;
        double valueMin;
        double valueMax;
        double timeWidth;                           // set by HudDataModel::Initialize()
        RingBuffer<double> const* pTimestampBuffer; // set by HudDataModel::Initialize()
        std::vector<MetricSignal> signals;          // set by HudDataModel::Initialize()
        std::vector<MetricSignal> stackedSignals;   // set by HudDataModel::Initialize()

        TimePlot()
            : Widget(Widget::Type::TimePlot)
            , label()
            , unit()
            , chartType(ChartType::Overlay)
            , valueMin(std::numeric_limits<double>::quiet_NaN())
            , valueMax(std::numeric_limits<double>::quiet_NaN())
            , timeWidth(std::numeric_limits<double>::quiet_NaN())
            , pTimestampBuffer(nullptr)
            , signals()
            , stackedSignals()
        {
        }
        TimePlot(
            const StyledText& label_
            , std::string unit_
            , ChartType chartType_
            , double valueMin_
            , double valueMax_
            , RingBuffer<double> const* timeBuffer_
            , std::vector<MetricSignal> metrics_)
            : Widget(Widget::Type::TimePlot)
            , label(label_)
            , unit(unit_)
            , chartType(chartType_)
            , valueMin(valueMin_)
            , valueMax(valueMax_)
            , timeWidth(std::numeric_limits<double>::quiet_NaN())
            , pTimestampBuffer(timeBuffer_)
            , signals(metrics_)
            , stackedSignals(chartType_ == ChartType::Stacked ? metrics_ : std::vector<MetricSignal>())
        {
        }

        static TimePlot FromYaml(const ryml::NodeRef& node, bool* pValid, const std::string& chipName, bool* pSkipped)
        {
            if (pValid)
            {
                *pValid = false;
            }
            if (pSkipped)
            {
                *pSkipped = false;
            }

            auto labelNode       = node.find_child("label");
            auto unitNode        = node.find_child("unit");
            auto chartTypeNode   = node.find_child("chartType");
            auto valueMinNode    = node.find_child("valueMin");
            auto valueMaxNode    = node.find_child("valueMax");
            auto metricsNode     = node.find_child("metrics");

            StyledText label;
            if (labelNode.valid())
            {
                bool valid;
                label = StyledText::FromYaml(labelNode, StyledText(), &valid);
                if (!valid)
                {
                    NV_PERF_LOG_ERR(20, "Failed parsing label from \"%s\"\n", ToString(labelNode).c_str());
                    return TimePlot();
                }
            }

            std::string unit;
            if (unitNode.valid())
            {
                if (!unitNode.is_keyval())
                {
                    NV_PERF_LOG_ERR(20, "Failed parsing unit from \"%s\"\n", ToString(unitNode).c_str());
                    return TimePlot();
                }
                unitNode >> unit;

                if (unit.empty()) // if unit is set, but empty it should be hidden
                {
                    unit = TimePlot::HideUnit();
                }
            }

            if (!chartTypeNode.valid() || !chartTypeNode.is_keyval())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid chartType\n");
                return TimePlot();
            }
            ChartType chartType;
            std::string chartTypeString;
            chartTypeNode >> chartTypeString;
            if (chartTypeString == "Overlay")
            {
                chartType = ChartType::Overlay;
            }
            else if (chartTypeString == "Stacked")
            {
                chartType = ChartType::Stacked;
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Invalid chart type \"%s\"\n", chartTypeString.c_str());
                return TimePlot();
            }

            double valueMin = std::numeric_limits<double>::quiet_NaN();
            double valueMax = std::numeric_limits<double>::quiet_NaN();
            if (valueMinNode.valid())
            {
                if (!valueMinNode.is_keyval() || !valueMinNode.val().is_number())
                {
                    NV_PERF_LOG_ERR(20, "Failed parsing valueMin from \"%s\"\n", ToString(valueMinNode).c_str());
                    return TimePlot();
                }
                valueMinNode >> valueMin;
            }

            if (valueMaxNode.valid())
            {
                if (!valueMaxNode.is_keyval() || !valueMaxNode.val().is_number())
                {
                    NV_PERF_LOG_ERR(20, "Failed parsing valueMax from \"%s\"\n", ToString(valueMaxNode).c_str());
                    return TimePlot();
                }
                valueMaxNode >> valueMax;
            }

            if (!metricsNode.valid() || !metricsNode.is_seq())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid metrics\n");
                return TimePlot();
            }

            std::vector<MetricSignal> metrics;
            for (const ryml::NodeRef& metricNode : metricsNode.children())
            {
                bool valid;
                bool skipped;
                MetricSignal metric = MetricSignal::FromYaml(metricNode, &valid, chipName, &skipped);
                if (!valid)
                {
                    NV_PERF_LOG_ERR(20, "Missing or invalid metric\n");
                    return TimePlot();
                }
                if (!skipped)
                {
                    metrics.emplace_back(metric);
                }
            }
            if (pValid)
            {
                *pValid = true;
            }
            return TimePlot(label, unit, chartType, valueMin, valueMax, nullptr, metrics);
        }

        virtual std::unique_ptr<Widget> Clone() const override
        {
            auto timePlot = std::make_unique<TimePlot>(*this);
            return timePlot;
        }

        void SetTimeWidth(double timeWidth_)
        {
            timeWidth = timeWidth_;
        }

        void SetTimestampBuffer(const RingBuffer<double>* pTimestampBuffer_)
        {
            pTimestampBuffer = pTimestampBuffer_;
        }

        void Print(std::ostream& os, const std::string indent = std::string()) const override
        {
            os << indent << "TimePlot(" << label << ", " << unit <<  ", " << (int) chartType << ", " << valueMin << ", " << valueMax << std::endl;
            if (pTimestampBuffer != nullptr)
            {
                pTimestampBuffer->Print(os, false, indent + "  ");
            }
            else
            {
                os << indent + "  RingBuffer(nullptr)" << std::endl;
            }
            for (const auto& signal : signals)
            {
                signal.Print(os, false, indent + "  ");
            }
            for (const auto& signal : stackedSignals)
            {
                signal.Print(os, false, indent + "  ");
            }
            os << indent << ")" << std::endl;
        }
    };

    // WidgetFromYaml /////////////////////////////////////////////////////////

    inline std::unique_ptr<Widget> WidgetFromYaml(const ryml::NodeRef& node, bool* pValid, const std::string& chipName, bool* pSkipped)
    {
        if (pValid)
        {
            *pValid = false;
        }

        if (pSkipped)
        {
            *pSkipped = false;
        }

        if (!node.valid() || !node.is_map())
        {
            NV_PERF_LOG_ERR(20, "Missing or invalid widget\n");
            return nullptr;
        }

        auto typeNode = node.find_child("type");
        if (!typeNode.valid() || !typeNode.is_keyval())
        {
            NV_PERF_LOG_ERR(20, "Missing or invalid type\n");
            return nullptr;
        }
        std::string type;
        typeNode >> type;

        if (type == "Panel")
        {
            NV_PERF_LOG_ERR(20, "Panels cannot be nested\n");
        }
        else if (type == "ScalarText")
        {
            return std::make_unique<ScalarText>(ScalarText::FromYaml(node, pValid, chipName, pSkipped));
        }
        else if (type == "Separator")
        {
            *pSkipped = false;
            return std::make_unique<Separator>(Separator::FromYaml(node, pValid));
        }
        else if (type == "TimePlot")
        {
            return std::make_unique<TimePlot>(TimePlot::FromYaml(node, pValid, chipName, pSkipped));
        }
        else
        {
            NV_PERF_LOG_ERR(20, "Unkown widget type \"%s\" under Panel\n", type.c_str());
        }

        return nullptr;
    }

    // ScopedRymlErrorHandler /////////////////////////////////////////////////

    // Makes sure no error calls ::abort() outright and crashes the program
    class ScopedRymlErrorHandler
    {
    private:
        ryml::Callbacks m_defaultCallbacks;

        static void OnError(const char* message, size_t length, ryml::Location loc, void* userData)
        {
            (void)userData;
            std::string errorString = ryml::formatrs<std::string>("{}:{}:{} ({}B): ERROR: {}", loc.name, loc.line, loc.col, loc.offset, ryml::csubstr(message, length));
            throw std::runtime_error(errorString.c_str());
        }
    public:
        ScopedRymlErrorHandler()
        {
            m_defaultCallbacks = ryml::get_callbacks();
            ryml::set_callbacks(ryml::Callbacks(this, nullptr, nullptr, ScopedRymlErrorHandler::OnError));
        }
        virtual ~ScopedRymlErrorHandler()
        {
            ryml::set_callbacks(m_defaultCallbacks);
        }
    };

    // HudPresets /////////////////////////////////////////////////////////////

    class HudPreset
    {
    public:
        std::string name;
        std::string chipName; // plumbed through to HudDataModel

        const char* pYaml; // 0-terminated
        std::string fileName;

        HudPreset() : name(), chipName(), pYaml(nullptr), fileName() {}
        HudPreset(const std::string& name_, const std::string& chipName_, const char* pYaml_, const std::string& fileName_)
            : name(name_), chipName(chipName_), pYaml(pYaml_), fileName(fileName_) {}

        static HudPreset FromYaml(const ryml::NodeRef& node, const char* pYaml, const std::string& fileName, bool* pValid, const std::string& chipName)
        {
            if (pValid)
            {
                *pValid = false;
            }

            if (!node.valid() || !node.is_map() )
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid configuration\n");
                return HudPreset();
            }

            auto nameNode = node.find_child("name");
            if (!nameNode.valid() || !nameNode.is_keyval())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid name\n");
                return HudPreset();
            }
            std::string name;
            nameNode >> name;

            if (pValid)
            {
                *pValid = true;
            }
            return HudPreset(name, chipName, pYaml, fileName);
        }

        bool IsValid() const
        {
            return !name.empty();
        }
    };

    inline std::ostream& operator<<(std::ostream& os, const HudPreset& preset)
    {
        os << "HudPreset(" << preset.name << ", " << (const void*) preset.pYaml << ", " << preset.fileName << ")";
        return os;
    }

    class HudPresets
    {
    private:
        std::string m_chipName;
        std::vector<std::string> m_loadedFiles;
        std::vector<HudPreset> m_presets;
    public:
        HudPresets() = default;

        bool Initialize(const std::string& chipName, bool loadPredefinedPresets = true)
        {
            m_loadedFiles.clear();
            m_presets.clear();

            m_chipName = chipName;

            if (loadPredefinedPresets)
            {
                size_t bakedConfigurationsSize            = HudConfigurations::GetHudConfigurationsSize(chipName.c_str());
                const char** bakedConfigurationsFileNames = HudConfigurations::GetHudConfigurationsFileNames(chipName.c_str());
                const char** bakedConfigurations          = HudConfigurations::GetHudConfigurations(chipName.c_str());

                if (bakedConfigurationsSize == 0)
                {
                    return false;
                }

                for (size_t index = 0; index < bakedConfigurationsSize; ++index)
                {
                    bool success = LoadFromString(bakedConfigurations[index], bakedConfigurationsFileNames[index]);
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "Failed loading baked file \"%s\"\n", bakedConfigurationsFileNames[index]);
                        return false;
                    }
                }
            }

            return true;
        }

        bool LoadFromString(const char* pYaml, const std::string& fileName = std::string())
        {
            ScopedRymlErrorHandler customRymlErrorHandler;

            ryml::Tree tree = ryml::parse_in_arena(ryml::csubstr(fileName.c_str(), fileName.size()), ryml::csubstr(pYaml, strlen(pYaml)));
            ryml::NodeRef root = tree.rootref();

            ryml::NodeRef configurationsNode;

            for (const ryml::NodeRef& node : root.children())
            {
                if (node.key() == "panels")
                {
                    continue;
                }
                else if (node.key() == "configurations")
                {
                    if (configurationsNode.valid())
                    {
                        NV_PERF_LOG_ERR(20, "YAML file can only have one configurations section\n");
                        return false;
                    }
                    configurationsNode = node;
                }
                else
                {
                    NV_PERF_LOG_ERR(20, "Unknown section \"%s\"\n", node.key().data());
                    return false;
                }
            }

            if (configurationsNode.valid() == false)
            {
                NV_PERF_LOG_ERR(20, "Cannot find configurations section\n");
                return false;
            }

            for (const ryml::NodeRef& child : configurationsNode.children())
            {
                // TODO would be good to point pYaml at the preset itself not the file beginning
                // If possible. There might be more to it, since it just pointing strings could break references in yaml.
                bool valid;
                HudPreset preset = HudPreset::FromYaml(child, pYaml, fileName, &valid, m_chipName);
                if (!valid)
                {
                    NV_PERF_LOG_ERR(20, "Missing or invalid configuration\n");
                    return false;
                }

                bool found = false;
                for (const auto& existingPreset : m_presets)
                {
                    if (existingPreset.name == preset.name)
                    {
                        NV_PERF_LOG_WRN(50, "Preset \"%s\" was already loaded from file \"%s\". Skipping to load it again from file \"%s\".\n",
                            preset.name.c_str(), existingPreset.fileName.c_str(), fileName.c_str());
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    m_presets.emplace_back(preset);
                }
            }
            return true;
        }

        bool LoadFile(const std::string& fileName)
        {
            std::ifstream ifs;
            ifs.open(fileName);

            if (ifs.good() == false)
            {
                NV_PERF_LOG_ERR(20, "Cannot open file \"%s\"\n", fileName.c_str());
                return false;
            }

            std::ostringstream buffer;
            buffer << ifs.rdbuf();

            ifs.close();

            m_loadedFiles.emplace_back(buffer.str());

            bool success = LoadFromString(m_loadedFiles.back().c_str(), fileName);
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "Failed loading file \"%s\"\n", fileName.c_str());
                return false;
            }
            return true;
        }

        const HudPreset& GetGraphicsLowSpeedPreset() const
        {
            if (m_presets.size() == 0)
            {
                static HudPreset dummy;
                NV_PERF_LOG_ERR(20, "No presets have been loaded\n");
                return dummy;
            }

            // TODO implement
            return m_presets[0];
        }

        const std::vector<HudPreset>& GetPresets() const
        {
            return m_presets;
        }

        const HudPreset& GetPreset(const std::string& name) const
        {
            static const HudPreset dummy;

            if (m_presets.size() == 0)
            {
                NV_PERF_LOG_ERR(20, "No presets have been loaded\n");
                return dummy;
            }

            for (const HudPreset& preset : m_presets)
            {
                if (preset.name == name)
                {
                    return preset;
                }
            }

            NV_PERF_LOG_ERR(20, "Could not find preset \"%s\"\n", name.c_str());
            return dummy;
        }

        void Print(std::ostream& os, const std::string indent = std::string()) const
        {
            os << indent << "HudPresets( (" << m_loadedFiles.size() << "){";
            for (const auto& file : m_loadedFiles)
            {
                os << "(" << file.size() << "), ";
            }
            os << "}" << std::endl;
            for (const auto& preset : GetPresets())
            {
                os << indent + "  " << preset << std::endl;
            }
            os << indent << ")" << std::endl;
        }
    };

    // HudDataModel ///////////////////////////////////////////////////////////

    class HudConfiguration
    {
    public:

        enum class SamplingSpeed : uint32_t
        {
            Low,
            High,
            Count
        };

        std::string name;
        SamplingSpeed samplingSpeed;
        std::vector<Panel> panels;

        HudConfiguration() : name(), samplingSpeed(SamplingSpeed::Low), panels() {}
        HudConfiguration(std::string name_, SamplingSpeed samplingSpeed_, const std::vector<Panel>& panels_) 
            : name(name_), samplingSpeed(samplingSpeed_), panels(panels_) {}

        static std::string NameFromYaml(const ryml::NodeRef& node, bool* pValid)
        {
            if (pValid)
            {
                *pValid = false;
            }

            if (!node.valid() || !node.is_map())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid configuration\n");
                return std::string();
            }

            auto nameNode = node.find_child("name");
            if (!nameNode.valid() || !nameNode.is_keyval())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid name\n");
                return std::string();
            }

            std::string name;
            nameNode >> name;

            if (pValid)
            {
                *pValid = true;
            }
            return name;
        }

        static HudConfiguration FromYaml(const ryml::NodeRef& node, const std::vector<Panel>& createdPanels, bool* pValid)
        {
            if (pValid)
            {
                *pValid = false;
            }
            if (!node.valid() || !node.is_map())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid configuration\n");
                return HudConfiguration();
            }

            auto nameNode   = node.find_child("name");
            auto speedNode  = node.find_child("speed");
            auto panelsNode = node.find_child("panels");

            if (!nameNode.valid() || !nameNode.is_keyval())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid name\n");
                return HudConfiguration();
            }
            std::string name;
            nameNode >> name;

            if (!speedNode.valid() || !speedNode.is_keyval())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid speed\n");
                return HudConfiguration();
            }
            SamplingSpeed speed;
            std::string speedString;
            speedNode >> speedString;
            if (speedString == "Low")
            {
                speed = SamplingSpeed::Low;
            }
            else if (speedString == "High")
            {
                speed = SamplingSpeed::High;
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Failed parsing speed from \"%s\"\n", ToString(speedNode).c_str());
                return HudConfiguration();
            }

            if (!panelsNode.valid() || !panelsNode.is_seq())
            {
                NV_PERF_LOG_ERR(20, "Missing or invalid panels\n");
                return HudConfiguration();
            }

            std::vector<Panel> panels;
            for (size_t index = 0; index < panelsNode.num_children(); ++index)
            {
                const ryml::NodeRef& panelNode = panelsNode[index];

                std::string panelName;
                bool defaultOpen = true;

                if (panelNode.is_map()) // parses " -name: asdf\n  defaultOpen: True"
                {
                    auto panelNameNode = panelNode.find_child("name");
                    auto panelDefaultOpenNode = panelNode.find_child("defaultOpen");

                    if (!panelNameNode.valid() || !panelNameNode.is_keyval())
                    {
                        NV_PERF_LOG_ERR(20, "Missing or invalid panel name\n");
                        return HudConfiguration();
                    }
                    panelNameNode >> panelName;
                    if (panelDefaultOpenNode.valid())
                    {
                        bool valid;
                        defaultOpen = BoolFromYaml(panelDefaultOpenNode, false, &valid);
                        if (!valid)
                        {
                            NV_PERF_LOG_WRN(50, "Failed parsing defaultOpen from \"%s\". Using default.\n", ToString(panelDefaultOpenNode).c_str());
                        }
                    }

                }
                else // parses the short-hand "- panel_name"
                {
                    panelNode >> panelName;
                }

                bool found = false;
                for (const auto& panel : createdPanels)
                {
                    if (panel.name == panelName)
                    {
                        panels.emplace_back(panel); // Copies the Panel object
                        panels.back().defaultOpen = defaultOpen;
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    NV_PERF_LOG_ERR(20, "Cannot find \"%s\" in panels section\n", panelName.c_str());
                    return HudConfiguration();
                }
            }

            if (pValid)
            {
                *pValid = true;
            }
            return HudConfiguration(name, speed, panels);
        }

        void Print(std::ostream& os, const std::string indent = std::string()) const
        {
            os << indent << "HudConfiguration(" << name << ", " << (int) samplingSpeed << std::endl;
            for (const auto& panel : panels)
            {
                panel.Print(os, indent + "  ");
            }
            os << indent << ")" << std::endl;
        }
    };

    class HudDataModel
    {
    private:
        class CounterConfigBuilder
        {
        private:
            MetricsEvaluator* m_pMetricsEvaluator;
            std::vector<NVPW_MetricEvalRequest>* m_pMetricEvalRequests;
            std::map<std::string, size_t> m_metricNameToIndex;
            MetricsConfigBuilder m_configBuilder;
            const RawCounterSchedulingHints* m_pSchedulingHints;
        
        public:
            CounterConfigBuilder()
                : m_pMetricsEvaluator()
                , m_pMetricEvalRequests()
            {
            }

            bool Initialize(const std::string& chipName, MetricsEvaluator& metricsEvaluator, std::vector<NVPW_MetricEvalRequest>& metricEvalRequests, const RawCounterSchedulingHints& schedulingHints)
            {
                NVPW_RawCounterConfig* pRawCounterConfig = sampler::DeviceCreateRawCounterConfig(chipName.c_str());
                if (!pRawCounterConfig)
                {
                    return false;
                }
                if (!m_configBuilder.Initialize(metricsEvaluator, pRawCounterConfig, chipName.c_str()))
                {
                    return false;
                }
				
                m_pMetricsEvaluator = &metricsEvaluator;
                m_pMetricEvalRequests = &metricEvalRequests;
                m_pSchedulingHints = &schedulingHints;
                return true;
            }

            // returns the relative metric index as in the scheduled metrics array, or "~0" if the input metric is invalid
            size_t AddMetric(const std::string& metric, const NVPW_MetricEvalRequest* pRequest_ = nullptr)
            {
                const size_t InvalidIndex = (size_t)~0;

                const auto itr = m_metricNameToIndex.find(metric);
                if (itr != m_metricNameToIndex.end())
                {
                    return itr->second;
                }

                NVPW_MetricEvalRequest request;
                const NVPW_MetricEvalRequest* pRequest;
                if (pRequest_)
                {
                    pRequest = pRequest_;
                }
                else
                {
                    if (!ToMetricEvalRequest(*m_pMetricsEvaluator, metric.c_str(), request))
                    {
                        return InvalidIndex;
                    }
                    pRequest = &request;
                }

                const bool KeepInstances = false; // Prefer to only keep the gpu values for better performance and storage efficiency
                if (!m_configBuilder.AddMetrics(pRequest, 1, KeepInstances, *m_pSchedulingHints))
                {
                    return InvalidIndex;
                }

                m_pMetricEvalRequests->push_back(*pRequest);
                const size_t metricIndex = m_pMetricEvalRequests->size() - 1;
                m_metricNameToIndex.insert(std::make_pair(metric, metricIndex));
                return metricIndex;
            }

            bool GenerateCounterConfiguration(CounterConfiguration& counterConfiguration)
            {
                if (!CreateConfiguration(m_configBuilder, counterConfiguration))
                {
                    return false;
                }
                if (counterConfiguration.numPasses != 1u)
                {
                    NV_PERF_LOG_ERR(10, "The resultant number of passes is not 1, actual = %llu\n", counterConfiguration.numPasses);
                    return false;
                }
                return true;
            }
        };

        class SampleProcessor
        {
        private:
            MetricsEvaluator* m_pMetricsEvaluator;
            const std::vector<NVPW_MetricEvalRequest>* m_pMetricEvalRequests;
            sampler::FrameLevelSampleCombiner m_combiner;
            std::vector<double> m_perSampleMetricValueScratchBuffer;
            std::vector<double> m_perFrameMetricValuesScratchBuffer;

        public:
            SampleProcessor()
                : m_pMetricsEvaluator()
                , m_pMetricEvalRequests()
                , m_combiner()
            {
            }

            bool Initialize(
                MetricsEvaluator& metricsEvaluator,
                const std::vector<NVPW_MetricEvalRequest>& metricEvalRequests,
                const std::vector<uint8_t>& counterData,
                const std::vector<uint8_t>& counterDataPrefix)
            {
                if (!MetricsEvaluatorSetDeviceAttributes(metricsEvaluator, counterData.data(), counterData.size()))
                {
                    return false;
                }

                sampler::CounterDataInfo counterDataInfo{};
                bool success = CounterDataGetInfo(counterData.data(), counterData.size(), counterDataInfo);
                if (!success)
                {
                    return false;
                }
                const uint32_t maxSampleLatency = (std::max)(64u, counterDataInfo.numTotalRanges);
                if (!m_combiner.Initialize(counterDataPrefix, counterData, maxSampleLatency))
                {
                    return false;
                }

                m_pMetricsEvaluator = &metricsEvaluator;
                m_pMetricEvalRequests = &metricEvalRequests;
                m_perSampleMetricValueScratchBuffer.resize(metricEvalRequests.size());
                m_perFrameMetricValuesScratchBuffer.resize(metricEvalRequests.size());
                return true;
            }

            void Reset()
            {
                m_pMetricsEvaluator = nullptr;
                m_pMetricEvalRequests = nullptr;
                m_combiner.Reset();
                m_perSampleMetricValueScratchBuffer.clear();
                m_perFrameMetricValuesScratchBuffer.clear();
            }

            bool AddSample(
                const uint8_t* pCounterDataImage,
                size_t counterDataImageSize,
                uint32_t rangeIndex)
            {
                bool success = m_combiner.AddSample(pCounterDataImage, counterDataImageSize, rangeIndex);
                if (!success)
                {
                    return false;
                }
                return true;
            }

            bool IsFrameDataComplete(uint64_t frameEndTime) const
            {
                return m_combiner.IsDataComplete(frameEndTime);
            }

            // "TConsumeSampleFunc" is expected to be in the form of bool(const std::vector<double>& metricValues)
            template <typename TConsumeSampleFunc>
            bool GetFrameLevelCombinedSampleValues(uint64_t frameEndTime, TConsumeSampleFunc&& consumeSampleFunc)
            {
                sampler::FrameLevelSampleCombiner::FrameInfo frameInfo{};
                bool success = m_combiner.GetCombinedSamples(frameEndTime, frameInfo);
                if (!success)
                {
                    return false;
                }

                if (frameInfo.numSamplesInFrame)
                {
                    success = EvaluateToGpuValues(
                        *m_pMetricsEvaluator,
                        frameInfo.pCombinedCounterData,
                        frameInfo.combinedCounterDataSize,
                        frameInfo.combinedCounterDataRangeIndex,
                        m_pMetricEvalRequests->size(),
                        m_pMetricEvalRequests->data(),
                        m_perFrameMetricValuesScratchBuffer.data());
                    if (!success)
                    {
                        return false;
                    }

                    success = consumeSampleFunc(m_perFrameMetricValuesScratchBuffer);
                    if (!success)
                    {
                        return false;
                    }
                }
                
                return true;
            }

            // "TConsumeSampleFunc" is expected to be in the form of bool(const sampler::SampleTimestamp&, const std::vector<double>& metricValues)
            template <typename TConsumeSampleFunc>
            bool GetSampleValues(
                const uint8_t* pCounterDataImage,
                size_t counterDataImageSize,
                uint32_t rangeIndex,
                TConsumeSampleFunc&& consumeSampleFunc)
            {
                bool success = EvaluateToGpuValues(
                    *m_pMetricsEvaluator,
                    pCounterDataImage,
                    counterDataImageSize,
                    rangeIndex,
                    m_pMetricEvalRequests->size(),
                    m_pMetricEvalRequests->data(),
                    m_perSampleMetricValueScratchBuffer.data());
                if (!success)
                {
                    return false;
                }

                sampler::SampleTimestamp timestamp{};
                success = sampler::CounterDataGetSampleTime(pCounterDataImage, rangeIndex, timestamp);
                if (!success)
                {
                    return false;
                }

                success = consumeSampleFunc(timestamp, m_perSampleMetricValueScratchBuffer);
                if (!success)
                {
                    return false;
                }
                return true;
            }
        };
    private:
        std::string m_chipName;
        std::vector<HudConfiguration> m_configurations;
        RingBuffer<double> m_timestampBuffer;

        MetricsEvaluator m_metricsEvaluator;
        std::vector<NVPW_MetricEvalRequest> m_metricEvalRequests;
        CounterConfiguration m_counterConfiguration;
        SampleProcessor m_sampleProcessor;
        uint64_t m_firstSampleTime;
        RingBuffer<uint64_t> m_pendingFrames;
        size_t m_pendingFramesReadIndex;
        bool m_isInitialized;

    public:
        HudDataModel()
            : m_firstSampleTime()
            , m_pendingFramesReadIndex()
            , m_isInitialized()
        {
        }

        bool Load(const HudPreset& preset)
        {
            if (IsInitialized())
            {
                NV_PERF_LOG_ERR(20, "Already initialized\n");
                return false;
            }
            if (!preset.IsValid())
            {
                NV_PERF_LOG_ERR(20, "Not a valid preset to load\n");
                return false;
            }

            if (m_chipName.empty())
            {
                m_chipName = preset.chipName;
            } 
            else
            {
                if (m_chipName != preset.chipName)
                {
                    NV_PERF_LOG_ERR(20, "All presets need the same chip name (%s != %s)\n", preset.chipName.c_str(), m_chipName.c_str());
                    return false;
                }
            }

            ScopedRymlErrorHandler customRymlErrorHandler;

            ryml::Tree tree = ryml::parse_in_arena(ryml::csubstr(preset.fileName.c_str(), preset.fileName.size()), ryml::csubstr(preset.pYaml, strlen(preset.pYaml)));
            ryml::NodeRef root = tree.rootref();

            ryml::NodeRef panelsNode;
            ryml::NodeRef configurationsNode;

            for (const ryml::NodeRef& childNode : root.children())
            {
                if (childNode.key() == "panels")
                {
                    if (panelsNode.valid())
                    {
                        NV_PERF_LOG_ERR(20, "YAML file can only have one panels section\n");
                        return false;
                    }
                    panelsNode = childNode;
                }
                else if (childNode.key() == "configurations")
                {
                    if (configurationsNode.valid())
                    {
                        NV_PERF_LOG_ERR(20, "YAML file can only have one configurations section\n");
                        return false;
                    }
                    configurationsNode = childNode;
                }
                else
                {
                    NV_PERF_LOG_ERR(20, "Unknown section \"%s\"\n", childNode.key().data());
                    return false;
                }
            }

            // TODO not necessary when we decouple panels from configs
            if (panelsNode.valid() == false)
            {
                NV_PERF_LOG_ERR(20, "Cannot find panels section in YAML file\n");
                return false;
            }
            if (configurationsNode.valid() == false)
            {
                NV_PERF_LOG_ERR(20, "Cannot find configurations section in YAML file\n");
                return false;
            }

            // TODO decouple this so that panels from other files can be used as well
            std::vector<Panel> panels;
            for (const ryml::NodeRef& panelNode : panelsNode.children())
            {
                bool valid;
                Panel panel = Panel::FromYaml(panelNode, &valid, m_chipName);
                if (!valid)
                {
                    NV_PERF_LOG_ERR(20, "Missing or invalid panel\n");
                    return false;
                }

                panels.emplace_back(panel);
            }

            bool found = false;
            for (const ryml::NodeRef& configurationNode : configurationsNode.children())
            {
                bool valid;
                std::string configurationName = HudConfiguration::NameFromYaml(configurationNode, &valid);
                if (!valid)
                {
                    NV_PERF_LOG_ERR(20, "Missing or invalid configuration\n");
                    return false;
                }

                if (preset.name == configurationName)
                {
                    HudConfiguration configuration = HudConfiguration::FromYaml(configurationNode, panels, &valid);
                    if (!valid)
                    {
                        NV_PERF_LOG_ERR(20, "Failed loading configuration \"%s\"\n", configurationName.c_str());
                        return false;
                    }
                    found = true;
                    m_configurations.emplace_back(configuration);
                    break;
                }
            }

            if (found == false)
            {
                NV_PERF_LOG_ERR(20, "Cannot find configuration \"%s\"\n", preset.name.c_str());
                return false;
            }

            return true;
        }

        bool Load(const std::vector<HudPreset>& presets)
        {
            if (IsInitialized())
            {
                NV_PERF_LOG_ERR(20, "Already initialized\n");
                return false;
            }

            for (const auto& preset : presets)
            {
                bool success = Load(preset);
                if (!success)
                {
                    NV_PERF_LOG_ERR(20, "Failed loading preset \"%s\"\n", preset.name.c_str());
                    return false;
                }
            }

            return true;
        }

        bool Initialize(double samplingIntervalInSeconds, double plotTimeWidthInSeconds, const MetricConfigObject& metricConfigObject = MetricConfigObject())
        {
            if (IsInitialized())
            {
                NV_PERF_LOG_ERR(20, "Already initialized\n");
                return false;
            }

            if (m_configurations.empty())
            {
                NV_PERF_LOG_ERR(20, "No valid presets have been loaded\n");
                return false;
            }

            size_t maxNumSamples = (size_t)std::ceil(plotTimeWidthInSeconds / samplingIntervalInSeconds);
            m_timestampBuffer = RingBuffer<double>(maxNumSamples);

            m_pendingFrames = RingBuffer<uint64_t>(64); // cache up to 64 frames' timestamps

            {
                std::vector<uint8_t> metricsEvaluatorScratchBuffer;
                NVPW_MetricsEvaluator* pMetricsEvaluator = sampler::DeviceCreateMetricsEvaluator(metricsEvaluatorScratchBuffer, m_chipName.c_str());
                if (!pMetricsEvaluator)
                {
                    return false;
                }
                m_metricsEvaluator = MetricsEvaluator(pMetricsEvaluator, std::move(metricsEvaluatorScratchBuffer)); // transfer ownership to metricsEvaluator
            }

            RawCounterSchedulingHints schedulingHints = {};
            if (!metricConfigObject.IsLoaded())
            {
                const std::string allUserMetricsScript = metricConfigObject.GenerateScriptForAllNamespacedUserMetrics();
                if (!allUserMetricsScript.empty())
                {
                    if (!m_metricsEvaluator.UserDefinedMetrics_Initialize())
                    {
                        return false;
                    }
                    if (!m_metricsEvaluator.UserDefinedMetrics_Execute(allUserMetricsScript))
                    {
                        return false;
                    }
                    if (!m_metricsEvaluator.UserDefinedMetrics_Commit())
                    {
                        return false;
                    }
                }
                nv::perf::MetricsAndSchedulingHints metricsAndSchedulingHints = metricConfigObject.GetMetricsAndScheduleHints("");
                for (auto& schedulingHint : metricsAndSchedulingHints.schedulingHints)
                {
                    for (auto& hint : schedulingHint.hints)
                    {
                        schedulingHints[hint.rawCounterName] = RawCounterConfigBuilder::ToRawCounterDomain(schedulingHint.domain.c_str());
                    }
                }
            }

            CounterConfigBuilder counterConfigBuilder;
            if (!counterConfigBuilder.Initialize(m_chipName.c_str(), m_metricsEvaluator, m_metricEvalRequests, schedulingHints))
            {
                return false;
            }

            auto setSignalDescription = [&](MetricSignal& signal)
            {
                NVPW_MetricType metricType;
                size_t nativeMetricIndex;
                bool success = GetMetricTypeAndIndex(m_metricsEvaluator, signal.metric.c_str(), metricType, nativeMetricIndex);
                if (success)
                {
                    const char* pDescription = GetMetricDescription(m_metricsEvaluator, metricType, nativeMetricIndex);
                    if (pDescription)
                    {
                        signal.description = pDescription;
                    }
                    else if (signal.description.empty())
                    {
                        NV_PERF_LOG_WRN(50, "Could not get description for %s\n", signal.metric.c_str());
                    }
                }
                else
                {
                    NV_PERF_LOG_WRN(50, "Could not get metric type and index for %s\n", signal.metric.c_str());
                }
            };

            auto setSignalUnit = [&](MetricSignal& signal)
            {
                if (!signal.unit.empty() || signal.unit == MetricSignal::HideUnit())
                {
                    return;
                }

                NVPW_MetricEvalRequest request;
                bool success = ToMetricEvalRequest(m_metricsEvaluator, signal.metric.c_str(), request);
                if (!success)
                {
                    NV_PERF_LOG_WRN(50, "Could not create MetricEvalRequest for %s\n", signal.metric.c_str());
                    return;
                }

                std::vector<NVPW_DimUnitFactor> dimUnitFactors;
                success = GetMetricDimUnits(m_metricsEvaluator, request, dimUnitFactors);
                if (!success)
                {
                    NV_PERF_LOG_WRN(50, "Could not get DimUnits for %s\n", signal.metric.c_str());
                    return;
                }

                if (dimUnitFactors.empty())
                {
                    NV_PERF_LOG_INF(50, "%s is unit less. As a result, its unit will be displayed as empty. If this is not desired, please set the unit explicitly in the configuration file.\n", signal.metric.c_str());
                    return;
                }

                std::string dimUnits = nv::perf::ToString(dimUnitFactors, [&](NVPW_DimUnitName dimUnit, bool plural) {
                    return ToCString(m_metricsEvaluator, dimUnit, plural);
                });

                std::map<std::string, std::string> renameUnit
                {
                    {"percent", "%"}
                };
                if (renameUnit.find(dimUnits) != renameUnit.end())
                {
                    dimUnits = renameUnit[dimUnits];
                }

                signal.unit = dimUnits;
            };

            // sets maxValue statically if possible, otherwise registers metricIndexMaxValue to query the value per sample/frame
            auto setSignalMaxValue = [&](MetricSignal& signal) -> bool {
                if (!std::isnan(signal.maxValue))
                {
                    return true;
                }

                const std::string& metric = signal.metric;

                NVPW_MetricEvalRequest request;
                if (!ToMetricEvalRequest(m_metricsEvaluator, metric.c_str(), request))
                {
                    NV_PERF_LOG_ERR(20, "Unknown metric: %s\n", metric.c_str());
                    return false;
                }

                NVPW_MetricEvalRequest maxRequest{ (size_t)~0 };
                if (NVPW_MetricType(request.metricType) == NVPW_METRIC_TYPE_THROUGHPUT)
                {
                    signal.maxValue = 100;
                }
                else if (NVPW_MetricType(request.metricType) == NVPW_METRIC_TYPE_RATIO)
                {
                    if (NVPW_Submetric(request.submetric) == NVPW_SUBMETRIC_PCT)
                    {
                        signal.maxValue = 100;
                    }
                    else // ratio and max_rate
                    {
                        maxRequest = request;
                        maxRequest.submetric = NVPW_SUBMETRIC_MAX_RATE;
                    }
                }
                else if (NVPW_MetricType(request.metricType) == NVPW_METRIC_TYPE_COUNTER)
                {
                    switch (NVPW_Submetric(request.submetric))
                    {
                    case NVPW_SUBMETRIC_PER_CYCLE_ACTIVE:
                    case NVPW_SUBMETRIC_PER_CYCLE_ELAPSED:
                    case NVPW_SUBMETRIC_PER_CYCLE_IN_FRAME:
                    case NVPW_SUBMETRIC_PER_CYCLE_IN_REGION:
                        maxRequest = request;
                        maxRequest.submetric = NVPW_SUBMETRIC_PEAK_SUSTAINED;
                        break;
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_ACTIVE:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_ACTIVE_PER_SECOND:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED_PER_SECOND:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_FRAME:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_FRAME_PER_SECOND:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_REGION:
                    case NVPW_SUBMETRIC_PEAK_SUSTAINED_REGION_PER_SECOND:
                        maxRequest = request;
                        break;
                    case NVPW_SUBMETRIC_NONE:
                        maxRequest = request;
                        maxRequest.submetric = NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED;
                        break;
                    case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ACTIVE:
                    case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ELAPSED:
                    case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_FRAME:
                    case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_REGION:
                        signal.maxValue = 100;
                        break;
                    case NVPW_SUBMETRIC_PER_SECOND:
                        maxRequest = request;
                        maxRequest.submetric = NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED_PER_SECOND;
                        break;
                    default:
                        NV_PERF_LOG_WRN(50, "Cannot determine maxValue for metric %s, sub metric: %d\n", metric.c_str(), request.submetric);
                    }
                }

                if (maxRequest.metricIndex != (size_t)~0)
                {
                    std::string maxMetric = ToString(m_metricsEvaluator, maxRequest);
                    const size_t metricIndexMaxValue = counterConfigBuilder.AddMetric(maxMetric, &maxRequest);
                    if (metricIndexMaxValue == (size_t)~0)
                    {
                        NV_PERF_LOG_WRN(50, "Failed configuring max metric %s for %s\n", maxMetric.c_str(), metric.c_str());
                    }
                    signal.SetMetricIndexMaxValue(metricIndexMaxValue);
                }

                return true;
            };

            for (auto& configuration : m_configurations)
            {
                for (auto& panel : configuration.panels)
                {
                    for (std::unique_ptr<Widget>& pWidget : panel.widgets)
                    {
                        if (pWidget->type == Widget::Type::ScalarText)
                        {
                            ScalarText& scalarText = *static_cast<ScalarText*>(pWidget.get());
                            MetricSignal& signal = scalarText.signal;
                            signal.SetMaxNumSamples(1);
                            setSignalDescription(signal);
                            setSignalUnit(signal);
                            const size_t metricIndex = counterConfigBuilder.AddMetric(signal.metric);
                            if (metricIndex == (size_t)~0)
                            {
                                NV_PERF_LOG_ERR(20, "Unknown metric: %s\n", signal.metric.c_str());
                                return false;
                            }
                            signal.SetMetricIndex(metricIndex);

                            if (scalarText.showValue == ScalarText::ShowValue::ValueWithMax)
                            {
                                bool success = setSignalMaxValue(signal);
                                if (!success)
                                {
                                    return false;
                                }
                            }
                        }
                        else if (pWidget->type == Widget::Type::TimePlot)
                        {
                            TimePlot& timePlot = *static_cast<TimePlot*>(pWidget.get());
                    
                            timePlot.SetTimestampBuffer(&m_timestampBuffer);
                            timePlot.SetTimeWidth(plotTimeWidthInSeconds);

                            for (size_t index = 0; index < timePlot.signals.size(); ++index)
                            {
                                MetricSignal& signal = timePlot.signals[index];
                                signal.SetMaxNumSamples(maxNumSamples);
                                setSignalDescription(signal);
                                setSignalUnit(signal);
                                const size_t metricIndex = counterConfigBuilder.AddMetric(signal.metric);
                                if (metricIndex == (size_t)~0)
                                {
                                    NV_PERF_LOG_ERR(20, "Unknown metric: %s\n", signal.metric.c_str());
                                    return false;
                                }
                                signal.SetMetricIndex(metricIndex);

                                if (timePlot.chartType == TimePlot::ChartType::Stacked)
                                {
                                    MetricSignal& stackedSignal = timePlot.stackedSignals[index];
                                    stackedSignal.SetMaxNumSamples(maxNumSamples);
                                    setSignalDescription(stackedSignal);
                                    setSignalUnit(signal);
                                    const size_t metricIndex = counterConfigBuilder.AddMetric(stackedSignal.metric);
                                    if (metricIndex == (size_t)~0)
                                    {
                                        NV_PERF_LOG_ERR(20, "Unknown metric: %s\n", stackedSignal.metric.c_str());
                                        return false;
                                    }
                                    stackedSignal.SetMetricIndex(metricIndex);
                                }
                            }
                        }
                    }
                }
            }

            if (!counterConfigBuilder.GenerateCounterConfiguration(m_counterConfiguration))
            {
                return false;
            }

            m_isInitialized = true;

            return true;
        }

        bool Initialize(const MetricConfigObject& metricConfigObject)
        {
            return Initialize(4, 1 / 60.0, metricConfigObject);
        }

        bool IsInitialized() const
        {
            return m_isInitialized;
        }

        const std::vector<HudConfiguration>& GetConfigurations() const
        {
            return m_configurations;
        }

        const CounterConfiguration& GetCounterConfiguration() const
        {
            return m_counterConfiguration;
        }

        bool PrepareSampleProcessing(const std::vector<uint8_t>& counterData)
        {
            if (!IsInitialized())
            {
                NV_PERF_LOG_ERR(20, "Not initialized\n");
                return false;
            }

            if (!m_sampleProcessor.Initialize(m_metricsEvaluator, m_metricEvalRequests, counterData, m_counterConfiguration.counterDataPrefix))
            {
                return false;
            }
            return true;
        }

        bool AddFrameDelimiter(uint64_t frameEndTime)
        {
            if (!IsInitialized())
            {
                NV_PERF_LOG_ERR(20, "Not initialized\n");
                return false;
            }
            m_pendingFrames.Push(frameEndTime);
            while (m_pendingFramesReadIndex != m_pendingFrames.WriteIndex())
            {
                const uint64_t endTime = m_pendingFrames.Data()[m_pendingFramesReadIndex]; // note Get() is relative to the writeIndex
                if (!m_sampleProcessor.IsFrameDataComplete(endTime))
                {
                    break;
                }

                ++m_pendingFramesReadIndex;
                if (m_pendingFramesReadIndex == m_pendingFrames.MaxSize())
                {
                    m_pendingFramesReadIndex = 0;
                }

                bool success = m_sampleProcessor.GetFrameLevelCombinedSampleValues(endTime, [&](const std::vector<double>& metricValues) {
                    AddFrameLevelValues(endTime, metricValues);
                    return true;
                });
                if (!success)
                {
                    return false;
                }
            }
            return true;
        }

        void AddFrameLevelValues(uint64_t frameEndTime, const std::vector<double>& metricValues)
        {
            for (auto& configuration : m_configurations)
            {
                for (auto& panel : configuration.panels)
                {
                    for (std::unique_ptr<Widget>& pWidget : panel.widgets)
                    {
                        if (pWidget->type == Widget::Type::ScalarText)
                        {
                            ScalarText& scalarText = *static_cast<ScalarText*>(pWidget.get());

                            double value = 0.0;
                            if (scalarText.signal.metricIndex < metricValues.size())
                            {
                                value = (std::max)(0.0, metricValues[scalarText.signal.metricIndex]);
                            }
                            else
                            {
                                NV_PERF_LOG_WRN(50, "metricValues has too few entries. metricIndex (%zu) >= metricValues.size (%zu)\n", scalarText.signal.metricIndex, metricValues.size());
                            }
                            scalarText.signal.AddSample(value);

                            if (scalarText.signal.metricIndexMaxValue != (size_t)~0)
                            {
                                double maxValue = 0.0;
                                if (scalarText.signal.metricIndexMaxValue < metricValues.size())
                                {
                                    maxValue = (std::max)(0.0, metricValues[scalarText.signal.metricIndexMaxValue]);
                                }
                                else
                                {
                                    NV_PERF_LOG_WRN(50, "metricValues has too few entries. metricIndexMaxValue (%zu) >= metricValues.size (%zu)\n", scalarText.signal.metricIndexMaxValue, metricValues.size());
                                }
                                scalarText.signal.SetMaxValue(maxValue);
                            }
                        }
                    }
                }
            }
        }

        bool AddSample(const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex)
        {
            if (!IsInitialized())
            {
                NV_PERF_LOG_ERR(20, "Not initialized\n");
                return false;
            }
            if (!m_sampleProcessor.AddSample(pCounterDataImage, counterDataImageSize, rangeIndex))
            {
                return false;
            }

            bool success = m_sampleProcessor.GetSampleValues(pCounterDataImage, counterDataImageSize, rangeIndex, [&](const sampler::SampleTimestamp& sampleTime, const std::vector<double>& metricValues) {
                const uint64_t endTimestamp = sampleTime.end;
                if (!m_firstSampleTime)
                {
                    m_firstSampleTime = endTimestamp;
                }
                const double timestamp = (endTimestamp - m_firstSampleTime) / double(1000000000);
                AddSample(timestamp, metricValues);
                return true;
            });
            if (!success)
            {
                return false;
            }
            return true;
        }

        // Note: Incoming values are clamped to a minimum of 0
        void AddSample(double timestamp, const std::vector<double>& metricValues)
        {
            m_timestampBuffer.Push(timestamp);

            for (auto& configuration : m_configurations)
            {
                for (auto& panel : configuration.panels)
                {
                    for (std::unique_ptr<Widget>& pWidget : panel.widgets)
                    {
                        if (pWidget->type == Widget::Type::TimePlot)
                        {
                            TimePlot& timePlot = *static_cast<TimePlot*>(pWidget.get());
                    
                            double accumulatedValue = 0;
                            for (int index = int(timePlot.signals.size())-1; index >= 0; index -= 1)
                            {
                                double value = 0.0;
                                if (timePlot.signals[index].metricIndex < metricValues.size())
                                {
                                    value = (std::max)(0.0, metricValues[timePlot.signals[index].metricIndex]);
                                }
                                else
                                {
                                    NV_PERF_LOG_WRN(50, "metricValues has too few entries. metricIndex (%zu) >= metricValues.size (%zu)\n", timePlot.signals[index].metricIndex, metricValues.size());
                                }
                                timePlot.signals[index].AddSample(value);
                    
                                if (timePlot.chartType == TimePlot::ChartType::Stacked)
                                {
                                    accumulatedValue += value;
                                    timePlot.stackedSignals[index].AddSample(accumulatedValue);
                                }
                            }
                        }
                    }
                }
            }
        }

        void Print(std::ostream& os, const std::string indent = std::string()) const
        {
            os << indent << "HudDataModel(" << std::endl;
            for (const auto& configuration : m_configurations)
            {
                configuration.Print(os, indent + "  ");
            }
            m_timestampBuffer.Print(os, false, indent + "  ");
            os << indent << ")" << std::endl;
        }
    };

} } } // nv::perf::hud
