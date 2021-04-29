import renderdoc as rd
import rdtest


class D3D12_VRS(rdtest.TestCase):
    demos_test_name = 'D3D12_VRS'

    def get_shading_rates(self):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        v = pipe.GetViewport(0)
        tex = pipe.GetOutputTargets()[0].resourceId

        # Ensure we check even-based quads
        x = int(v.x) - int(v.x % 2)
        y = int(v.y) - int(v.y % 2)

        return (self.get_shading_rate_for_quad(tex, x + 24, y + 50),
                self.get_shading_rate_for_quad(tex, x + 74, y + 42))

    def get_shading_rate_for_quad(self, tex, x, y):
        picked = [self.controller.PickPixel(tex, x+0, y+0, rd.Subresource(), rd.CompType.Typeless),
                  self.controller.PickPixel(tex, x+1, y+0, rd.Subresource(), rd.CompType.Typeless),
                  self.controller.PickPixel(tex, x+0, y+1, rd.Subresource(), rd.CompType.Typeless),
                  self.controller.PickPixel(tex, x+1, y+1, rd.Subresource(), rd.CompType.Typeless)]

        # all same - 2x2
        if all([p.floatValue == picked[0].floatValue for p in picked]):
            return "2x2"
        # X same Y diff - 2x1
        if (picked[0].floatValue == picked[1].floatValue) and (picked[2].floatValue == picked[3].floatValue) and \
                (picked[0].floatValue != picked[2].floatValue):
            return "2x1"
        # X diff Y same - 1x2
        if (picked[0].floatValue == picked[2].floatValue) and (picked[1].floatValue == picked[3].floatValue) and \
                (picked[0].floatValue != picked[1].floatValue):
            return "1x2"
        # all different - 1x1
        if all([p.floatValue != picked[0].floatValue for p in picked[1:]]):
            return "1x1"
        return "?x?"

    def check_capture(self):
        # we do two passes, first when we're selecting the actual draws and second when we're in a second command buffer
        # going over the same viewports but with dummy draws. To ensure the results are the same whether or not we're
        # in the VRS command buffer
        for pass_name in ["First", "Second"]:
            pass_draw = self.find_draw(pass_name)
            
            draw = self.find_draw("Default", pass_draw.eventId)
            self.check(draw is not None)
            self.controller.SetFrameEvent(draw.next.eventId, False)

            num_checks = 0

            self.check(self.get_shading_rates() == ("1x1", "1x1"),
                       "{} shading rates unexpected: {}".format(draw.name, self.get_shading_rates()))
            num_checks += 1

            draw = self.find_draw("Base", pass_draw.eventId)
            self.controller.SetFrameEvent(draw.next.eventId, False)
            self.check(self.get_shading_rates() == ("2x2", "2x2"),
                       "{} shading rates unexpected: {}".format(draw.name, self.get_shading_rates()))
            num_checks += 1

            draw = self.find_draw("Vertex", pass_draw.eventId)
            if draw is not None:
                self.controller.SetFrameEvent(draw.next.eventId, False)
                self.check(self.get_shading_rates() == ("1x1", "2x2"),
                           "{} shading rates unexpected: {}".format(draw.name, self.get_shading_rates()))
                num_checks += 1
                rdtest.log.success("Shading rates were as expected in per-vertex case")

            draw = self.find_draw("Image", pass_draw.eventId)
            if draw is not None:
                self.controller.SetFrameEvent(draw.next.eventId, False)
                self.check(self.get_shading_rates() == ("2x2", "1x1"),
                           "{} shading rates unexpected: {}".format(draw.name, self.get_shading_rates()))
                num_checks += 1
                rdtest.log.success("Shading rates were as expected in image-based case")

            draw = self.find_draw("Base + Vertex", pass_draw.eventId)
            if draw is not None:
                self.controller.SetFrameEvent(draw.next.eventId, False)
                self.check(self.get_shading_rates() == ("2x2", "2x2"),
                           "{} shading rates unexpected: {}".format(draw.name, self.get_shading_rates()))
                num_checks += 1

            draw = self.find_draw("Base + Image", pass_draw.eventId)
            if draw is not None:
                self.controller.SetFrameEvent(draw.next.eventId, False)
                self.check(self.get_shading_rates() == ("2x2", "2x2"),
                           "{} shading rates unexpected: {}".format(draw.name, self.get_shading_rates()))
                num_checks += 1

            draw = self.find_draw("Vertex + Image", pass_draw.eventId)
            if draw is not None:
                self.controller.SetFrameEvent(draw.next.eventId, False)
                self.check(self.get_shading_rates() == ("2x2", "2x2"),
                           "{} shading rates unexpected: {}".format(draw.name, self.get_shading_rates()))
                num_checks += 1

            rdtest.log.success("{}pass: Shading rates were as expected in {} test cases".format(pass_name, num_checks))