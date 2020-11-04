# AMD AGS SDK
![AMD AGS SDK](http://gpuopen-librariesandsdks.github.io/media/amd_logo_black.png)

The AMD GPU Services (AGS) library provides software developers with the ability to query AMD GPU software and hardware state information that is not normally available through standard operating systems or graphics APIs.  If you are serious about getting the most from your AMD GPU, then AGS can help you harness that power.

AGS includes support for querying graphics driver version info, GPU hardware info, performance metrics, shader extensions and additional extensions supported in the AMD drivers for DirectX 11 and DirectX 12.  AGS also provides Crossfire&trade; (AMD's multi-GPU rendering technology) support and Eyefinity (AMD's multi-display rendering technology) configuration info.

In addition to the library itself, the AGS SDK includes several samples to demonstrate use of the library.

<div>
  <a href="https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK/releases/latest/"><img src="http://gpuopen-librariesandsdks.github.io/media/latest-release-button.svg" alt="Latest release" title="Latest release"></a>
</div>

### What's new in AGS 6.0
Version 6.0 introduces several new shader intrinsics, namely a DX12 ray tracing hit token for RDNA2 hardware for ray tracing optimisation, ReadLaneAt and explicit float conversions.  There is also a change to the initialization API to make sure the AGS dll matches the header and calling code.

### What's new in AGS 5.4.2
Version 5.4.2 reinstates the sharedMemoryInBytes field which is required when calculating the memory available on APUs.

### What's new in AGS 5.4.1
Version 5.4.1 includes x86 libs and Visual Studio 2019 support.  There is also support for base vertex and base instance intrinsics as well as GetWaveSize intrinsics.  The samples have been ported from premake to cmake.

### What's new in AGS 5.4
Version 5.4 adds a better description of the GPU architecture for those wishing to fine tune their games for specific code paths.  In addition, there are now shader intrinsics for getting the draw index for execute indirect calls as well as support for atomic U64 ops.

Radeon 7 and RDNA GPU core and memory speeds are now returned.

### What's new in AGS 5.3
Version 5.3 adds DirectX 11 deferred context support for our MultiDrawIndirect and UAV overlap extensions, along with a helper function to let your app determine if the installed driver meets your game's minimum driver version requirements. If you're a Vulkan user, you can pair that with our machine readable AMD Vulkan versions database, to get more information about the Vulkan implementation in our client driver.

Lastly, there's a new FreeSync 2 gamma 2.2 mode. It uses a 10-bit (per RGB component, 2-bit alpha) swapchain, as opposed to the 16-bit (per RGB component, 16-bit alpha) swapchain needed for FreeSync 2 scRGB.

### What's new in AGS 5.2
Version 5.2 adds support for app registration in DirectX 12. App registration lets you give more information about your game or application to our driver, which can then use that (ideally unique) information to better support the game or app if we need to make driver-side changes to help things run as efficiently and correctly as possible.

We also changed how you get access to extensions under DX12, requiring you to create your GPU device using `agsDriverExtensionsDX12_CreateDevice()` , instead of the normal `D3D12CreateDevice()` call you’d make to D3D.

Lastly, we’ve also added support for breadcrumb markers in D3D11. Using the `agsDriverExtensionsDX11_WriteBreadcrumb()` API, you can put in place a strategy for debugging driver issues more easily. Sometimes your game or app can interact with the driver in a way that causes it to crash or TDR. The new API gives you the ability to leave markers around your D3D11 API calls, helping you narrow down exactly what interaction with the driver caused the problem.

### What's new in AGS 5.1
Version 5.1 is a partly developer-focused update to AGS 5. We've listened to feedback about how difficult it can be to integrate the binary AGS libs into your games, and while we can't open the source code to AGS to allow you to integrate it from source, we canvassed developers to figure out what pre-built binaries would be most useful to provide. So we've now added builds of the AGS binary library that are linkable with Visual Studio projects built with `/MT` and `/MD`, used to select a particular CRT.

There's also a breaking change in how you get access to DX11 AMD extensions. Look at the Changelog for details on that. We've also added an application registration extension for DX11 apps. That lets you tell the driver that your game can be considered in a uniquely indentifiable way, which is particularly helpful if you build on top of popular middleware like Unity or UE4 and make rendering changes.

There's also support for FreeSync 2 HDR, DX12 application user markers for [Radeon GPU Profiler](https://gpuopen.com/gaming-product/radeon-gpu-profiler-rgp/), VS2017 versions of the shipping samples, and new wave-level shader intrinsics for both DX11 and DX12.

See the full Changelog for more details.

### What's new in AGS 5.0
Version 5.0 is a major overhaul of the library designed to provide a much clearer view of the GPUs in the system and the displays attached to them. It also exposes the ability to query each display for HDR capabilities and put those HDR-capable displays into various HDR modes.

Highlights include the following:
* Full GPU enumeration with adapter string, device id, revision id and vendor id.
* Per-GPU display enumeration including information on display name, resolution, and HDR capabilities.
* Optional user-supplied memory allocator.
* Function to set displays into HDR mode.<sup>[1](#ags-sdk-footnote1)</sup>
* A Microsoft WACK compliant version of the library.
* DirectX 11 shader compiler controls.<sup>[1](#ags-sdk-footnote1)</sup>
* DirectX 11 multiview extension.<sup>[2](#ags-sdk-footnote1)</sup>
* DirectX 11 Crossfire API updates.
  * Now supports using the API without needing a driver profile.
  * You can also now specify the transfer engine.

### Driver extensions
AGS exposes GCN shader extensions for both DirectX 11 and DirectX 12. It also provides access to additional extensions available in the AMD driver for DirectX 11:
  * Quad List primitive type
  * UAV overlap
  * Depth bounds test
  * Multi-draw indirect
  * Multiview

### Prerequisites
* AMD Radeon&trade; GCN-based GPU (HD 7000 series or newer)
  * Or other DirectX&reg; 11 compatible GPU with Shader Model 5 support<sup>[3](#ags-sdk-footnote1)</sup> 
* 64-bit Windows&reg; 7 (SP1 with the [Platform Update](https://msdn.microsoft.com/en-us/library/windows/desktop/jj863687.aspx)), Windows&reg; 8.1, or Windows&reg; 10
* Visual Studio&reg; 2013 or Visual Studio&reg; 2015
* Recommended driver: Radeon Software Crimson ReLive Edition 16.12.1 (driver version 16.50.2001) or later 

### Getting Started
* It is recommended to take a look at the sample source code.
  * There are three samples: ags_sample, crossfire_sample, and eyefinity_sample.
* Visual Studio projects for VS2013 and VS2015 can be found in each sample's `build` directory.
* Additional documentation, including API documentation and instructions on how to add AGS support to an existing project, can be found in the `ags_lib\doc` directory.
  * There is also [online documentation](http://gpuopen-librariesandsdks.github.io/ags/). 

### Additional Samples
In addition to the three samples included in this repo, there are other samples available on GitHub that use AGS:
* [CrossfireAPI11](https://github.com/GPUOpen-LibrariesAndSDKs/CrossfireAPI11) - a larger example of using the explicit Crossfire API
  * The CrossfireAPI11 sample also comes with an extensive guide for multi-GPU: the *AMD Crossfire guide for Direct3D&reg; 11 applications*
* [DepthBoundsTest11](https://github.com/GPUOpen-LibrariesAndSDKs/DepthBoundsTest11) - a sample showing how to use the depth bounds test extension
* [Barycentrics11](https://github.com/GPUOpen-LibrariesAndSDKs/Barycentrics11) - a sample showing how to use the GCN shader extensions for DirectX 11
* [Barycentrics12](https://github.com/GPUOpen-LibrariesAndSDKs/Barycentrics12) - a sample showing how to use the GCN shader extensions for DirectX 12

### Premake
The Visual Studio projects in each sample's `build` directory were generated with Premake. To generate the project files yourself, open a command prompt in the sample's `premake` directory (where the premake5.lua script for that sample is located, not the top-level directory where the premake5 executable is located) and execute the following command:

* `..\..\premake\premake5.exe [action]`
* For example: `..\..\premake\premake5.exe vs2015`

Alternatively, to regenerate all Visual Studio files for the SDK, execute `ags_update_vs_files.bat` in the top-level `premake` directory.

This version of Premake has been modified from the stock version to use the property sheet technique for the Windows SDK from this [Visual C++ Team blog post](http://blogs.msdn.com/b/vcblog/archive/2012/11/23/using-the-windows-8-sdk-with-visual-studio-2010-configuring-multiple-projects.aspx). The technique was originally described for using the Windows 8.0 SDK with Visual Studio 2010, but it applies more generally to using newer versions of the Windows SDK with older versions of Visual Studio.

By default, Visual Studio 2013 projects will compile against the Windows 8.1 SDK. However, the VS2013 projects generated with this version of Premake will use the next higher SDK (i.e. the Windows 10 SDK), if the newer SDK exists on the user's machine.

For Visual Studio 2015, the systemversion Premake function is used to add the `WindowsTargetPlatformVersion` element to the project file, to specify which version of the Windows SDK will be used. To change `WindowsTargetPlatformVersion` for Visual Studio 2015, change the value for `_AMD_WIN_SDK_VERSION` in `premake\amd_premake_util.lua` and regenerate the Visual Studio files.

### Third-Party Software
* DXUT is distributed under the terms of the MIT License. See `eyefinity_sample\dxut\MIT.txt`.
* Premake is distributed under the terms of the BSD License. See `premake\LICENSE.txt`.

### Attribution
* AMD, the AMD Arrow logo, Radeon, Crossfire, and combinations thereof are either registered trademarks or trademarks of Advanced Micro Devices, Inc. in the United States and/or other countries.
* Microsoft, DirectX, Visual Studio, and Windows are either registered trademarks or trademarks of Microsoft Corporation in the United States and/or other countries.

### Notes
<a name="ags-sdk-footnote1">1</a>: Requires Radeon Software Crimson Edition 16.9.2 (driver version 16.40.2311) or later.

<a name="ags-sdk-footnote1">2</a>: Requires Radeon Software Crimson ReLive Edition 16.12.1 (driver version 16.50.2001) or later.

<a name="ags-sdk-footnote1">3</a>: While the AGS SDK samples will run on non-AMD hardware, they will be of limited usefulness, since the purpose of AGS is to provide convenient access to AMD-specific information and extensions.
