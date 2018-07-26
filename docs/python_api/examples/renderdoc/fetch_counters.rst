Fetch GPU Counter Data
======================

In this example we will gather GPU counter data over a capture and find any drawcalls that completely failed the depth/stencil test.

The first thing we do is enumerate a list of counters that the implementation supports using :py:meth:`~renderdoc.ReplayController.EnumerateCounters`. A few of these counters values are statically known - see :py:class:`~renderdoc.GPUCounter`. If you know which counter you want ahead of time you can continue straight away by calling :py:meth:`~renderdoc.ReplayController.FetchCounters` with a list of counters to sample from, or :py:meth:`~renderdoc.ReplayController.DescribeCounter` to obtain a :py:class:`~renderdoc.CounterDescription` of the counter itself:

.. highlight:: python
.. code:: python

	# Enumerate the available counters
	counters = controller.EnumerateCounters()

	if not (rd.GPUCounter.SamplesPassed in counters):
		raise RuntimeError("Implementation doesn't support Samples Passed counter")

	# Now we fetch the counter data, this is a good time to batch requests of as many
	# counters as possible, the implementation handles any book keeping.
	results = controller.FetchCounters([rd.GPUCounter.SamplesPassed])

	# Get the description for the counter we want
	samplesPassedDesc = controller.DescribeCounter(rd.GPUCounter.SamplesPassed)

However we will also print all available counters. If the implementation supports vendor-specific counters they will be enumerated as well and we can print their descriptions.

.. highlight:: python
.. code:: python

	# Describe each counter
	for c in counters:
		desc = controller.DescribeCounter(c)

		print("Counter %d (%s):" % (c, desc.name))
		print("    %s" % desc.description)
		print("    Returns %d byte %s, representing %s" % (desc.resultByteWidth, desc.resultType, desc.unit))

Once we have the list of :py:class:`~renderdoc.CounterResult` from sampling the specified counters, each result returned is for one counter on one event. Since we only fetched one counter we can simply iterate over the results looking up the drawcall for each. For actual draws (excluding clears and markers etc) we use the counter description to determine the data payload size, and get the value out. Interpreting this can either happen based on the description, or in our case we know that this counter returns a simple value we can check:

.. highlight:: python
.. code:: python

	# Look in the results for any draws with 0 samples written - this is an indication
	# that if a lot of draws appear then culling could be better.
	for r in results:
		draw = draws[r.eventId]

		# Only care about draws, not about clears and other misc events
		if not (draw.flags & rd.DrawFlags.Drawcall):
			continue

		if samplesPassedDesc.resultByteWidth == 4:
			val = r.value.u32
		else:
			val = r.value.u64

		if val == 0:
			print("EID %d '%s' had no samples pass depth/stencil test!" % (r.eventId, draw.name))

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <fetch_counters.py>`.

.. literalinclude:: fetch_counters.py

Sample output:

.. sourcecode:: text

    Counter 1 (GPU Duration):
        Time taken for this event on the GPU, as measured by delta between two GPU timestamps.
        Returns 8 byte CompType.Double, representing CounterUnit.Seconds
    Counter 2 (Input Vertices Read):
        Number of vertices read by input assembler.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 3 (Input Primitives):
        Number of primitives read by the input assembler.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 4 (GS Primitives):
        Number of primitives output by a geometry shader.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 5 (Rasterizer Invocations):
        Number of primitives that were sent to the rasterizer.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 6 (Rasterized Primitives):
        Number of primitives that were rendered.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 7 (Samples Passed):
        Number of samples that passed depth/stencil test.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
        ... which we will sample
    Counter 8 (VS Invocations):
        Number of times a vertex shader was invoked.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 9 (HS Invocations):
        Number of times a hull shader was invoked.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 10 (DS Invocations):
        Number of times a domain shader (or tesselation evaluation shader in OpenGL) was invoked.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 11 (GS Invocations):
        Number of times a geometry shader was invoked.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 12 (PS Invocations):
        Number of times a pixel shader was invoked.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    Counter 13 (CS Invocations):
        Number of times a compute shader was invoked.
        Returns 8 byte CompType.UInt, representing CounterUnit.Absolute
    EID 69 'DrawIndexed(5580)' had no samples pass depth/stencil test!
    EID 82 'DrawIndexed(5580)' had no samples pass depth/stencil test!
    EID 95 'DrawIndexed(5580)' had no samples pass depth/stencil test!
    EID 108 'DrawIndexed(5580)' had no samples pass depth/stencil test!
    EID 199 'DrawIndexed(5220)' had no samples pass depth/stencil test!
    EID 212 'DrawIndexed(5220)' had no samples pass depth/stencil test!
    EID 225 'DrawIndexed(5220)' had no samples pass depth/stencil test!
    EID 238 'DrawIndexed(5220)' had no samples pass depth/stencil test!