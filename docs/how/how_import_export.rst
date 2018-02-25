How do I import or export a capture?
====================================

RenderDoc capture files are in an opaque format containing all of the data needed to construct every API object used in a capture, and then replay the captured frame.

The data is known internally as *structured data* and it can be examined in memory while a capture is opened, and exported to an external file in another format. Similarly, if the external file format contains the full set of data required then it can be imported as a RenderDoc capture.

In-capture access
-----------------

The structured data is available through the :doc:`python scripting <../window/python_shell>`. As an example, we look at one function call from a capture:

First we obtain the :py:class:`APIEvent` that we want to examine, as the last event in a drawcall's list of events and find the chunk index it refers to:

   .. highlight:: python
   .. code:: python

       event = pyrenderdoc.GetDrawcall(111).events[-1]
       print("event %d is at chunk %d" % (event.eventId, event.chunkIndex))

   .. highlight:: none
   .. code::

       > event 111 is at chunk 223

The structured data is organised as one chunk per function call or logical unit of work, so in this case we can obtain the chunk corresponding to the function call we're interested in.

Once we have the chunk, we can examine its members:

   .. highlight:: python
   .. code:: python

       chunk = pyrenderdoc.GetStructuredFile().chunks[event.chunkIndex]

       print("We have chunk '%s'" % chunk.name)

       for child in chunk.data.children:
           print("Parameter %s" % child.name)

   .. highlight:: none
   .. code::

       > Parameter commandBuffer
       > Parameter RenderPassBegin
       > Parameter contents
       > Parameter DebugMessages

From here we can drill down even further to iterate into struct members, arrays, and all the way down to basic types.

Import/Export to file
---------------------

RenderDoc offers several built-in file formats for export. Not all of these export the full set of data that can then be re-imported, some only export a certain subset.

One format that shows the full set of data is the XML exporter. There are two options - XML only, which is quick to export as it writes only the structured data, and XML+ZIP which is slower as it also exports the large buffers of data with things like texture and buffer contents.

The XML+ZIP format contains all of the data needed to construct a RenderDoc capture, and so it can also be imported from a file and loaded as a capture.

An example of the above function call exported as XML is here below:

   .. highlight:: xml
   .. code::

      <chunk id="1045" name="vkCmdBeginRenderPass" length="69" threadID="17140" timestamp="865021" duration="6">
        <ResourceId name="commandBuffer" typename="VkCommandBuffer" width="8" string="ResourceId::146">146</ResourceId>
        <struct name="RenderPassBegin" typename="VkRenderPassBeginInfo">
          <enum name="sType" typename="VkStructureType" string="VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO">43</enum>
          <null name="pNext" typename="VkGenericStruct" />
          <ResourceId name="renderPass" typename="VkRenderPass" width="8" string="ResourceId::158">158</ResourceId>
          <ResourceId name="framebuffer" typename="VkFramebuffer" width="8" string="ResourceId::130">130</ResourceId>
          <struct name="renderArea" typename="VkRect2D">
            <struct name="offset" typename="VkOffset2D">
              <int name="x" typename="int32_t" width="4">0</int>
              <int name="y" typename="int32_t" width="4">0</int>
            </struct>
            <struct name="extent" typename="VkExtent2D">
              <uint name="width" typename="uint32_t" width="4">1280</uint>
              <uint name="height" typename="uint32_t" width="4">720</uint>
            </struct>
          </struct>
          <uint name="clearValueCount" typename="uint32_t" width="4">0</uint>
          <array name="pClearValues" typename="VkClearValue" />
        </struct>
        <enum name="contents" typename="VkSubpassContents" string="VK_SUBPASS_CONTENTS_INLINE">0</enum>
        <array name="DebugMessages" typename="DebugMessage" hidden="true" />
      </chunk>
