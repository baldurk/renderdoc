import rdtest
import renderdoc as rd


class VK_Synchronization_2(rdtest.TestCase):
    demos_test_name = 'VK_Synchronization_2'

    def get_capture_options(self):
        opts = rd.CaptureOptions()

        # Ref all resources to pull in the image with unbound data
        opts.refAllResources = True

        return opts

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

        action = self.find_action("Before Transition")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.VKState = self.controller.GetVulkanPipelineState()

        pre_init = rd.ResourceId()
        undef_img = rd.ResourceId()

        # Check that the layout is reported correctly before transitions still
        for img in pipe.images:
            img: rd.VKImageData
            res = self.get_resource(img.resourceId)
            if res.name == "Image:Preinitialised":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_PREINITIALIZED":
                    raise rdtest.TestFailureException("Pre-initialised image is in {} layout".format(img.layouts[0].name))
                pre_init = img.resourceId
            elif res.name == "Image:Undefined":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_UNDEFINED":
                    raise rdtest.TestFailureException("Undefined image is in {} layout".format(img.layouts[0].name))
                undef_img = img.resourceId
            elif res.name == "Image:Swapchain":
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR":
                    raise rdtest.TestFailureException("Swapchain image is in {} layout".format(img.layouts[0].name))

        action = self.find_action("vkCmdDraw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        # Check that the backbuffer didn't get discarded
        self.check_triangle(out=action.outputs[0])

        col = [float(0x40) / 255.0] * 4

        # The pre-initialised image should have the correct data still also
        self.check_triangle(out=pre_init, back=col, fore=col)

        # we copied its contents into the undefined image so it should also have the right colour
        self.check_triangle(out=undef_img, back=col, fore=col)

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
            elif img.resourceId == pipe.currentPass.framebuffer.attachments[0].resource:
                if img.layouts[0].name != "VK_IMAGE_LAYOUT_GENERAL":
                    raise rdtest.TestFailureException("Rendered swapchain image is in {} layout".format(img.layouts[0].name))
