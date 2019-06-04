import rdtest
import renderdoc as rd


class VK_Image_Layouts(rdtest.TestCase):
    demos_test_name = 'VK_Image_Layouts'

    def check_capture(self):
        self.controller.SetFrameEvent(0, False)

        pipe: rd.VKState = self.controller.GetVulkanPipelineState()

        # Check that the layout is reported correctly at the start of the frame
        for img in pipe.images:
            img: rd.VKImageData
            res = self.get_resource(img.resourceId)
            if res.name == "Image:Preinitialised":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_PREINITIALIZED":
                    raise rdtest.TestFailureException("Pre-initialised image is in {} layout".format(img.layouts[0].name))
            elif res.name == "Image:Undefined":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_UNDEFINED":
                    raise rdtest.TestFailureException("Undefined image is in {} layout".format(img.layouts[0].name))
            elif res.name == "Image:Swapchain":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR":
                    raise rdtest.TestFailureException("Swapchain image is in {} layout".format(img.layouts[0].name))

        draw = self.find_draw("Before Transition")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.VKState = self.controller.GetVulkanPipelineState()

        # Check that the layout is reported correctly before transitions still
        for img in pipe.images:
            img: rd.VKImageData
            res = self.get_resource(img.resourceId)
            if res.name == "Image:Preinitialised":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_PREINITIALIZED":
                    raise rdtest.TestFailureException("Pre-initialised image is in {} layout".format(img.layouts[0].name))
            elif res.name == "Image:Undefined":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_UNDEFINED":
                    raise rdtest.TestFailureException("Undefined image is in {} layout".format(img.layouts[0].name))
            elif res.name == "Image:Swapchain":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR":
                    raise rdtest.TestFailureException("Swapchain image is in {} layout".format(img.layouts[0].name))

        draw = self.find_draw("vkCmdDraw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.VKState = self.controller.GetVulkanPipelineState()

        # Check that after transitions, the images are in the right state
        for img in pipe.images:
            img: rd.VKImageData
            res = self.get_resource(img.resourceId)
            if res.name == "Image:Preinitialised":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL":
                    raise rdtest.TestFailureException("Pre-initialised image is in {} layout".format(img.layouts[0].name))
            elif res.name == "Image:Undefined":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL":
                    raise rdtest.TestFailureException("Undefined image is in {} layout".format(img.layouts[0].name))
            elif img.resourceId == pipe.currentPass.framebuffer.attachments[0].imageResourceId:
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_GENERAL":
                    raise rdtest.TestFailureException("Rendered swapchain image is in {} layout".format(img.layouts[0].name))
