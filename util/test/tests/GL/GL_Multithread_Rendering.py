import renderdoc as rd
import rdtest


class GL_Multithread_Rendering(rdtest.TestCase):
    demos_test_name = 'GL_Multithread_Rendering'

    def check_capture(self):
        action = self.get_last_action()

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.25, 0.0, [0.0, 0.0, 0.0, 0.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.75, 0.0, [0.0, 0.0, 0.0, 0.0])

        tex_details: rd.TextureDescription = self.get_texture(pipe.GetOutputTargets()[0].resource)

        w = int(tex_details.width / 40)
        h = int(tex_details.height / 40)

        # Left side of the screen should be 20x40 red V shaped triangles on red background
        for y in range(40):
            for x in range(20):
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, int(x*w), int(tex_details.height - 1 - y*h), [0.3, 0.2, 0.2, 1.0])
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, int(x*w + w/2), int(tex_details.height - 1 - y*h - h/2), [1.0, 0.0, 0.25, 1.0])

        # Right side of the screen should be delta shaped blue triangles
        for y in range(40):
            for x in range(20):
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, int(tex_details.width/2) + int(x*w), int(tex_details.height - 1 - y*h - (h-1)), [0.2, 0.3, 0.2, 1.0])
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, int(tex_details.width/2) + int(x*w + w/2), int(tex_details.height - 1 - y*h - h/2), [0.0, 1.0, 0.75, 1.0])