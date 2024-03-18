import renderdoc as rd
import rdtest


class GL_Resource_Lifetimes(rdtest.TestCase):
    demos_test_name = 'GL_Resource_Lifetimes'
    demos_frame_cap = 200

    def check_capture(self):
        action: rd.ActionDescription = self.find_action("glDraw")

        self.controller.SetFrameEvent(action.eventId, True)

        pipe = self.controller.GetPipelineState()

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Vertex)
        location = self.controller.GetDescriptorLocations(rw[0].access.descriptorStore, [rd.DescriptorRange(rw[0].access)])[0]
        self.check_eq(location.fixedBindNumber, 3)

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Pixel)
        location = self.controller.GetDescriptorLocations(rw[0].access.descriptorStore, [rd.DescriptorRange(rw[0].access)])[0]
        self.check_eq(location.fixedBindNumber, 3)

        action: rd.ActionDescription = self.find_action("glDraw", action.eventId+1)

        self.controller.SetFrameEvent(action.eventId, True)

        pipe = self.controller.GetPipelineState()

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Vertex)
        location = self.controller.GetDescriptorLocations(rw[0].access.descriptorStore, [rd.DescriptorRange(rw[0].access)])[0]
        self.check_eq(location.fixedBindNumber, 3)

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Pixel)
        location = self.controller.GetDescriptorLocations(rw[0].access.descriptorStore, [rd.DescriptorRange(rw[0].access)])[0]
        self.check_eq(location.fixedBindNumber, 3)


        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        tex = last_action.copyDestination

        # green background around first triangle, blue around second
        self.check_pixel_value(tex, 10, 10, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 118, 10, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 118, 118, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 10, 118, [0.0, 1.0, 0.0, 1.0])

        self.check_pixel_value(tex, 138, 10, [0.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(tex, 246, 10, [0.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(tex, 246, 118, [0.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(tex, 138, 118, [0.0, 0.0, 1.0, 1.0])

        # Sample across each triangle, we expect things to be identical
        for xoffs in [0, 128]:
            # Check black checkerboard squares
            self.check_pixel_value(tex, xoffs+42, 92, [0.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(tex, xoffs+40, 85, [0.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(tex, xoffs+68, 66, [0.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(tex, xoffs+59, 47, [0.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(tex, xoffs+81, 92, [0.0, 0.0, 0.0, 1.0])

            # Check the red and green eyes of the smiley
            self.check_pixel_value(tex, xoffs+49, 83, [1.0, 0.0, 0.09, 1.0])
            self.check_pixel_value(tex, xoffs+60, 83, [0.0, 1.0, 0.09, 1.0])

            # Check the blue smile
            self.check_pixel_value(tex, xoffs+64, 72, [0.09, 0.0, 1.0, 1.0])

            # Check the orange face
            self.check_pixel_value(tex, xoffs+46, 86, [1.0, 0.545, 0.36, 1.0])

            # Check the empty space where we clamped and didn't repeat
            self.check_pixel_value(tex, xoffs+82, 79, [0.72, 1.0, 1.0, 1.0])
            self.check_pixel_value(tex, xoffs+84, 86, [0.72, 1.0, 1.0, 1.0])
            self.check_pixel_value(tex, xoffs+88, 92, [0.72, 1.0, 1.0, 1.0])

            # Check that the repeated smiley above is there
            self.check_pixel_value(tex, xoffs+67, 53, [0.905, 0.635, 0.36, 1.0])
            self.check_pixel_value(tex, xoffs+65, 50, [1.0, 0.0, 0.09, 1.0])

        # Check for resource leaks
        if len(self.controller.GetResources()) > 75:
            raise rdtest.TestFailureException(
                "Too many resources found: {}".format(len(self.controller.GetResources())))

