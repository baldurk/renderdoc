Timeline Bar
============

The timeline bar shows an alternate view of the scene to the :doc:`event_browser`. Displaying the scene with time running as the horizontal axis, although it does not show each section in size relative to its timing.

Introduction
------------

The timeline bar shows the scene horizontally with the hierarchical frame markers being expandable from top to bottom.

This alternate view can be a useful way to get an overview of the whole frame, allowing you to jump around very different parts of the frame.

Given that the timeline bar shows the whole frame it also provides a secondary function of showing dependencies in a global way. For whichever texture is currently selected in the texture viewer, the timeline bar shows reads and writes to that texture throughout the frame. This can be especially useful for render targets that are used in both ways, as well as simply to see where a given texture is used in a frame without laboriously searching.

.. figure:: ../imgs/Screenshots/TimelineBar.png

	Timeline Bar: The timeline bar showing a birds-eye view of a typical application.

Timeline Display
----------------

By default the timeline bar views the whole frame, but with the mouse wheel you can zoom in and out. When zoomed in, you can scroll through the frame with the horizontal scroll bar.

The timeline bar sizes the sections that it contains to try and make the frame markers as visible as possible. This attempts to keep the frame browsable for as long as possible, such that small sections (in time or number of events) are still visible rather than being crushed to tiny bars.

For this reason the timeline can resize itself when bars are expanded, as this can reveal new sections which must be allocated enough room. When there is slack space to be allocated it is given to the sections which have more events than others.

Underneath expanded sections, a blue pip is rendered for each drawcall-type event. The currently selected event is shown as a green pip, as well as there being a light grey vertical line to indicate the current position, such that this is visible even when the relevant section is not expanded.

Clicking on any section will toggle it between expanded and unexpanded, and any sections underneath a section which is collapsed will remain in their previous state but will not be visible.

Left clicking on the timeline will jump to the event underneath that point on the horizontal display.

Resource usage Display
----------------------

The timeline bar also shows the usage of the currently displayed texture.

Whichever texture is currently displayed in the :doc:`texture_viewer` (whether that be a currently bound texture, or a locked tab) will have its usage laid out across the frame on the timeline bar.

If your textures have their names annotated you will see which texture is being inspected in the label for the usage bar.

A green arrow will be drawn under each pip that reads from the texture. If the pips are too close together to resolve the arrow, the arrows will stay distinct and lose the 1:1 association with event pips.

Similarly, a purple arrow will be drawn under pips where the event writes to the texture. In the case that a resource is bound such that it can be read or written, the arrow will be half-green, half-purple.

Clear calls will be shown with a light grey.

In cases where the arrows are too close together to show distinctly, they will not all draw. To see exactly what usage is happening you can zoom into the context around that area of the frame.

.. figure:: ../imgs/Screenshots/ResourceUsage.png

	Resource Usage: The usage bar showing reads and writes to a texture.

Pixel history event Display
---------------------------

When a pixel history window is focussed, the timeline bar will show the results over each EID with a red triangle to show rejected pixels and green triangles to show passed pixels.

.. figure:: ../imgs/Screenshots/PixelHistoryTimeline.png

	Pixel History Results: The timeline bar shows the results of a pixel's history.
