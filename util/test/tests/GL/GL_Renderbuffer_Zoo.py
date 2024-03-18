import renderdoc as rd
import rdtest


class GL_Renderbuffer_Zoo(rdtest.TestCase):
    demos_test_name = 'GL_Renderbuffer_Zoo'

    def check_capture(self):
        action: rd.ActionDescription = self.find_action('glDraw')

        while action is not None:
            self.controller.SetFrameEvent(action.eventId, True)

            self.check_triangle(fore=[0.2, 0.75, 0.2, 1.0])

            pipe = self.controller.GetPipelineState()
            depth = pipe.GetDepthTarget()
            vp = pipe.GetViewport(0)

            id = pipe.GetOutputTargets()[0].resource

            mn, mx = self.controller.GetMinMax(id, rd.Subresource(), rd.CompType.Typeless)

            if not rdtest.value_compare(mn.floatValue, [0.2, 0.2, 0.2, 1.0], eps=1.0/255.0):
                raise rdtest.TestFailureException(
                    "Minimum color values {} are not as expected".format(mn.floatValue))

            if not rdtest.value_compare(mx.floatValue, [0.2, 0.75, 0.2, 1.0], eps=1.0/255.0):
                raise rdtest.TestFailureException(
                    "Maximum color values {} are not as expected".format(mx.floatValue))

            hist = self.controller.GetHistogram(id, rd.Subresource(), rd.CompType.Typeless, 0.199, 0.75,
                                                (False, True, False, False))

            if hist[0] == 0 or hist[-1] == 0 or any([x > 0 for x in hist[1:-1]]):
                raise rdtest.TestFailureException(
                    "Green histogram didn't return expected values, values should have landed in first or last bucket")

            rdtest.log.success('Color Renderbuffer at action {} is working as expected'.format(action.eventId))

            if depth.resource != rd.ResourceId():
                val = self.controller.PickPixel(depth.resource, int(0.5 * vp.width), int(0.5 * vp.height),
                                                rd.Subresource(), rd.CompType.Typeless)

                if not rdtest.value_compare(val.floatValue[0], 0.75):
                    raise rdtest.TestFailureException(
                        "Picked value {} in triangle for depth doesn't match expectation".format(val))

                mn, mx = self.controller.GetMinMax(depth.resource, rd.Subresource(), rd.CompType.Typeless)
                hist = self.controller.GetHistogram(depth.resource, rd.Subresource(),
                                                    rd.CompType.Typeless, 0.75, 0.9, (True, False, False, False))

                if not rdtest.value_compare(mn.floatValue[0], 0.75):
                    raise rdtest.TestFailureException(
                        "Minimum depth values {} are not as expected".format(mn.floatValue))

                if not rdtest.value_compare(mx.floatValue[0], 0.9):
                    raise rdtest.TestFailureException(
                        "Maximum depth values {} are not as expected".format(mx.floatValue))

                if hist[0] == 0 or hist[-1] == 0 or any([x > 0 for x in hist[1:-1]]):
                    raise rdtest.TestFailureException(
                        "Depth histogram didn't return expected values, values should have landed in first or last bucket")

                rdtest.log.success('Depth Renderbuffer at action {} is working as expected'.format(action.eventId))

            tex_details = self.get_texture(id)

            if tex_details.msSamp > 1:
                samples = []
                for i in range(tex_details.msSamp):
                    samples.append(self.controller.GetTextureData(id, rd.Subresource(0, 0, i)))

                for i in range(tex_details.msSamp):
                    for j in range(tex_details.msSamp):
                        if i == j:
                            continue

                        if samples[i] == samples[j]:
                            save_data = rd.TextureSave()
                            save_data.resourceId = id
                            save_data.destType = rd.FileType.PNG
                            save_data.slice.sliceIndex = 0
                            save_data.mip = 0

                            img_path0 = rdtest.get_tmp_path('sample{}.png'.format(i))
                            img_path1 = rdtest.get_tmp_path('sample{}.png'.format(j))

                            save_data.sample.sampleIndex = i
                            self.controller.SaveTexture(save_data, img_path0)
                            save_data.sample.sampleIndex = j
                            self.controller.SaveTexture(save_data, img_path1)

                            raise rdtest.TestFailureException("Two MSAA samples returned the same data", img_path0, img_path1)

            action: rd.ActionDescription = self.find_action('glDraw', action.eventId+1)

        rdtest.log.success('All renderbuffers checked and rendered correctly')
