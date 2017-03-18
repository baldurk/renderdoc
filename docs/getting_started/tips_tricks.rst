Tips & Tricks
=============

.. |go_arrow| image:: ../imgs/icons/GoArrow.png
.. |wand| image:: ../imgs/icons/wand.png

This page is a random hodge-podge of different tips and tricks that might not be obvious and aren't practical to make clear in the UI - e.g. keyboard shortcuts, edge cases and suchlike.

#. File associations for ``.rdc`` and ``.cap`` files can be set up in the installer or in the :doc:`../window/options_window`. These allow automatic opening of capture logs or capture settings files directly from files.

   .. note::

       These associations must be re-created if RenderDoc is moved to another folder.

#. RenderDoc can be used as an image viewer! If you drag in or use file → open, you can open images in a variety of formats - ``.dds``, ``.hdr``, ``.exr``, ``.bmp``, ``.jpg``, ``.png``, ``.tga``, ``.gif``, ``.psd``. The image will load up in RenderDoc's texture viewer and you can use the normal controls to view it as if it were a texture in a log. Note that ``.dds`` files support all DXGI formats, compressed formats, arrays and mips - all of which will display as expected. If the file is modified, RenderDoc will reload it and display it. Note that changing the image's dimensions or format will likely cause problems.
#. If a ``.cap`` file is saved with the "auto-start" option enabled, launching RenderDoc by opening this file will cause RenderDoc to automatically trigger a capture with the given options. This is useful for saving a common path & set of options that you regularly re-run.

   For more information check out the :doc:`../window/capture_log_attach` page.

#. If you'd like to see the geometry data with each component separated out and formatted, either open "Mesh Output" under the window menu, or click the Go Arrow |go_arrow| on the input layouts in the :doc:`../window/pipeline_state`.
#. Right clicking on one of the channel buttons in the texture viewer (R, G, B, A) will either select only that channel, or if it's already the only one selected it will select all of the others. This is useful e.g. to toggle between viewing RGB and alpha, or for looking at individual channels in a packed texture or render target.
#. Similarly, right-clicking on the 'auto-fit' button |wand| will auto-fit whenever the texture or event changes, so that the visible range is maintained as you move through the frame. This can be useful if jumping between places where the visible range is very different.
   Note though that by default the range will be remembered or each texture, so once you have fitted the range once for each texture you should be able to flip back and forth more easily.
#. You can double click on a thumbnail in the texture viewer to open a :doc:`locked texture <../how/how_view_texture>` tab
#. You can close tabs by middle clicking on them.
#. You can trigger a capture from code. ``renderdoc.dll`` exports an :doc:`../in_application_api` for this purpose, defined in ``renderdoc_app.h`` in the distributions:

   .. highlight:: c++
   .. code:: c++

       #include "renderdoc_app.h"

       RENDERDOC_API_1_0_1 *rdoc_api = NULL;

       // At init
       if(HMODULE mod = GetModuleHandleA("renderdoc.dll"))
       {
           pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
           int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_0_1, (void **)&rdoc_api);
           assert(ret == 1);
       }

       // When you wish to trigger the capture
       if(rdoc_api) rdoc_api->TriggerCapture();

   The next ``Swap()`` after this call will begin the captured frame, and the ``Swap()`` after that will end it (barring  complications)

   You can also use the ``RENDERDOC_StartFrameCapture()`` and ``RENDERDOC_EndFrameCapture()`` functions to precisely define the period to be captured. For more information look at the :doc:`../in_application_api` documentation or the ``renderdoc_app.h`` header.

#. When you have right clicked to select a pixel in the texture viewer, you can perform precise refinements with the arrow keys to nudge the selection in each direction.
#. To get API debug or error messages, enable "Enable API validation" when capturing then check out the :doc:`../window/debug_messages` window.
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

#. You can hit :kbd:`Ctrl-G` to open a popup that lets you jump to a particular co-ordinate.
#. More coming soon hopefully :).
