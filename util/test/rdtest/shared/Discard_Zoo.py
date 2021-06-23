import renderdoc as rd
import rdtest

# Not a real test, re-used by API-specific tests
class Discard_Zoo(rdtest.TestCase):
    internal = True

    def check_val(self, picked, val, fmt):
        if type(val) != list:
            val = [val, val, val, val]

        if fmt.compType == rd.CompType.UInt or fmt.compType == rd.CompType.SInt:
            val = [int(a) for a in val]
            return rdtest.value_compare(picked.intValue[0:fmt.compCount], val[0:fmt.compCount])
        else:
            comp_val = picked.floatValue[0:fmt.compCount]
            if fmt.compType == rd.CompType.Depth or fmt.type in [rd.ResourceFormatType.D16S8, rd.ResourceFormatType.D24S8, rd.ResourceFormatType.D32S8]:
                comp_val = [min(1.0, a) for a in comp_val]

            return rdtest.value_compare(comp_val, val[0:fmt.compCount])

    def check_texture(self, id, discarded: bool):
        tex: rd.TextureDescription = self.get_texture(id)
        res: rd.ResourceDescription = self.get_resource(id)

        fmt: rd.ResourceFormat = tex.format

        props: rd.APIProperties = self.controller.GetAPIProperties()
        gl = (props.pipelineType == rd.GraphicsAPI.OpenGL)

        name = '{} - {}x{} {} mip {} slice {}x MSAA {} format texture'.format(res.name, tex.width, tex.height,
                                                                              tex.mips, tex.arraysize,
                                                                              tex.msSamp, tex.format.Name())

        minval = 0.0
        maxval = 1000.0

        if fmt.type == rd.ResourceFormatType.R9G9B9E5:
            maxval = 998.0
        elif fmt.type == rd.ResourceFormatType.BC6:
            maxval = 996.0
        elif fmt.type == rd.ResourceFormatType.R10G10B10A2 and fmt.compType == rd.CompType.UInt:
            maxval = [127.0, 127.0, 127.0, 3.0]
        elif fmt.type in [rd.ResourceFormatType.D16S8, rd.ResourceFormatType.D24S8, rd.ResourceFormatType.D32S8]:
            maxval = 1.0
            fmt.compCount = 2

            if "DepthOnly" in res.name:
                minval = [minval, float(0x40)/float(255)]
                maxval = [maxval, float(0x40)/float(255)]
            elif "StencilOnly" in res.name:
                minval = [0.4, minval]
                maxval = [0.4, maxval]
        elif fmt.type == rd.ResourceFormatType.S8:
            minval = [0.0, 0.0]
            maxval = [0.0, 1.0]
            fmt.compCount = 2
        elif fmt.compType == rd.CompType.UNorm or fmt.compType == rd.CompType.Depth:
            maxval = 1.0
        elif fmt.compType == rd.CompType.UInt or fmt.compType == rd.CompType.SInt:
            maxval = 127.0

        # ignore alpha in BC1 and BC6
        if fmt.type == rd.ResourceFormatType.BC1 or fmt.type == rd.ResourceFormatType.BC6:
            fmt.compCount = 3

        for mip in range(tex.mips):
            for slice in range(tex.arraysize):
                for samp in range(tex.msSamp):
                    sub = rd.Subresource(mip, slice, samp)

                    sub_discarded = discarded

                    if "Mip" in res.name:
                        idx = res.name.index('Mip')+3
                        try:
                            idx2 = res.name.index(' ', idx)
                        except ValueError:
                            idx2 = len(res.name)
                        if mip not in [int(m) for m in res.name[idx:idx2].split(',')]:
                            sub_discarded = False

                    if "Slice" in res.name:
                        idx = res.name.index('Slice') + 5
                        try:
                            idx2 = res.name.index(' ', idx)
                        except ValueError:
                            idx2 = len(res.name)
                        if slice not in [int(s) for s in res.name[idx:idx2].split(',')]:
                            sub_discarded = False

                    if "NoDiscard" in res.name:
                        sub_discarded = False

                    w = max(1, tex.width >> mip)
                    h = max(1, tex.height >> mip)

                    if not sub_discarded or "DiscardRect" in res.name:
                        # if not discarded, or we only discarded a rect, check a few locations in the corners and ensure
                        # that we don't see our pattern colours.
                        for (x, y) in [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (1, 1), (2, 2), (3, 3),
                                       (4, 4), (w - 1, h - 1), (w - 2, h - 2), (w - 1, 0), (w - 1, 1), (w - 1, 2),
                                       (0, h - 1), (1, h - 1), (2, h - 1), (3, h - 2)]:
                            # underflow can happen with 1D textures or tiny mips
                            if x < 0:
                                x = 0
                            if y < 0:
                                y = 0

                            if gl and h > 1:
                                y = h - 1 - y

                            picked: rd.PixelValue = self.controller.PickPixel(id, x, y, sub, rd.CompType.Typeless)

                            if self.check_val(picked, minval, fmt) or self.check_val(picked, maxval, fmt):
                                raise rdtest.TestFailureException(
                                    '{} has unexpected value at {},{}: {}'.format(name, x, y,
                                                                                  picked.floatValue))

                    if sub_discarded:
                        seen = [False, False]
                        # Check that pixels inside the rect only show our pattern colours (we don't check the actual
                        # pattern)
                        for (x, y) in [(50, 50), (51, 50), (52, 50), (53, 50), (54, 50), (55, 50), (51, 52), (120, 123),
                                       (124, 124), (119, 122), (119, 124), (122, 124), (70, 70), (80, 85), (73, 124),
                                       (123, 60)]:
                            # overflow can happen with 1D textures or tiny mips
                            if x >= w:
                                x = w-1
                            if y >= h:
                                y = h-1

                            if gl and h > 1:
                                y = h - 1 - y

                            picked: rd.PixelValue = self.controller.PickPixel(id, x, y, sub, rd.CompType.Typeless)

                            is_min = self.check_val(picked, minval, fmt)
                            is_max = self.check_val(picked, maxval, fmt)

                            if not is_min and not is_max:
                                raise rdtest.TestFailureException(
                                    '{} has unexpected value at {},{}: {}'.format(name, x, y,
                                                                                  picked.floatValue))

                            if is_min:
                                seen[0] = True
                            if is_max:
                                seen[1] = True

                        # Do an additional checks outside the rect
                        if "DiscardRect" not in res.name:
                            for (x, y) in [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (1, 1), (2, 2), (3, 3),
                                           (4, 4), (w - 1, h - 1), (w - 2, h - 2), (w - 1, 0), (w - 1, 1), (w - 1, 2),
                                           (0, h - 1), (1, h - 1), (2, h - 1), (3, h - 2)]:
                                # underflow can happen with 1D textures or tiny mips
                                if x < 0:
                                    x = 0
                                if y < 0:
                                    y = 0

                                if gl and h > 1:
                                    y = h - 1 - y

                                picked: rd.PixelValue = self.controller.PickPixel(id, x, y, sub, rd.CompType.Typeless)

                                is_min = self.check_val(picked, minval, fmt)
                                is_max = self.check_val(picked, maxval, fmt)

                                if not is_min and not is_max:
                                    raise rdtest.TestFailureException(
                                        '{} has unexpected value at {},{}: {}'.format(name, x, y,
                                                                                      picked.floatValue))

                                if is_min:
                                    seen[0] = True
                                if is_max:
                                    seen[1] = True

                        # We also expect to have seen both colours. That means if we only saw black for example then we
                        # fail
                        if not seen[0] or not seen[1]:
                            raise rdtest.TestFailureException('{} doesn\'t contain expected pattern'.format(name))

        rdtest.log.success('{} is OK {} discarding'.format(name, "after" if discarded else "before"))

    def check_textures(self):
        action = self.find_action("TestStart")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, True)

        for tex in self.controller.GetTextures():
            tex: rd.TextureDescription
            res: rd.ResourceDescription = self.get_resource(tex.resourceId)

            if "Discard" in res.name:
                self.check_texture(tex.resourceId, False)

        action = self.find_action("TestEnd")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, True)

        for tex in self.controller.GetTextures():
            tex: rd.TextureDescription
            res: rd.ResourceDescription = self.get_resource(tex.resourceId)

            if "Discard" in res.name:
                self.check_texture(tex.resourceId, True)