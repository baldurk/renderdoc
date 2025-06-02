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

#include <string>

namespace nv { namespace perf {

    inline std::string GetReadMeHtml()
    {
        return R"(
<html>

  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>

  <head>
    <title>NvPerfSDK</title>
    <style id="ReportStyle">
      .titlearea {
        display: flex;
        align-items: center;
        color: white;
        font-family: verdana;
      }

      .titlebar {
        margin-left: 0;
        margin-right: auto;
      }

      .title {
        font-size: 28px;
        margin-left: 10px;
      }

      .section {
        border-radius: 15px;
        padding: 10px;
        background: #FFFFFF;
        margin: 10px;
      }

      .section_title {
        font-family: verdana;
        font-weight: bold;
        color: black;
      }

      li {
        white-space: normal;;
      }
    </style>
  </head>

  <body style="background-color:#202020;">
    <div>
      <div class="titlearea">
        <div class="titlebar">
          <img src="https://developer.nvidia.com/sites/all/themes/devzone_new/nvidia_logo.png"/>
          <span class="title" id="titlebar_text">Nsight Perf SDK</span>
        </div>
      </div>
    </div>

    <div class="section" id="intro">
      <h2>Nsight Perf SDK HTML Report</h2>
      <p>Navigate to the <a href="summary.html">summary.html</a> to begin.
    </div>

    <div class="section" id="unintended_use">
      <h2>Unintended Use of Product</h2>
      <p>The Nsight Perf SDK should not be used for benchmarking absolute performance, nor for comparing results between GPUs, due to the following factors:</p>
      <ul>
        <li>To ensure stable measurements, the Perf SDK encourages users to lock the GPU to its <a href=https://en.wikipedia.org/wiki/Thermal_design_power target="_blank">rated TDP (thermal design power)</a>. This forces thermally stable clock rates and disables boost clocks, ensuring consistent performance, but preventing the GPU from reaching its absolute peak performance.</li>
        <li>The Perf SDK disables certain GPU power management settings during profiling, to meet hardware requirements.</li>
        <li>Not all metrics are comparable between GPUs or architectures. For example, a more powerful GPU may complete a workload in less time while showing lower %%-of-peak throughput values, when compared against a less powerful GPU.</li>
      </ul>
      <p>The Nsight Perf SDK is intended to be used for performance profiling, to assist developers and artists in improving the performance of their code, art assets, and GPU shaders.</p>
      <p>See the bundled NVIDIA Developer Tools License document for additional details.</p>
    </div>


  </body>

</html>
)";
    }

    inline std::string GetSchedulingInfoHtml()
    {
        return R"ImADelimiter(
<html>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>

  <head>
    <title>Metrics Scheduling Information</title>

    <style id="ReportStyle">

      table {
        font-size: 14px;
        margin: 2 auto;
        border-collapse: collapse;
        border: 1px solid ;
      }

      table th {
        margin: 0 auto;
        border-collapse: collapse;
        border: 1px solid ;
        background: #F8F8F8;
      }

      table td {
        margin: 0 auto;
        border-collapse: collapse;
        border: 1px solid ;
      }

      table tbody tr td {
        margin: 0 auto;
        border-collapse: collapse;
        border: 1px solid ;
      }

      .tablename {
        color: DarkGreen;
        border-color: Black;
        background: #F8F8F8;
        font-weight: bold;
      }

      .subhdr {
        background: #F8F8F8;
      }

      .ca {
        text-align: center;
      }

      .la {
        text-align: left;
      }

      .ra {
        text-align: right;
      }

      .ww {
        word-wrap: break-word;
      }

      .full_name {
        min-width: 150px;
        width: 33vw;
        max-width: calc(92vw - 920px);
      }

      .base {
        font-size: 8px;
        color: #606060;
        border-color: Black;
      }

      .comp {
        font-size: 8px;
        color: steelblue;
        border-color: Black;
      }

      .titlearea {
        display: flex;
        align-items: center;
        color: white;
        font-family: verdana;
      }

      .titlebar {
        margin-left: 0;
        margin-right: auto;
      }

      .global_settings {
        margin-left: auto;
        margin-right: 0;
      }

      .title {
        font-size: 28px;
        margin-left: 10px;
      }

      .section {
        border-radius: 15px;
        padding: 10px;
        background: #FFFFFF;
        margin: 10px;
        min-width: calc(100% - 40px);
        width: max-content;
      }

      .section_title {
        font-family: verdana;
        font-weight: bold;
        color: black;
      }

      .workflow {
        width: 960px;
        max-width: 90vw;
      }

    </style>


    <script type="text/JavaScript">

      function escapeHtml(input) {
        if (typeof input == 'string' || input instanceof String) {
          return input
            .replace(/&/g, "&amp;") // make it first
            .replace(/ /g, "&nbsp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&apos;");
        }
        return input;
      }

      function addCellAttr(trow, attributes, text, passThrough = false) {
        let td = document.createElement('td');
        for (const attr in attributes) {
          td.setAttribute(attr, attributes[attr]);
        }

        if (passThrough) {
          td.innerHTML = text;
        } else {
          td.innerHTML = escapeHtml(text);
        }
        trow.appendChild(td);
      }

      function addCellSimple(trow, classes, text, passThrough = false, tooltip = "") {
        addCellAttr(trow, { "class": classes, "title": tooltip }, text, passThrough);
      }

      function tbody_MetricsSchedulingInformation(tbody) {
        let perRawCounterPassIndex = {}; // rawCounter : passIndex
        for (const [passIndex, rawCounters] of Object.entries(g_rawCounterDistribution)) {
          for (const rawCounter of rawCounters) {
            perRawCounterPassIndex[rawCounter] = passIndex;
          }
        }

        let perMetricSchedulingData = {}; // metric : { passIndex : [Counters] }
        for (const metric in g_metricRawCounters) {
          const rawCounters = g_metricRawCounters[metric];
          perMetricSchedulingData[metric] = {};
          for (const rawCounter of rawCounters) {
            const passIndex = perRawCounterPassIndex[rawCounter];
            if (!perMetricSchedulingData[metric].hasOwnProperty(passIndex)) {
              perMetricSchedulingData[metric][passIndex] = [];
            }
            perMetricSchedulingData[metric][passIndex].push(rawCounter);
          }
        }

        // sort the dict
        let sortedMetrics = Object.keys(perMetricSchedulingData).sort();
        let sortedPerMetricSchedulingData = {};
        // for dict, since ES6, insertion order is preserved
        for (const metric of sortedMetrics) {
          sortedPerMetricSchedulingData[metric] = {};
          let sortedPassIndices = Object.keys(perMetricSchedulingData[metric]).sort((a, b) => parseInt(a) - parseInt(b));
          for (const passIndex of sortedPassIndices) {
            sortedPerMetricSchedulingData[metric][passIndex] = perMetricSchedulingData[metric][passIndex].sort();
          }
        }
        perMetricSchedulingData = sortedPerMetricSchedulingData;

        for (const [metric, schedulingData] of Object.entries(perMetricSchedulingData)) {
          const sortedPassIndices = Object.keys(schedulingData).sort((a, b) => parseInt(a) - parseInt(b));
          const rowspan = sortedPassIndices.length;

          for (let ii = 0; ii < rowspan; ii++) {
            const trow = document.createElement('tr');
            if (ii === 0) {
              addCellAttr(trow, { "class": "la", 'rowspan' : rowspan.toString() }, metric);
            }

            const passIndex = sortedPassIndices[ii];
            const passLink = `<a href="#Pass_${passIndex}">${passIndex}</a>`;
            addCellAttr(trow, { "class": "ca" }, passLink, true);
            addCellSimple(trow, "la", schedulingData[passIndex].join(', '));
            tbody.appendChild(trow);
          }
        }
      }

      function tbody_PassInformation(tbody) {
        const charLimitPerRow = 300;
        for (const [passIndex, counters] of Object.entries(g_rawCounterDistribution)) {
          let rows = [];
          let currentRow = [];
          let currentCharCount = 0;

          counters.sort();
          for (const counter of counters) {
            if (currentCharCount + counter.length > charLimitPerRow) {
              rows.push(currentRow);
              currentRow = [];
              currentCharCount = 0;
            }
            currentRow.push(counter);
            currentCharCount += counter.length;
          }

          if (currentRow.length > 0) {
            rows.push(currentRow);
          }

          for (let ii = 0; ii < rows.length; ii++) {
            const trow = document.createElement('tr');
            if (ii === 0) {
              trow.id = `Pass_${passIndex}`;
              addCellAttr(trow, { "class": "ca", 'rowspan': rows.length.toString()}, passIndex);
            }

            addCellSimple(trow, "la", rows[ii].join(', '));
            tbody.appendChild(trow);
          }
        }
      }

      function onBodyLoaded() {
        tbody_MetricsSchedulingInformation(document.getElementById('tbody_metrics_scheduling_information'));
        tbody_PassInformation(document.getElementById('tbody_pass_information'));
      }
    </script>

  </head>

  <body onload="onBodyLoaded()" style="background-color:#202020;">
    <noscript>
      <p>Enable javascript to see report contents</span>
    </noscript>

    <div>
    <div class="titlearea">
        <div class="titlebar">
          <img src="https://developer.nvidia.com/sites/all/themes/devzone_new/nvidia_logo.png"/>
          <span class="title" id="titlebar_text">Nsight Perf SDK Profiler Report</span>
        </div>
    </div>

    <div class="section">
        <table style="display: inline-block; border: 1px solid;">
            <thead>
                <tr>
                    <th colspan="3" class="tablename">Metrics Scheduling Information</th>
                </tr>
                <tr>
                    <th class="la">Metric</th>
                    <th class="la">Pass Index</th>
                    <th class="la">Raw Counters</th>
                </tr>
            </thead>
            <tbody id="tbody_metrics_scheduling_information">
            </tbody>
        </table>
    </div>

    <div class="section">
        <table style="display: inline-block; border: 1px solid;">
            <thead>
                <tr>
                    <th colspan="3" class="tablename">Pass Information</th>
                </tr>
                <tr>
                    <th class="la">Pass Index</th>
                    <th class="la">Raw Counters</th>
                </tr>
            </thead>
            <tbody id="tbody_pass_information">
            </tbody>
        </table>
    </div>

    </div>

    <script>
      g_json = {
        /***JSON_DATA_HERE***/
      };

      g_metricRawCounters = g_json.metricRawCounterDependencies || {};
      g_rawCounterDistribution = g_json.rawCounterDistribution || {}
    </script>

  </body>
</html>

)ImADelimiter";
    }
}}
