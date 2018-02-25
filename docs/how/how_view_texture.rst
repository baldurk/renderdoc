How do I view a specific texture?
=================================

This page documents how to annotate your resources with user-friendly names to make it easier to follow use of them throughout the frame, as well as the functions for finding and following textures by name rather than by usage.

Annotating resources with names
-------------------------------

It is much easier to browse and debug frames when the resources are given meaningful names - along with including :doc:`debugging information in shaders <how_debug_shader>` and marking up the frame with :doc:`hierarchical user-defined regions <../window/event_browser>`.

The way this is done varies by API. In D3D11 the resource is named using the ``SetPrivateData`` function:

.. highlight:: c++
.. code:: c++

	// Creating an example resource - a 2D Texture.
	ID3D11Texture2D *tex2d = NULL;
	d3dDevice->CreateTexture2D(&descriptor, NULL, &tex2d);

	// Give the buffer a useful name
	tex2d->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Example Texture"), "Example Texture");

With D3D12 you can use the ``SetName`` function:

.. highlight:: c++
.. code:: c++

	// Creating an example resource - a 2D Texture.
	ID3D12Resource *tex2d = NULL;
	d3dDevice->CreateCommittedResource(&heapProps, heapFlags, &descriptor, initState, &clearValue, __uuidof(ID3D12Resource), (void **)&tex2d);

	// Give the buffer a useful name
	tex2d->SetName(L"Example Texture");

In OpenGL this can be done with ``GL_KHR_debug`` - ``glObjectLabel``.

.. highlight:: c++
.. code:: c++

  // Creating an example resource - a 2D Texture.
  GLuint tex2d = 0;
  glGenTextures(1, &tex2d);
  glBindTexture(GL_TEXTURE_2D, tex2d);

  // apply the name, -1 means NULL terminated
  glObjectLabel(GL_TEXTURE, tex2d, -1, "Example Texture");

In Vulkan you can enable the ``VK_EXT_debug_marker`` extension, which is provided by RenderDoc, and use the ``vkDebugMarkerSetObjectNameEXT`` function.

.. highlight:: c++
.. code:: c++

  // At creation time, request the VK_EXT_debug_marker extension and
  // use vkGetInstanceProcAddr to obtain vkDebugMarkerSetObjectNameEXT

  // create the image
  VkImage tex2d;
  vkCreateImage(device, &createInfo, NULL, &tex2d);

  // set the name
  VkDebugMarkerObjectNameInfoEXT nameInfo = {};
  nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
  nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
  nameInfo.object = (uint64_t)tex2d; // this cast may vary by platform/compiler
  nameInfo.pObjectName = "Off-screen color framebuffer";
  vkDebugMarkerSetObjectNameEXT(device, &nameInfo);

When this texture is bound to the pipeline it will be listed like so:

.. figure:: ../imgs/Screenshots/NamedTex.png

	Named Texture: The example texture bound with name displayed.

In a similar way any other resource can be named and this will be useful throughout the rest of the analysis. If a custom name is not provided, a default name will be generated - as seen above with the Render Pass and Framebuffer objects.

Texture list in Texture Viewer
------------------------------

.. |page_white_stack| image:: ../imgs/icons/page_white_stack.png

In the texture viewer you can open a filterable list of all textures in the capture. This can be opened with the texture list icon |page_white_stack|. When clicked on this will open a sidebar on the texture viewer that lists all textures.

.. figure:: ../imgs/Screenshots/TexList.png

	Texture list: The sidebar showing the list of textures

This list of textures can be filtered by a custom string which will narrow the list of textures displayed, or simply by their creation flags as either a render target or a texture.

When selecting and opening one of the textures from here, a new tab is opened in the texture viewer that follows that texture.

Locked tab of a Texture
-----------------------

By default the tab open in the texture viewer follows whichever pipeline slot is currently selected. When a new event is selected this tab can display a new texture if the contents of that slot has changed.

If you want to follow a particular texture even as it becomes unbound or moves from output to input and vice versa, you can open a new locked tab that will stay consistently on this texture.

.. figure:: ../imgs/Screenshots/CurrentVsLockedTab.png

	Texture Tabs: Default tab following pipeline slot vs Locked tab.

This can be done by locating the texture by name as above, then clicking on the entry in the list. This will open up a new tab for this texture which will not change regardless of the current pipeline state, or current event.


Opening a texture from the pipeline state viewer (:doc:`how_object_details`) will also open a new locked tab for the texture in question. You can also open a new locked tab by right clicking on the texture thumbnail while it is currently bound.

.. figure:: ../imgs/Screenshots/OpenLockedTab.png

	Opening new Tab: Opening a new locked tab for a texture.

See Also
--------

* :doc:`../window/texture_viewer`
