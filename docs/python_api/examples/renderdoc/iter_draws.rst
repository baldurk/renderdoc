Iterate drawcall tree
=====================

In this example we will show how to iterate over drawcalls.

The drawcalls returned from :py:meth:`~renderdoc.ReplayController.GetDrawcalls` are draws or marker regions at the root level - with no parent marker region. There are multiple ways to iterate through the list of draws.

The first way illustrated in this sample is to walk the tree using :py:attr:`~renderdoc.DrawcallDescription.children`, which contains the list of child draws at any point in the tree. There is also :py:attr:`~renderdoc.DrawcallDescription.parent` which points to the parent drawcall.

The second is to use :py:attr:`~renderdoc.DrawcallDescription.previous` and :py:attr:`~renderdoc.DrawcallDescription.next`, which point to the previous and next draw respectively in a linear fashion, regardless of nesting depth.

In the example we use this iteration to determine the number of passes, using the drawcall flags to denote the start of each pass by a starting clear call.

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <iter_draws.py>`.

.. literalinclude:: iter_draws.py

Sample output:

.. sourcecode:: text

    1: Scene
        2: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 1.000000)
        3: ClearDepthStencilView(D=1.000000, S=00)
        9: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 0.000000)
        10: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 0.000000)
        11: ClearDepthStencilView(D=1.000000, S=00)
        13: GBuffer
            28: DrawIndexed(4800)
            43: DrawIndexed(36)
            56: DrawIndexed(5220)
            69: DrawIndexed(5580)
            82: DrawIndexed(5580)
            95: DrawIndexed(5580)
            108: DrawIndexed(5580)
            121: DrawIndexed(5580)
            134: DrawIndexed(5580)
            147: DrawIndexed(5580)
            160: DrawIndexed(5580)
            173: DrawIndexed(5580)
            186: DrawIndexed(5580)
            199: DrawIndexed(5220)
            212: DrawIndexed(5220)
            225: DrawIndexed(5220)
            238: DrawIndexed(5220)
            251: DrawIndexed(5220)
            264: DrawIndexed(5220)
            277: DrawIndexed(5220)
            290: DrawIndexed(5220)
            303: DrawIndexed(5220)
            316: DrawIndexed(5220)
        319: ClearDepthStencilView(D=1.000000, S=00)
        321: Shadowmap
            333: DrawIndexed(4800)
            345: DrawIndexed(36)
            355: DrawIndexed(5220)
            365: DrawIndexed(5580)
            375: DrawIndexed(5580)
            385: DrawIndexed(5580)
            395: DrawIndexed(5580)
            405: DrawIndexed(5580)
            415: DrawIndexed(5580)
            425: DrawIndexed(5580)
            435: DrawIndexed(5580)
            445: DrawIndexed(5580)
            455: DrawIndexed(5580)
            465: DrawIndexed(5220)
            475: DrawIndexed(5220)
            485: DrawIndexed(5220)
            495: DrawIndexed(5220)
            505: DrawIndexed(5220)
            515: DrawIndexed(5220)
            525: DrawIndexed(5220)
            535: DrawIndexed(5220)
            545: DrawIndexed(5220)
            555: DrawIndexed(5220)
        558: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 1.000000)
        559: ClearDepthStencilView(D=1.000000, S=00)
        561: Lighting
            563: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 0.000000)
            564: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 0.000000)
            580: DrawIndexed(36)
            597: DrawIndexed(36)
            614: DrawIndexed(36)
        617: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 1.000000)
        618: ClearDepthStencilView(D=1.000000, S=00)
        620: Shading
            630: Draw(6)
            645: DrawIndexed(960)
            652: API Calls
    655: End of Frame
    Pass #0 starts with 2: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 1.000000)
    Pass #0 contained 24 draws
    Pass #1 starts with 319: ClearDepthStencilView(D=1.000000, S=00)
    Pass #1 contained 24 draws
    Pass #2 starts with 558: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 1.000000)
    Pass #2 contained 3 draws
    Pass #3 starts with 617: ClearRenderTargetView(0.000000, 0.000000, 0.000000, 1.000000)
    Pass #3 contained 4 draws

