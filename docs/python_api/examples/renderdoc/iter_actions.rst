Iterate Action tree
===================

In this example we will show how to iterate over actions.

The actions returned from :py:meth:`~renderdoc.ReplayController.GetRootActions` are actions or marker regions at the root level - with no parent marker region. There are multiple ways to iterate through the list of actions.

The first way illustrated in this sample is to walk the tree using :py:attr:`~renderdoc.ActionDescription.children`, which contains the list of child actions at any point in the tree. There is also :py:attr:`~renderdoc.ActionDescription.parent` which points to the parent action.

The second is to use :py:attr:`~renderdoc.ActionDescription.previous` and :py:attr:`~renderdoc.ActionDescription.next`, which point to the previous and next action respectively in a linear fashion, regardless of nesting depth.

In the example we use this iteration to determine the number of passes, using the action flags to denote the start of each pass by a starting clear call.

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <iter_actions.py>`.

.. literalinclude:: iter_actions.py

Sample output:

.. sourcecode:: text

    1: Scene
    2: ID3D11DeviceContext::ClearRenderTargetView()
    3: ID3D11DeviceContext::ClearDepthStencilView()
    9: ID3D11DeviceContext::ClearRenderTargetView()
    10: ID3D11DeviceContext::ClearRenderTargetView()
    11: ID3D11DeviceContext::ClearDepthStencilView()
    13: GBuffer
            25: Floor
            28: ID3D11DeviceContext::DrawIndexed()
            29: empty label
                    30: ID3DUserDefinedAnnotation::EndEvent()
            40: Base
            43: ID3D11DeviceContext::DrawIndexed()
            53: Center sphere
            56: ID3D11DeviceContext::DrawIndexed()
            66: Cone
            69: ID3D11DeviceContext::DrawIndexed()
            79: Cone
            82: ID3D11DeviceContext::DrawIndexed()
            92: Cone
            95: ID3D11DeviceContext::DrawIndexed()
            105: Cone
            108: ID3D11DeviceContext::DrawIndexed()
            118: Cone
            121: ID3D11DeviceContext::DrawIndexed()
            131: Cone
            134: ID3D11DeviceContext::DrawIndexed()
            144: Cone
            147: ID3D11DeviceContext::DrawIndexed()
            157: Cone
            160: ID3D11DeviceContext::DrawIndexed()
            170: Cone
            173: ID3D11DeviceContext::DrawIndexed()
            183: Cone
            186: ID3D11DeviceContext::DrawIndexed()
            196: Sphere
            199: ID3D11DeviceContext::DrawIndexed()
            209: Sphere
            212: ID3D11DeviceContext::DrawIndexed()
            222: Sphere
            225: ID3D11DeviceContext::DrawIndexed()
            235: Sphere
            238: ID3D11DeviceContext::DrawIndexed()
            248: Sphere
            251: ID3D11DeviceContext::DrawIndexed()
            261: Sphere
            264: ID3D11DeviceContext::DrawIndexed()
            274: Sphere
            277: ID3D11DeviceContext::DrawIndexed()
            287: Sphere
            290: ID3D11DeviceContext::DrawIndexed()
            300: Sphere
            303: ID3D11DeviceContext::DrawIndexed()
            313: Sphere
            316: ID3D11DeviceContext::DrawIndexed()
            317: ID3DUserDefinedAnnotation::EndEvent()
    319: ID3D11DeviceContext::ClearDepthStencilView()
    321: Shadowmap
            330: Floor
            333: ID3D11DeviceContext::DrawIndexed()
            334: empty label
                    335: ID3DUserDefinedAnnotation::EndEvent()
            342: Base
            345: ID3D11DeviceContext::DrawIndexed()
            352: Center sphere
            355: ID3D11DeviceContext::DrawIndexed()
            362: Cone
            365: ID3D11DeviceContext::DrawIndexed()
            372: Cone
            375: ID3D11DeviceContext::DrawIndexed()
            382: Cone
            385: ID3D11DeviceContext::DrawIndexed()
            392: Cone
            395: ID3D11DeviceContext::DrawIndexed()
            402: Cone
            405: ID3D11DeviceContext::DrawIndexed()
            412: Cone
            415: ID3D11DeviceContext::DrawIndexed()
            422: Cone
            425: ID3D11DeviceContext::DrawIndexed()
            432: Cone
            435: ID3D11DeviceContext::DrawIndexed()
            442: Cone
            445: ID3D11DeviceContext::DrawIndexed()
            452: Cone
            455: ID3D11DeviceContext::DrawIndexed()
            462: Sphere
            465: ID3D11DeviceContext::DrawIndexed()
            472: Sphere
            475: ID3D11DeviceContext::DrawIndexed()
            482: Sphere
            485: ID3D11DeviceContext::DrawIndexed()
            492: Sphere
            495: ID3D11DeviceContext::DrawIndexed()
            502: Sphere
            505: ID3D11DeviceContext::DrawIndexed()
            512: Sphere
            515: ID3D11DeviceContext::DrawIndexed()
            522: Sphere
            525: ID3D11DeviceContext::DrawIndexed()
            532: Sphere
            535: ID3D11DeviceContext::DrawIndexed()
            542: Sphere
            545: ID3D11DeviceContext::DrawIndexed()
            552: Sphere
            555: ID3D11DeviceContext::DrawIndexed()
            556: ID3DUserDefinedAnnotation::EndEvent()
    558: ID3D11DeviceContext::ClearRenderTargetView()
    559: ID3D11DeviceContext::ClearDepthStencilView()
    561: Lighting
            563: ID3D11DeviceContext::ClearRenderTargetView()
            564: ID3D11DeviceContext::ClearRenderTargetView()
            566: Point light
            580: ID3D11DeviceContext::DrawIndexed()
            581: Sun light
            597: ID3D11DeviceContext::DrawIndexed()
            600: Cube light
            614: ID3D11DeviceContext::DrawIndexed()
            615: ID3DUserDefinedAnnotation::EndEvent()
    617: ID3D11DeviceContext::ClearRenderTargetView()
    618: ID3D11DeviceContext::ClearDepthStencilView()
    620: Shading
            630: ID3D11DeviceContext::Draw()
            645: ID3D11DeviceContext::DrawIndexed()
            652: ID3DUserDefinedAnnotation::EndEvent()
    653: ID3DUserDefinedAnnotation::EndEvent()
    654: Present(ResourceId::47)
    Pass #0 starts with 2: ID3D11DeviceContext::ClearRenderTargetView()
    Pass #0 contained 23 actions
    Pass #1 starts with 319: ID3D11DeviceContext::ClearDepthStencilView()
    Pass #1 contained 23 actions
    Pass #2 starts with 558: ID3D11DeviceContext::ClearRenderTargetView()
    Pass #2 contained 3 actions
    Pass #3 starts with 617: ID3D11DeviceContext::ClearRenderTargetView()
    Pass #3 contained 3 actions
