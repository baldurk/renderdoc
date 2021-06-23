import renderdoc as rd
import rdtest


class D3D12_RenderTarget_Binds(rdtest.TestCase):
    demos_test_name = 'D3D12_RenderTarget_Binds'

    def check_capture(self):
        # find the clear
        action = self.find_action("ClearRenderTargetView")

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(action.copyDestination, 0.5, 0.5, [1.0, 0.0, 1.0, 1.0])

        self.check('Swapchain' in self.get_resource(action.copyDestination).name)

        rdtest.log.success("Picked value for clear is as expected")

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rtvs = pipe.GetOutputTargets()
        self.check(len(rtvs) == 2)

        self.check_triangle(out=rtvs[0].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[1.0, 0.0, 0.0, 1.0])
        self.check_triangle(out=rtvs[1].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[0.0, 0.0, 1.0, 1.0])
        self.check(self.get_resource(rtvs[0].resourceId).name == 'TextureA')
        self.check(self.get_resource(rtvs[1].resourceId).name == 'TextureB')

        rdtest.log.success("RTVs at first action are as expected")

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rtvs = pipe.GetOutputTargets()
        self.check(len(rtvs) == 2)

        self.check_triangle(out=rtvs[0].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[1.0, 1.0, 0.0, 1.0])
        self.check_triangle(out=rtvs[1].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[0.0, 1.0, 1.0, 1.0])
        self.check(self.get_resource(rtvs[0].resourceId).name == 'TextureC')
        self.check(self.get_resource(rtvs[1].resourceId).name == 'TextureD')

        rdtest.log.success("RTVs at second action are as expected")

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rtvs = pipe.GetOutputTargets()
        self.check(len(rtvs) == 2)

        self.check_triangle(out=rtvs[0].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[1.0, 0.0, 0.0, 1.0])
        self.check_triangle(out=rtvs[1].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[0.0, 0.0, 1.0, 1.0])
        self.check(self.get_resource(rtvs[0].resourceId).name == 'TextureE')
        self.check(self.get_resource(rtvs[1].resourceId).name == 'TextureF')

        rdtest.log.success("RTVs at third action are as expected")

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rtvs = pipe.GetOutputTargets()
        self.check(len(rtvs) == 2)

        self.check_triangle(out=rtvs[0].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[1.0, 1.0, 0.0, 1.0])
        self.check_triangle(out=rtvs[1].resourceId, back=[0.0, 1.0, 0.0, 1.0], fore=[0.0, 1.0, 1.0, 1.0])
        self.check(self.get_resource(rtvs[0].resourceId).name == 'TextureG')
        self.check(self.get_resource(rtvs[1].resourceId).name == 'TextureH')

        rdtest.log.success("RTVs at fourth action are as expected")
