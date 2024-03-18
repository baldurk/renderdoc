import renderdoc as rd
import rdtest


class GL_Unshared_Context(rdtest.TestCase):
    demos_test_name = 'GL_Unshared_Context'

    def check_capture(self):
        
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        texs = pipe.GetReadOnlyResources(rd.ShaderStage.Fragment)
        
        id = texs[0].descriptor.resource

        #sample 4 corners and middle
        magic_value: rd.PixelValue = [1.0, 0.5, 0.25, 1.0]
        epsilon = .005
        
        self.check_pixel_value(id, 0.0, 0.0, magic_value, eps=epsilon)
        self.check_pixel_value(id, 1.0, 0.0, magic_value, eps=epsilon)
        self.check_pixel_value(id, 0.0, 1.0, magic_value, eps=epsilon)
        self.check_pixel_value(id, 1.0, 1.0, magic_value, eps=epsilon)
        self.check_pixel_value(id, 0.5, 0.5, magic_value, eps=epsilon)
        
        rdtest.log.success("Texture captured properly from unshared context")

        
