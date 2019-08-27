How do I control the replay?
============================

RenderDoc works by capturing a frame and all resources used in it, and then replaying them with introspection and analysis of the replayed commands.

The default replay works well for the large majority of cases, but there are some fine controls that can be useful. This page details how to change those options.

Opening a capture with options
------------------------------

When a capture is opened it normally opens with the default replay options. To change those options when loading a single capture you can use :guilabel:`Open Capture with Options` from the :guilabel:`File` menu.

This brings up a window that allows you to select the file (recent files are automatically populated with the most recent selected by default) and configure the settings.

Replay options
--------------


  | :guilabel:`Use API Validation on replay` Default: ``Disabled``

Normally RenderDoc enables API validation during capture and saves any messages seen. That is the most reliable way to get API validation messages as there is a minimum of other work going on.

However if the capture was made without API validation enabled, this option allows you to instead get API validation messages from the first load of the capture. These will be visible in the :doc:`../window/debug_messages` window.

.. note::

  Due to the heavier amount of analysis work happening during replay it is possible that some different or false-positive validation messages may come through. The best solution is always to use API validation on your program without RenderDoc running at all, since RenderDoc doesn't handle all invalid API use, but if that is not possible you should prefer to enable validation at capture time.

.. _gpu-selection-override:

---------------

  | :guilabel:`GPU Selection Override` Default: ``Default GPU selection``

By default RenderDoc will try to select the closest GPU on replay to the one used in the capture. When the :guilabel:`Default GPU selection` entry is selected this algorithm is left to behave as normal.

The other entries in the list are the unique GPUs available on the system, together with the APIs they are available on. Some GPUs - such as the virtual WARP GPU for D3D - may not be available on all APIs.

If the overridden GPU is not available for a given capture, then the default selection algorithm will be used. However if the overridden GPU is available and fails to open the capture then the replay will fail - there is no attempt to fall back to a default GPU in that case.

---------------

  | :guilabel:`Replay optimisation level` Default: ``Balanced``

RenderDoc's replay contains some trade-offs between correctness and performance. This selection allows you to choose how far along that scale to go.

:guilabel:`No Optimisation` will disable all optimisations, which may be useful for isolating RenderDoc bugs.

:guilabel:`Conservative` will make some optimisations which should be invisible and undetectable to the user.

:guilabel:`Balanced` will make more optimisations which may be detectable but with a minor impact. Generally this will mean optimising out data or redundant behaviour which has no effect on the final replayed frame - for example clearing render targets where their contents can never be read by the commands in the frame, instead of restoring the contents from previous frames.

:guilabel:`Fastest` will make even more optimisations which may lead to confusing or 'impossible' data in the analysis. For example in the above case this would not even clear the render target meaning that data from later in the frame could leak back and be visible when selecting events earlier in the frame before the render target's first use. This option will not sacrifice ultimate correctness, such that the final frame results will still be correct, however inspecting intermediate data may diverge from ground truth.

.. note::

  Not all APIs are able to optimise to the same degree, so this option may not do anything or may do less on some APIs.

Changing the default options
----------------------------

In the :guilabel:`Settings` window under the :guilabel:`Replay` section you will see the same options available to allow you to change the options used globally whenever a capture is opened without specific replay options chosen.
