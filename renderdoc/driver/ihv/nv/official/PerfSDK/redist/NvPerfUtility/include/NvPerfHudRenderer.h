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

#include "NvPerfHudDataModel.h" // include before NvPerfInit.h (and thus Windows.h) to avoid std::min/max errors in rapidyaml

#include "NvPerfInit.h"

namespace nv { namespace perf { namespace hud {
	class HudRenderer {
    protected:
        // One of these three is set in Initialize()
        const Panel* m_pPanel = nullptr;
        const HudConfiguration* m_pConfiguration = nullptr;
        const HudDataModel* m_pModel = nullptr;
        std::map<const ScalarText*, std::vector<const ScalarText*>> m_scalarTextBlocks; // blocks of ScalarTexts for column alignment

    protected:
        virtual bool InitializeScalarTextBlocks()
        {
            std::vector<const Panel*> panels;
            std::vector<const HudConfiguration*> configurations;

            if (m_pModel)
            {
                for (const HudConfiguration& configuration : m_pModel->GetConfigurations())
                {
                    configurations.emplace_back(&configuration);
                }
            }
            else if (m_pConfiguration)
            {
                configurations.emplace_back(m_pConfiguration);
            }

            if (configurations.size() != 0)
            {
                if (m_pPanel)
                {
                    NV_PERF_LOG_ERR(20, "Only one of m_panel, m_configuration, m_model can be set\n");
                    return false;
                }
                for (const HudConfiguration* pConfiguration : configurations)
                {
                    for (const Panel& panel : pConfiguration->panels)
                    {
                        panels.emplace_back(&panel);
                    }
                }
            }
            else
            {
                if (!m_pPanel)
                {
                    NV_PERF_LOG_ERR(20, "Could not find any panels\n");
                    return false;
                }

                panels.push_back(m_pPanel);
            }

            for (const Panel* pPanel : panels)
            {
                for (size_t index = 0; index < pPanel->widgets.size(); ++index)
                {
                    if (pPanel->widgets[index]->type == Widget::Type::ScalarText)
                    {
                        std::vector<const ScalarText*> block;

                        size_t blockSize = 0;
                        while (index + blockSize < pPanel->widgets.size() && pPanel->widgets[index + blockSize]->type == Widget::Type::ScalarText)
                        {
                            block.emplace_back(static_cast<const ScalarText*>(pPanel->widgets[index + blockSize].get()));
                            ++blockSize;
                        }

                        m_scalarTextBlocks[static_cast<const ScalarText*>(pPanel->widgets[index].get())] = block;

                        index += blockSize - 1;
                    }
                }
            }

            return true;
        }

        // default empty
        virtual bool RenderFrameSeparator() { return true; }
        virtual bool RenderPanelBegin(const Panel&, bool*)  const { return true; }
        virtual bool RenderPanelEnd(const Panel&) const { return true; }
        virtual bool RenderSeparator(const Separator&) const { return true; }
        virtual bool RenderScalarTextBlock(const std::vector<const ScalarText*>&) const { return true; }
        virtual bool RenderTimePlot(const TimePlot&) const { return true; }

        virtual bool RenderPanel(const Panel& panel) const
        {
            bool success;

            bool showPanelContents = true;
            success = RenderPanelBegin(panel, &showPanelContents);
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "Error in RenderPanelBegin()\n");
                return false;
            }

            if (!showPanelContents)
            {
                return true;
            }

            for (size_t index = 0; index < panel.widgets.size(); ++index)
            {
                const Widget* pWidget = panel.widgets[index].get();

                if (pWidget->type == Widget::Type::Panel)
                {
                    NV_PERF_LOG_WRN(50, "Panels cannot be nested\n");
                }
                else if (pWidget->type == Widget::Type::Separator)
                {
                    success = RenderSeparator(*static_cast<const Separator*>(pWidget));
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "Error in RenderSeparator()\n");
                        return false;
                    }
                }
                else if (pWidget->type == Widget::Type::ScalarText)
                {
                    auto it = m_scalarTextBlocks.find(static_cast<const ScalarText*>(pWidget));
                    if (it == m_scalarTextBlocks.end())
                    {
                        NV_PERF_LOG_ERR(20, "Missing ScalarTextBlock\n");
                        return false;
                    }
                    const auto& block = it->second;
                    success = RenderScalarTextBlock(block);
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "Error in RenderScalarTextBlock()\n");
                        return false;
                    }
                    index += block.size() - 1;
                }
                else if (pWidget->type == Widget::Type::TimePlot)
                {
                    success = RenderTimePlot(*static_cast<const TimePlot*>(pWidget));
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "Error in RenderTimePlot()\n");
                        return false;
                    }
                }
                else
                {
                    NV_PERF_LOG_WRN(50, "Cannot render unknown Widget\n");
                }
            }

            success = RenderPanelEnd(panel);
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "Error in RenderPanelEnd()\n");
                return false;
            }

            return true;
        }

        virtual bool RenderConfiguration(const HudConfiguration& configuration) const
        {
            for (const Panel& panel : configuration.panels)
            {
                bool success = RenderPanel(panel);
                if (!success)
                {
                    return false;
                }
            }
            return true;
        }

        virtual bool RenderDataModel(const HudDataModel& model) const
        {
            for (const HudConfiguration& configuration : model.GetConfigurations())
            {
                bool success = RenderConfiguration(configuration);
                if (!success)
                {
                    return false;
                }
            }
            return true;
        }

    public:
        HudRenderer()
        {
        }

        virtual ~HudRenderer()
        {
        }

        virtual bool Initialize(const Panel& panel)
        {
            if (m_pPanel || m_pConfiguration || m_pModel)
            {
                NV_PERF_LOG_ERR(20, "Already initialized\n");
                return false;
            }
            m_pPanel = &panel;
            bool success = InitializeScalarTextBlocks();
            return success;
        }

        virtual bool Initialize(const HudConfiguration& configuration)
        {
            if (m_pPanel || m_pConfiguration || m_pModel)
            {
                NV_PERF_LOG_ERR(20, "Already initialized\n");
                return false;
            }
            m_pConfiguration = &configuration;
            bool success = InitializeScalarTextBlocks();
            return success;
        }

        virtual bool Initialize(const HudDataModel& model)
        {
            if (m_pPanel || m_pConfiguration || m_pModel)
            {
                NV_PERF_LOG_ERR(20, "Already initialized\n");
                return false;
            }
            m_pModel = &model;
            bool success = InitializeScalarTextBlocks();
            return success;
        }

        virtual bool Render()
        {
            bool success = false;
            success = RenderFrameSeparator();
            if (m_pPanel)
            {
                success = RenderPanel(*m_pPanel);
            }
            else if (m_pConfiguration)
            {
                success = RenderConfiguration(*m_pConfiguration);
            }
            else if (m_pModel)
            {
                success = RenderDataModel(*m_pModel);
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Renderer has not been initialized\n");
                return false;
            }
            return success;
        }
	};
}}}