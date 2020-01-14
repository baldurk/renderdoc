import renderdoc as rd
import rdtest


class GL_Unshared_Context(rdtest.TestCase):
    demos_test_name = 'GL_Unshared_Context'

    def check_capture(self):
        
        draw = self.find_draw("Draw")

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        texs: List[rd.BoundResourceArray] = pipe.GetReadOnlyResources(rd.ShaderStage.Fragment)
        
        id = texs[0].resources[0].resourceId

        tex_details = self.get_texture(id)

        #sample 4 corners and middle
        magic_value: PixelValue = [1.0, 0.5, 0.25, 1.0];
        epsilon = .005
        
        self.check_pixel_value(id, 0, 0, magic_value, epsilon)
        self.check_pixel_value(id, tex_details.width-1, 0, magic_value, epsilon)
        self.check_pixel_value(id, 0, tex_details.height-1, magic_value, epsilon)
        self.check_pixel_value(id, tex_details.width-1, tex_details.height-1, magic_value, epsilon)
        self.check_pixel_value(id, tex_details.width/2, tex_details.height/2, magic_value, epsilon)
        
        rdtest.log.success("Texture captured properly from un-current context")

        
