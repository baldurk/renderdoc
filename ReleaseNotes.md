RenderDoc for Vulkan
========

These are the release notes for an early preview version of RenderDoc with Vulkan support. The intention is to have a reliable graphics debugger for 1.0 release of Vulkan, full details of the feature set are still to be determined.

This build is still quite early in development and is provided primarily for early feedback and awareness, to give an idea to Khronos and Vulkan Advisors of what is in the pipeline. A further release will be made in the near future when the debugger has been stabilised and is more ready to be stressed.

With the known issues listed below in mind, anyone can try the tool and report back bugs encountered or general feedback. Likewise feedback is welcome about where future work should be prioritised.

If you have any questions you can contact myself (baldurk@baldurk.org) or LunarG via LunarXchange.

Current Support
========

* A single VkInstance/VkQueue/VkDevice triple is supported.
* Capture and replay of a single frame at a time.
* On replay you can step into each vkQueueSubmit call to see the command buffers submitted, and step into them to browse through the commands.
* The pipeline state will be displayed at each command, roughly showing the data contained in each member of the pipeline createinfo struct, as well as dynamic state.
* Simple disassembly/reflection of SPIR-V to determine which descriptors to read for read-only resources and uniform buffers. The uniform buffers will be listed separately and the member variables filled out.
* Simple display of most 2D textures in the texture viewer.

Known Issues
========

On capture:

* Multiple VkQueues are untested and may likely not work.
* Some API functions are not currently hooked and will crash if called by the application to be debugged (see note 1).
* Subpasses aren't supported.
* Push constants aren't supported.
* Memory/image barriers are as yet unverified, potentially could lead to bad capture or replay.
* Sparse resources are not supported.
* Memory maps are not intercepted, so any modifications are saved by reading back from the mapped pointer, even if it is uncached/write combined.
* Image contents are saved out by copying aliasing their backing memory to a buffer, so will not be GPU-portable.
* Captures will not be GPU-portable where memory indices etc change.

On replay:

* Only 2D non-array non-integer textures can currently be displayed, and only the first mip.
* Pixel values aren't fetched.
* Auto texture range-fit or histogram display is not implemented.
* Debug overlays aren't implemented.
* The display pipeline is not yet gamma correct.
* Saving textures is not supported.
* Queue-level API events are not properly listed. API calls between draw-type vkCmd... are listed.
* Meshes are not rendered as a preview.
* No drawcall timings.

Future work, post 1.0
========

In no particular order, features that are not planned until after 1.0.

* Multiple VkDevices and multiple VkInstances
* Shader debugging
* Pixel history
* Custom visualisation shaders
* Linux support with Qt UI
* Shader edit & replace
* Mesh output data capture and visualisation


Note 1, unsupported entry points:

* vkResetFences
* vkWaitForFences

* vkCreateEvent
* vkDestroyEvent
* vkGetEventStatus
* vkSetEvent
* vkResetEvent
* vkCmdSetEvent
* vkCmdResetEvent
* vkCmdWaitEvents
