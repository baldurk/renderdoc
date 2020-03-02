<!--
- Copyright (c) 2019, Arm Limited and Contributors
-
- SPDX-License-Identifier: MIT
-
- Permission is hereby granted, free of charge,
- to any person obtaining a copy of this software and associated documentation files (the "Software"),
- to deal in the Software without restriction, including without limitation the rights to
- use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
- and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
-
- The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
-
- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
- INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
- IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
- WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-
-->

# HWCPipe

## Introduction

HWCPipe is a simple and extensible interface for reading CPU and GPU hardware counters.

## License

The software is provided under an MIT license.

This project has a third-party dependency, which may have independent licensing:

- [nlohmann/json](https://github.com/nlohmann/json): A JSON library for modern C++

## Contributions

All contributions are accepted under the same [LICENSE](LICENSE).

## Building

To use HWCPipe, build it as a shared library in your project.

If your project uses CMake, you can add the following to your `CMakeLists.txt`:

```
add_subdirectory(hwcpipe)
```

## Usage

### Using HWCPipe

Basic usage for HWCPipe is simple:

```
// HWCPipe performs automated platform detection for CPU/GPU counters
hwcpipe::HWCPipe h;

// Start HWCPipe once at the beginning of the profiling session
h.run();

while (main_loop) {
    // Call sample() to sample counters with the frequency you need
    auto measurements = h.sample();

    [...]
}

// At the end of the profiling session, stop HWCPipe
h.stop();
```

The `sample` function returns a `Measurements` struct, which can be accessed like this:

```
// Check if CPU measurements are available
if (measurements.cpu)
{
    // Look for a counter in the map
    const auto &counter = measurements.cpu->find(CpuCounter::Cycles);
    if (counter != measurements.cpu->end())
    {
        // Get the data stored in the counter, casted to the type you need
        auto value = counter->second.get<float>();
    }
}
```

### Enabling counters

The available counters are specified in the `CpuCounter` and `GpuCounter` enums (`cpu_profiler.h` and `gpu_profiler.h` respectively).

Platforms will support a subset of these counters, which can be queried via:

```
auto cpu_counters = h.cpu_profiler()->supported_counters();
auto gpu_counters = h.gpu_profiler()->supported_counters()
```

You can specify the counters to be enabled in the following ways:

```
// Enable a default set of counters
auto h = hwcpipe::HWCPipe();

// Pass sets of CPU and GPU counters to be enabled
auto h = hwcpipe::HWCPipe({CpuCounter::Cycles, CpuCounter::Instructions}, {GpuCounter::GpuCycles});

// Pass a JSON string
auto h = hwcpipe::HWCPipe(json);
```

The JSON string should be formatted like this:

```
{
    "cpu": ["Cycles", "Instructions"],
    "gpu": ["GpuCycles"]
}
```

Available counter names can be found in `cpu_counter_names` (`cpu_profiler.h`) and `gpu_counter_names` (`gpu_profiler.h`).

For more information regarding Mali counters, see [Mali Performance Counters](https://community.arm.com/graphics/b/blog/posts/mali-bifrost-family-performance-counters).

### Enabling profiling on Android

In order for performance data to be displayed, profiling needs to be enabled on the device.
Some devices may disable it by default.

Profiling can be enabled via `adb`:

```
adb shell setprop security.perf_harden 0
```

## Adding support for a new platform

If the counters provided in `CpuCounter` and `GpuCounter` are enough for the new platform,
the process is simple:

* Add an implementation of either `CpuProfiler` of `GpuProfiler` (you can use `PmuProfiler` and `MaliProfiler` as references).
* Add your platform to the automated platform detection in `hwcpipe.cpp`. For consistency in platform detection, the constructor for your platform should throw if the platform is not available.
* Add your platform to the build system.

### Adding new counters

If you need to add new counters to the existing ones, you should update the following variables:

* Add the counter to the `CpuCounter`/`GpuCounter` enum.
* Add the counter name to the `cpu_counter_names`/`gpu_counter_names` map (necessary for JSON initialization).
* Add a description and the unit for your counter to the `cpu_counter_info`/`gpu_counter_info` map.

The `CpuCounter` and `GpuCounter` enums are meant to be expanded. Platforms must not break if new counters are added.
