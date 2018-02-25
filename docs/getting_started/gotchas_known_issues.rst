Gotchas & Known Issues
======================

This page (hopefully) keeps up to date with any known issues, bugs, unimplemented or partially unimplemented features.

Things to Note
--------------

* Vulkan captures are not portable between different vendor GPUs, or possibly even between different GPUs from the same vendor if the hardware changes significantly enough.

* OpenGL is only supported from 3.2 on, so legacy GL features from 2.0 and before that were deprecated in 3.2 will not work. Similarly OpenGL ES is only supported from 2.0 and above. In addition in both cases, multiple GL contexts are only supported if they share with each other.

* RenderDoc doesn't serialise out the initial contents of large graphics resources when it believes that they will not be used in replay. e.g. a G-Buffer render target will not be saved out as it is initialised and written to in-frame. This detection will go wrong if a render target is partially written to but partially re-used, as RenderDoc will count this as initialised in-frame. This could happen e.g. with an accumulating texture that is written to in the frame over the top of previous results.

  You can override this behaviour by selecting 'Save All Initials' in the :doc:`capture options <../how/how_capture_frame>` before capturing, as this will force RenderDoc to serialise out all initial contents regardless, at the cost of larger logfiles and slightly slower replay app analysis.

* When capturing, only one swapchain is captured at any given time. The in-app overlay renders to all swapchains but only one is considered "active" at any given time - this can be cycled with the F11 key. The capture key will trigger a capture at the next swap of the currently active swapchain.

* RenderDoc relies on saving out the graphics command stream and replaying it back at inspection time. This means if a bug is timing, machine or driver specific it is in no way guaranteed to reproduce the bug on a different machine or driver.

  RenderDoc has few runtime dependencies, and should run anywhere that a normal application will run. In particular it has no dependencies on any SDKs being installed and will run on Artist or QA machines.

  Currently RenderDoc also assumes feature level 11+ hardware for the replay app. It can capture applications running at a lower feature level, but when replaying if 11+ hardware isn't available, RenderDoc will fall back to WARP software emulation and will run slowly.
* If capturing callstacks from the app, ensure that ``dbghelp.dll`` is not loaded or used by the application as this can easily interfere with RenderDoc's use and cause undefined or empty results. More information on this can be found in :doc:`../how/how_capture_callstack`.
* RenderDoc can have a significant memory overhead, especially when a lot of resources are allocated as shadow copies in main memory are created.

  If running in 32bit, it's possible that an application can run out of memory - particularly when capturing, as this causes a significant spike in memory use. Improvements in memory management are planned but for now it's recommended to use 64bit, or to limit captures to simple scenes wherever possible.
