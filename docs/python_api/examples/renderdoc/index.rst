renderdoc Examples
==================

Here we have some examples of the lower level renderdoc API. Some may be only relevant when using the module directly, but most are general and can be followed either when running in python or in the UI.

In order to help with this, the examples are organised such that most code is written within a function that accepts the replay controller, and the code from :doc:`../renderdoc_intro` runs only when standalone and not within the UI:

.. note::

    While some of the samples will work within the UI, because they directly access the lower level API and skip the UI's API, it may cause some state inconsistencies. See :doc:`../qrenderdoc/index` for examples using the UI API directly.

.. highlight:: python
.. code:: python

    def sampleCode(controller):
        print("Here we can use the replay controller")

    if 'pyrenderdoc' in globals():
        pyrenderdoc.Replay().BlockInvoke(sampleCode)
    else:
        cap,controller = loadCapture('test.rdc')

        sampleCode(controller)

        controller.Shutdown()
        cap.Shutdown()

.. toctree::
    iter_draws
    fetch_shader
    fetch_counters
    save_texture
    decode_mesh
    display_window