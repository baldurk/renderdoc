RenderDoc for Vulkan
========

These are the release notes for an early preview version of RenderDoc with Vulkan support. The intention is to have a reliable graphics debugger for 1.0 release of Vulkan, full details of the feature set are still to be determined.

Note that for 1.0 only a baseline feature set is to be supported, see below.

Please try the tool on your workloads or programs and report back bugs encountered or general feedback. Likewise feedback is welcome about where future work should be prioritised.

If you have any questions you can contact myself (baldurk@baldurk.org) or LunarG via LunarXchange.

Current Support
========

* A single VkInstance/VkQueue/VkDevice triple is supported.
* Capture and replay of single-frame captures (not full-program streams a la vktrace).
* On replay you can step into each vkQueueSubmit call to see the command buffers submitted, and step into them to browse through the commands.
* The pipeline state will be displayed at each command, showing the data contained in each member of the pipeline createinfo struct, as well as dynamic state.
* Simple disassembly/reflection of SPIR-V to determine which descriptors to read for read-only resources and uniform buffers. The uniform buffers will be listed separately and the member variables filled out.
* Simple display of most 2D textures with mips in the texture viewer.
* You can view mesh input data both as data and a 3D preview
* Drawcall highlight and wireframe overlays
* Threading should be pretty efficient - no heavy locks on common paths (outside of creation/deletion)

Known Issues
========

* Memory/image barriers are as yet unverified, potentially could lead to bad capture or replay.
* Sparse images with mips or array slices will not properly replay
* Only 2D non-array non-integer textures can currently be displayed.
* Auto texture range-fit or histogram display is not implemented.
* Debug overlays other than drawcall highlight and wireframe aren't implemented.
* Saving textures is not supported.
* Queue-level API events are not properly listed. API calls between draw-type vkCmd... are listed.
* No post-transform mesh data is fetched
* No drawcall timings.

Future work, post 1.0
========

In no particular order, features that are not planned until after 1.0.

* More than one VkInstance/VkDevice/VkQueue triple
* Stepping inside vkCmdExecuteCommands
* Support for replaying captures on a different machine to where they were captured
* Shader debugging
* Pixel history
* Custom visualisation shaders
* Linux support with Qt UI
* Shader edit & replace
* Mesh output data capture and visualisation
