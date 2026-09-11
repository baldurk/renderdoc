from typing import List

import renderdoc as rd
import rdtest


class GL_Renderbuffer_Zoo(rdtest.TestCase):
    demos_test_name = 'GL_Renderbuffer_Zoo'

    def check_capture(self):
        action = self.find_action('glDraw')

        while action is not None:
            self.set_event(action.eventId, True)

            self.check_triangle(fore=[0.2, 0.75, 0.2, 1.0])

            pipe = self.controller.GetPipelineState()
            depth = pipe.GetDepthTarget()

            x, y = self.get_view_centre()

            id = pipe.GetOutputTargets()[0].resource

            mn, mx = self.controller.GetMinMax(id, rd.Subresource(), rd.CompType.Typeless)

            if not rdtest.value_compare(mn.floatValue, [0.2, 0.2, 0.2, 1.0], eps=1.0/255.0):
                raise rdtest.TestFailureException(
                    f"Minimum color values {mn.floatValue} are not as expected")

            if not rdtest.value_compare(mx.floatValue, [0.2, 0.75, 0.2, 1.0], eps=1.0/255.0):
                raise rdtest.TestFailureException(
                    f"Maximum color values {mx.floatValue} are not as expected")

            hist = self.controller.GetHistogram(id, rd.Subresource(), rd.CompType.Typeless, 0.199, 0.75,
                                                (False, True, False, False))

            if hist[0] == 0 or hist[-1] == 0 or any([x > 0 for x in hist[1:-1]]):
                raise rdtest.TestFailureException(
                    "Green histogram didn't return expected values, values should have landed in first or last bucket")

            rdtest.log.success(f'Color Renderbuffer at action {action.eventId} is working as expected')

            if depth.resource != rd.ResourceId():
                val = self.controller.PickPixel(depth.resource, x, y,
                                                rd.Subresource(), rd.CompType.Typeless)

                if not rdtest.value_compare(val.floatValue[0], 0.75):
                    raise rdtest.TestFailureException(
                        f"Picked value {val} in triangle for depth doesn't match expectation")

                mn, mx = self.controller.GetMinMax(depth.resource, rd.Subresource(), rd.CompType.Typeless)
                hist = self.controller.GetHistogram(depth.resource, rd.Subresource(),
                                                    rd.CompType.Typeless, 0.75, 0.9, (True, False, False, False))

                if not rdtest.value_compare(mn.floatValue[0], 0.75):
                    raise rdtest.TestFailureException(
                        f"Minimum depth values {mn.floatValue} are not as expected")

                if not rdtest.value_compare(mx.floatValue[0], 0.9):
                    raise rdtest.TestFailureException(
                        f"Maximum depth values {mx.floatValue} are not as expected")

                if hist[0] == 0 or hist[-1] == 0 or any([x > 0 for x in hist[1:-1]]):
                    raise rdtest.TestFailureException(
                        "Depth histogram didn't return expected values, values should have landed in first or last bucket")

                rdtest.log.success(f'Depth Renderbuffer at action {action.eventId} is working as expected')

            tex_details = self.get_texture(id)

            if tex_details.msSamp > 1:
                samples: List[bytes] = []
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

                            img_path0 = rdtest.get_tmp_path(f'sample{i}.png')
                            img_path1 = rdtest.get_tmp_path(f'sample{j}.png')

                            save_data.sample.sampleIndex = i
                            self.controller.SaveTexture(save_data, img_path0)
                            save_data.sample.sampleIndex = j
                            self.controller.SaveTexture(save_data, img_path1)

                            raise rdtest.TestFailureException("Two MSAA samples returned the same data", img_path0, img_path1)

            action = self.find_action('glDraw', action.eventId+1)

        rdtest.log.success('All renderbuffers checked and rendered correctly')
