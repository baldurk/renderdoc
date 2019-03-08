Tips & Tricks
=============

.. |go_arrow| image:: ../imgs/icons/action_hover.png
.. |wand| image:: ../imgs/icons/wand.png

This page is a random hodge-podge of different tips and tricks that might not be obvious and aren't practical to make clear in the UI - e.g. keyboard shortcuts, edge cases and suchlike.

#. RenderDoc can be used as an image viewer! If you drag in or use file → open, you can open images in a variety of formats - ``.dds``, ``.hdr``, ``.exr``, ``.bmp``, ``.jpg``, ``.png``, ``.tga``, ``.gif``, ``.psd``. The image will load up in RenderDoc's texture viewer and you can use the normal controls to view it as if it were the only texture in a capture. Note that ``.dds`` files support all DXGI formats, compressed formats, arrays and mips - all of which will display as expected. If the file is modified, RenderDoc will reload it and display it. Note that changing the image's dimensions or format will likely cause problems.
#. If a ``.cap`` file is saved with the "auto-start" option enabled, launching RenderDoc by opening this file will cause RenderDoc to automatically trigger a capture with the given options. This is useful for saving a common path & set of options that you regularly re-run.

   For more information check out the :doc:`../window/capture_attach` page.

#. If you'd like to see the geometry data visualised in 3D and with each component separated out and formatted, either open "Mesh Viewer" under the window menu, or click the Go Arrow |go_arrow| on the vertex input attributes in the :doc:`../window/pipeline_state`.
#. Right clicking on one of the channel buttons in the texture viewer (R, G, B, A) will either select only that channel, or if it's already the only one selected it will select all of the others. This is useful e.g. to toggle between viewing RGB and alpha, or for looking at individual channels in a packed texture or render target.
#. Similarly, right-clicking on the 'auto-fit' button |wand| will auto-fit whenever the texture or event changes, so that the visible range is maintained as you move through the frame. This can be useful if jumping between places where the visible range is very different.
   Note though that by default the range will be remembered or each texture, so once you have fitted the range once for each texture you should be able to flip back and forth more easily.
#. You can double click on a thumbnail in the texture viewer to open a :doc:`locked texture <../how/how_view_texture>` tab
#. You can close tabs by middle clicking on them.
#. You can trigger a capture from code. ``renderdoc.dll`` exports an :doc:`../in_application_api` for this purpose, defined in ``renderdoc_app.h`` in the distributed builds.
#. To get API debug or error messages, enable "Enable API validation" when capturing then check out the :doc:`../window/debug_messages` window.
#. You can annotate a capture by adding bookmarks, renaming resources, and adding comments. These can all be saved and embedded in the capture, so that when you share it with someone else.
#. Dragging an executable onto the RenderDoc window anywhere will open the :guilabel:`Launch Executable` panel with the executable path filled in.
#. Detecting RenderDoc from your code can either be done by trying to load and use the renderdoc :doc:`../in_application_api`, or through API specific ways:

   .. highlight:: c++
   .. code:: c++

       // For D3D11:
       ID3D11Device *devicePointer = ...;
       IUnknown *unk = NULL;
       HRESULT hr = devicePointer->QueryInterface(MAKE_GUID({A7AA6116-9C8D-4BBA-9083-B4D816B71B78}), &unk);
       if(SUCCEEDED(hr)) { /* renderdoc is present; */ }

       // For OpenGL:
       // if GL_EXT_debug_tool is present (see https://renderdoc.org/debug_tool.txt)
       glIsEnabled(GL_DEBUG_TOOL_EXT);

       // Use reserved enumerants as the extension will not become official
       #define GL_DEBUG_TOOL_EXT                 0x6789
       #define GL_DEBUG_TOOL_NAME_EXT            0x678A
       #define GL_DEBUG_TOOL_PURPOSE_EXT         0x678B

#. RenderDoc can be informed about separated debug shader blobs through API specific ways - see :ref:`unstripped-shader-info` for more details:

   .. highlight:: c++
   .. code:: c++

       // For D3D11:
       GUID RENDERDOC_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_value; // GUID value in renderdoc_app.h

       ID3D11VertexShader *shader = ...;
       std::string pathName = "/path/to/saved/blob"; // path name is in UTF-8
       // path name can also be prefixed with lz4# to indicate the blob is compressed
       pathName = "lz4#/path/to/saved/blob";

       // string parameter must be NULL-terminated, and in UTF-8
       shader->SetPrivateData(RENDERDOC_ShaderDebugMagicValue, (UINT)pathName.length(), pathName.c_str());

       // Alternatively at build time:
       struct { GUID guid; char name[MAX_PATH]; } path;

       path.guid = RENDERDOC_ShaderDebugMagicValue;
       // must include NULL-terminator, and be in UTF-8
       memcpy(path.name, debugPath.c_str(), debugPath.length() + 1);

       size_t pathSize = sizeof(GUID) + debugPath.length() + 1;

       D3DSetBlobPart(strippedBlob->GetBufferPointer(), strippedBlob->GetBufferSize(), D3D_BLOB_PRIVATE_DATA, 0, &path,        pathSize, &annotatedBlob);
       // use annotatedBlob instead of strippedBlob from here on

#. More coming soon hopefully :).

Keyboard Shortcuts
------------------

#. In the texture viewer you can hit :kbd:`Ctrl-G` to open a popup that lets you jump to a particular pixel co-ordinate.
#. In the texture viewer, after selecting a pixel you can use the arrow keys to 'nudge' one pixel at a time in any direction to fine-tune the selection.
#. To close a capture, press :kbd:`Ctrl-F4`. This will prompt to save if there are any unsaved changes.
#. Anywhere in the UI, you can use :kbd:`Ctrl-Left` and :kbd:`Ctrl-Right` to jump to the previous or next drawcall.
#. If you :doc:`add some bookmarks <../how/how_annotate_capture>` you can globally press any key from :kbd:`Ctrl-1` to :kbd:`Ctrl-0` to jump to the first 10 bookmarks.
