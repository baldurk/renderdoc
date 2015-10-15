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
* Simple display of most 2D textures with mips in the texture viewer.
* Threading should be pretty efficient - no heavy locks on common paths (outside of creation/deletion)

Known Issues
========

On capture:

* Multiple VkQueues are untested and may likely not work.
* Some API functions are not currently implemented. They will work while capturing but will not replay correctly.
* Memory/image barriers are as yet unverified, potentially could lead to bad capture or replay.
* Memory maps are not intercepted, so any modifications are saved by reading back from the mapped pointer, even if it is uncached/write combined.
* Image contents are saved out by copying aliasing their backing memory to a buffer, so will not be GPU-portable.
* Captures will not be GPU-portable where memory indices etc change.
* Unsupported or untested features:
	* Subpasses
	* Nested command buffer execution
	* Push constants
	* Most event, fence and semaphore operations for synchronisation, apart from simple use in WSI extensions.
	* Sparse resources

On replay:

* Only 2D non-array non-integer textures can currently be displayed.
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
