Gotchas & Known Issues
======================

This page (hopefully) keeps up to date with any known issues, bugs, unimplemented or partially unimplemented features.

Things to Note
--------------

* RenderDoc doesn't serialise out the initial contents of large graphics resources when it believes that they will not be used in replay. e.g. a G-Buffer render target will not be saved out as it is initialised and written to in-frame. This detection will go wrong if a render target is partially written to but partially re-used, as RenderDoc will count this as initialised in-frame. This could happen e.g. with an accumulating texture that is written to in the frame over the top of previous results.

  You can override this behaviour by selecting 'Save All Initials' in the :doc:`capture options <../how/how_capture_log>` before capturing, as this will force RenderDoc to serialise out all initial contents regardless, at the cost of larger logfiles and slightly slower replay app analysis.

* When capturing, only one swapchain is captured at any given time. The in-app overlay renders to all swapchains but only one is considered "active" at any given time - this can be cycled with the F11 key. The capture key will trigger a capture at the next swap of the currently active swapchain.

* RenderDoc relies on saving out the graphics command stream and replaying it back at inspection time. This means if a bug is timing, machine or driver specific it is in no way guaranteed to reproduce the bug on a different machine or driver.

  RenderDoc has no runtime dependencies on Visual Studio or the DirectX or Windows SDK, and should run anywhere that a normal DirectX application will run - i.e. on Artist or QA machines.

  Currently RenderDoc also assumes feature level 11+ hardware for the replay app. It can capture applications running at a lower feature level, but when replaying if 11+ hardware isn't available, RenderDoc will fall back to WARP software emulation and will run slowly.
* If capturing callstacks from the app, ensure that ``dbghelp.dll`` is not loaded or used by the application as this can easily interfere with RenderDoc's use and cause undefined or empty results. More information on this can be found in :doc:`../how/how_capture_callstack`.
* RenderDoc can have a significant memory overhead, especially when a lot of resources are allocated as shadow copies in main memory are created.

  If running in 32bit, it's possible that an application can run out of memory - particularly when capturing, as this causes a significant spike in memory use. Improvements in memory management are planned but for now it's recommended to use 64bit, or to limit captures to simple scenes wherever possible.

Partially Implemented Features
------------------------------


* Deferred context & command list support will probably run into problems with non-trivial use-cases. Let me know if you find a use-case that breaks, as I don't have many test programs!
* The API Inspector shows essentially the raw serialised form of the commands in the log file and so is not always very useful beyond showing which functions were called. There isn't a way yet to see what views a particular ID corresponds to, and some of the parameters are a little different from their official function signature.
* There are several such notes for OpenGL, which are noted on :any:`its own page <../behind_scenes/opengl_support>`, as well as for Vulkan on :doc:`its own page <../behind_scenes/vulkan_support>`.
