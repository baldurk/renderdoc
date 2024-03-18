import renderdoc as rd
import rdtest
from typing import List, Tuple
import time
import os


def srgb2linear(f):
    if f <= 0.04045:
        return f / 12.92
    else:
        return ((f + 0.055) / 1.055) ** 2.4


def linear2srgb(f):
    if f <= 0.0031308:
        return f * 12.92
    else:
        return 1.055 * (f ** (1 / 2.4)) - 0.055


# Not a real test, re-used by API-specific tests
class Texture_Zoo():
    def __init__(self):
        self.proxied = False
        self.fake_msaa = False
        self.textures = {}
        self.filename = ''
        self.textures = {}
        self.controller = None
        self.controller: rd.ReplayController
        self.out: rd.ReplayOutput
        self.out = None
        self.pipeType = rd.GraphicsAPI.D3D11
        self.opengl_mode = False
        self.d3d_mode = False

    def sub(self, mip: int, slice: int, sample: int):
        if self.fake_msaa:
            return rd.Subresource(mip, slice * 2 + sample, 0)
        else:
            return rd.Subresource(mip, slice, sample)

    def pick(self, tex: rd.ResourceId, x: int, y: int, sub: rd.Subresource, typeCast: rd.CompType):
        if self.opengl_mode:
            y = max(1, self.textures[tex].height >> sub.mip) - 1 - y

        return self.controller.PickPixel(tex, x, y, sub, typeCast)

    TEST_CAPTURE = 0
    TEST_DDS = 1
    TEST_PNG = 2

    def check_test(self, fmt_name: str, name: str, test_mode: int):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        image_view = (test_mode != Texture_Zoo.TEST_CAPTURE)

        if image_view:
            desc = pipe.GetOutputTargets()[0]
        else:
            desc = pipe.GetReadOnlyResources(rd.ShaderStage.Pixel)[0].descriptor

        tex_id = desc.resource

        texs = self.controller.GetTextures()
        for t in texs:
            self.textures[t.resourceId] = t

        tex: rd.TextureDescription = self.textures[tex_id]

        testCompType = desc.format.compType
        if testCompType == rd.CompType.Typeless:
            testCompType = tex.format.compType

        pickCompType = testCompType

        if tex.format.type == rd.ResourceFormatType.S8:
            pickCompType = rd.CompType.UInt

        # When not running proxied, save non-typecasted textures to disk
        if not image_view and not self.proxied and (tex.format.compType == testCompType or
                                                    tex.format.type == rd.ResourceFormatType.D24S8 or
                                                    tex.format.type == rd.ResourceFormatType.D32S8):
            save_data = rd.TextureSave()
            save_data.resourceId = tex_id
            save_data.destType = rd.FileType.DDS
            save_data.sample.mapToArray = True

            self.textures[self.filename] = tex

            path = rdtest.get_tmp_path(self.filename + '.dds')

            success: bool = self.controller.SaveTexture(save_data, path)

            if not success:
                if self.d3d_mode:
                    raise rdtest.TestFailureException("Couldn't save DDS to {} on D3D.".format(self.filename))

                try:
                    os.remove(path)
                except Exception:
                    pass

            save_data.destType = rd.FileType.PNG
            save_data.typeCast = testCompType
            save_data.slice.sliceIndex = 0
            save_data.sample.sampleIndex = 0
            path = path.replace('.dds', '.png')

            if testCompType == rd.CompType.UInt:
                save_data.comp.blackPoint = 0.0
                save_data.comp.whitePoint = 255.0
            elif testCompType == rd.CompType.SInt:
                save_data.comp.blackPoint = -255.0
                save_data.comp.whitePoint = 0.0
            elif testCompType == rd.CompType.SNorm:
                save_data.comp.blackPoint = -1.0
                save_data.comp.whitePoint = 0.0

            success: bool = self.controller.SaveTexture(save_data, path)

            if not success:
                try:
                    os.remove(path)
                except Exception:
                    pass

        # When viewing PNGs only compare the components that the original texture had
        if test_mode == Texture_Zoo.TEST_PNG:
            tex.format = self.textures[self.filename].format
            testCompType = tex.format.compType
            tex.msSamp = 1
            tex.arraysize = 1
            tex.depth = 1
            self.fake_msaa = 'MSAA' in name

            orig_format = self.textures[self.filename].format
            if orig_format.type == rd.ResourceFormatType.S8:
                pickCompType = rd.CompType.UInt
        elif test_mode == Texture_Zoo.TEST_DDS:
            tex_format = tex.format
            orig_format = self.textures[self.filename].format

            bgra = tex_format.BGRAOrder()
            ct = tex_format.compType

            # handle some formats that DDS can't represent
            # DDS only supports BGRA order for unorm/srgb, for all other formats compare without BGRA order
            if orig_format.BGRAOrder() and tex_format.compType != rd.CompType.UNorm and tex_format.compType != rd.CompType.UNormSRGB:
                tex_format.SetBGRAOrder(orig_format.BGRAOrder())

            # DDS only supports SRGB for four-component formats
            if orig_format.compType == rd.CompType.UNormSRGB and tex_format.compCount < 4:
                tex_format.compType = orig_format.compType

            # S8 will be loaded up as R8_UINT
            if orig_format.type == rd.ResourceFormatType.S8:
                tex_format = orig_format
                pickCompType = rd.CompType.UInt

            if tex_format.type == rd.ResourceFormatType.D24S8 or tex_format.type == rd.ResourceFormatType.D16S8 or tex_format.type == rd.ResourceFormatType.D32S8:
                if tex_format.type != orig_format.type:
                    raise rdtest.TestFailureException(
                        "Format on load from dds {} is different to format expected {}".format(tex_format.Name(),
                                                                                               orig_format.Name()))
            elif tex_format.Name() != orig_format.Name():
                raise rdtest.TestFailureException(
                    "Format on load from dds {} is different to format expected {}".format(tex_format.Name(),
                                                                                           orig_format.Name()))

            tex_format.SetBGRAOrder(bgra)
            tex_format.compType = ct

            tex.arraysize = self.textures[self.filename].arraysize
            tex.msSamp = self.textures[self.filename].msSamp
            self.fake_msaa = 'MSAA' in name

        comp_count = tex.format.compCount

        # HACK: We don't properly support BGRX, so just drop the alpha channel. We can't set this to compCount = 3
        # internally because that's a 24-bit format with no padding...
        if 'B8G8R8X8' in fmt_name:
            comp_count = 3

        # Completely ignore the alpha for BC1, our encoder doesn't pay attention to it
        if tex.format.type == rd.ResourceFormatType.BC1:
            comp_count = 3

        # Calculate format-appropriate epsilon
        eps_significand = 1.0
        # Account for the sRGB curve by more generous epsilon

        if testCompType == rd.CompType.UNormSRGB:
            eps_significand = 2.5
        # Similarly SNorm essentially loses a bit of accuracy due to us only using negative values
        elif testCompType == rd.CompType.SNorm:
            eps_significand = 2.0

        if tex.format.type == rd.ResourceFormatType.R4G4B4A4 or tex.format.type == rd.ResourceFormatType.R4G4:
            eps = (eps_significand / 15.0)
        elif rd.ResourceFormatType.BC1 <= tex.format.type <= rd.ResourceFormatType.BC3:
            eps = (eps_significand / 15.0)  # 4-bit precision in some channels
        elif tex.format.type == rd.ResourceFormatType.R5G5B5A1 or tex.format.type == rd.ResourceFormatType.R5G6B5:
            eps = (eps_significand / 31.0)
        elif tex.format.type == rd.ResourceFormatType.R11G11B10:
            eps = (eps_significand / 31.0)  # 5-bit mantissa in blue
        elif tex.format.type == rd.ResourceFormatType.R9G9B9E5:
            eps = (eps_significand / 63.0)  # we have 9 bits of data, but might lose 2-3 due to shared exponent
        elif tex.format.type == rd.ResourceFormatType.BC6 and tex.format.compType == rd.CompType.SNorm:
            eps = (eps_significand / 63.0)  # Lose a bit worth of precision for the signed version
        elif rd.ResourceFormatType.BC4 <= tex.format.type <= rd.ResourceFormatType.BC7:
            eps = (eps_significand / 127.0)
        elif tex.format.compByteWidth == 1:
            eps = (eps_significand / 255.0)
        elif testCompType == rd.CompType.Depth and tex.format.compCount == 2 or tex.format.type == rd.ResourceFormatType.S8:
            eps = (eps_significand / 255.0)  # stencil is only 8-bit
        elif tex.format.type == rd.ResourceFormatType.A8:
            eps = (eps_significand / 255.0)
        elif tex.format.type == rd.ResourceFormatType.R10G10B10A2:
            eps = (eps_significand / 1023.0)
        else:
            # half-floats have 11-bit mantissa. This epsilon is tight enough that we can be sure
            # any remaining errors are implementation inaccuracy and not our bug
            eps = (eps_significand / 2047.0)

        if test_mode == Texture_Zoo.TEST_PNG:
            eps = max(eps, (2.5 / 255.0))

        for mp in range(tex.mips):
            for sl in range(max(tex.arraysize, max(1, tex.depth >> mp))):
                z = 0
                if tex.depth > 1:
                    z = sl

                for sm in range(tex.msSamp):
                    cur_sub = self.sub(mp, sl, sm)

                    tex_display = rd.TextureDisplay()
                    tex_display.resourceId = tex_id
                    tex_display.subresource = cur_sub
                    tex_display.flipY = self.opengl_mode
                    tex_display.typeCast = testCompType
                    tex_display.alpha = False
                    tex_display.scale = 1.0 / float(1<<mp)
                    tex_display.backgroundColor = rd.FloatVector(0.0, 0.0, 0.0, 1.0)

                    if testCompType == rd.CompType.UInt:
                        tex_display.rangeMin = 0.0
                        tex_display.rangeMax = 255.0
                    elif testCompType == rd.CompType.SInt:
                        tex_display.rangeMin = -255.0
                        tex_display.rangeMax = 0.0
                    elif testCompType == rd.CompType.SNorm:
                        tex_display.rangeMin = -1.0
                        tex_display.rangeMax = 0.0

                    self.out.SetTextureDisplay(tex_display)

                    self.out.Display()

                    pixels: bytes = self.out.ReadbackOutputTexture()
                    dim = self.out.GetDimensions()

                    stencilpixels = None

                    # Grab stencil separately
                    if tex.format.type == rd.ResourceFormatType.D16S8 or tex.format.type == rd.ResourceFormatType.D24S8 or tex.format.type == rd.ResourceFormatType.D32S8:
                        tex_display.red = False
                        tex_display.green = True
                        tex_display.blue = False
                        tex_display.alpha = False

                        self.out.SetTextureDisplay(tex_display)
                        self.out.Display()

                        stencilpixels: bytes = self.out.ReadbackOutputTexture()

                    # Grab alpha if needed (since the readback output is RGB only)
                    if comp_count == 4 or tex.format.type == rd.ResourceFormatType.A8:
                        tex_display.red = False
                        tex_display.green = False
                        tex_display.blue = False
                        tex_display.alpha = True

                        self.out.SetTextureDisplay(tex_display)
                        self.out.Display()

                        alphapixels: bytes = self.out.ReadbackOutputTexture()

                    all_good = True

                    for x in range(max(1, tex.width >> mp)):
                        if not all_good:
                            break
                        for y in range(max(1, tex.height >> mp)):
                            expected = self.get_expected_value(comp_count, testCompType, cur_sub, test_mode, tex, x, y, z)

                            # for this test to work the expected values have to be within the range we selected for
                            # display above
                            if test_mode != Texture_Zoo.TEST_PNG:
                                for i in expected:
                                    if i < tex_display.rangeMin or tex_display.rangeMax < i:
                                        raise rdtest.TestFailureException("expected value {} is outside of texture display range! {} - {}".format(i, tex_display.rangeMin, tex_display.rangeMax))

                            # convert the expected values to range-adapted values
                            for i in range(len(expected)):
                                expected[i] = (expected[i] - tex_display.rangeMin) / (
                                            tex_display.rangeMax - tex_display.rangeMin)

                            # get the bytes from the displayed pixel
                            offs = y * dim[0] * 3 + x * 3
                            displayed = [int(a) for a in pixels[offs:offs + comp_count]]
                            if comp_count == 4:
                                del displayed[3]
                                displayed.append(int(alphapixels[offs]))
                            if stencilpixels is not None:
                                del displayed[1:]
                                displayed.append(int(stencilpixels[offs]))
                            if tex.format.type == rd.ResourceFormatType.A8:
                                displayed = [int(alphapixels[offs])]

                            # normalise the displayed values
                            for i in range(len(displayed)):
                                displayed[i] = float(displayed[i]) / 255.0

                            # For SRGB textures match linear picked values. We do this for alpha too since it's rendered
                            # via RGB
                            if testCompType == rd.CompType.UNormSRGB:
                                displayed[0:4] = [srgb2linear(x) for x in displayed[0:4]]

                            # alpha channel in 10:10:10:2 has extremely low precision, and the ULP requirements mean
                            # we basically can't trust anything between 0 and 1 on float formats. Just round in that
                            # case as it still lets us differentiate between alpha 0.0-0.5 and 0.5-1.0
                            if tex.format.type == rd.ResourceFormatType.R10G10B10A2 and testCompType != rd.CompType.UInt:
                                displayed[3] = round(displayed[3]) * 1.0

                            # Handle 1-bit alpha
                            if tex.format.type == rd.ResourceFormatType.R5G5B5A1:
                                displayed[3] = 1.0 if displayed[3] >= 0.5 else 0.0

                            # Need an additional 1/255 epsilon to account for us going via a 8-bit backbuffer for display
                            if not rdtest.value_compare(displayed, expected, 1.0/255.0 + eps):
                                #rdtest.log.print(
                                #    "Quick-checking ({},{}) of slice {}, mip {}, sample {} of {} {} got {}. Expected {}.".format(
                                #        x, y, sl, mp, sm, name, fmt_name, displayed, expected) +
                                #    "Falling back to pixel picking tests.")
                                # Currently this seems to fail in some proxy scenarios with sRGB, but since it's not a
                                # real error we just silently swallow it
                                all_good = False
                                break

                    if all_good:
                        continue

                    for x in range(max(1, tex.width >> mp)):
                        for y in range(max(1, tex.height >> mp)):
                            expected = self.get_expected_value(comp_count, testCompType, cur_sub, test_mode, tex, x, y, z)
                            picked = self.get_picked_pixel_value(comp_count, pickCompType, cur_sub, tex, tex_id, x, y)

                            if mp == 0 and sl == 0 and sm == 0 and x == 0 and y == 0:
                                pass

                            if not rdtest.value_compare(picked, expected, eps):
                                raise rdtest.TestFailureException(
                                    "At ({},{}) of slice {}, mip {}, sample {} of {} {} got {}. Expected {}".format(
                                        x, y, sl, mp, sm, name, fmt_name, picked, expected))

        if not image_view:
            output_tex = pipe.GetOutputTargets()[0].resource

            # in the test captures pick the output texture, it should be identical to the
            # (0,0) pixel in slice 0, mip 0, sample 0
            view: rd.Viewport = pipe.GetViewport(0)

            val: rd.PixelValue = self.pick(pipe.GetOutputTargets()[0].resource, int(view.x + view.width / 2),
                                           int(view.y + view.height / 2), rd.Subresource(), rd.CompType.Typeless)

            picked = list(val.floatValue)

            # A8 picked values come out in alpha, but we want to compare against the single channel
            if tex.format.type == rd.ResourceFormatType.A8:
                picked[0] = picked[3]

            # Clamp to number of components in the texture
            picked = picked[0:comp_count]

            value0 = self.get_picked_pixel_value(comp_count, pickCompType, rd.Subresource(), tex, tex_id, 0, 0)

            # Up-convert any non-float expected values to floats
            value0 = [float(x) for x in value0]

            # For depth/stencil images, one of either depth or stencil should match
            if testCompType == rd.CompType.Depth and len(value0) == 2:
                if picked[0] == 0.0:
                    value0[0] = 0.0
                    # normalise stencil value if it isn't already
                    if picked[1] > 1.0:
                        picked[1] /= 255.0
                elif picked[0] > 1.0:
                    # un-normalised stencil being rendered in red, match against our stencil expectation
                    picked[0] /= 255.0
                    value0[0] = value0[1]
                    value0[1] = 0.0
                else:
                    if picked[1] == 0.0:
                        value0[1] = 0.0
                    if picked[1] > 1.0:
                        picked[1] /= 255.0
            elif tex.format.type == rd.ResourceFormatType.S8:
                picked[0] /= 255.0

            if not rdtest.value_compare(picked, value0, eps):
                raise rdtest.TestFailureException(
                    "In {} {} Top-left pixel as rendered is {}. Expected {}".format(name, fmt_name, picked, value0))

    def get_expected_value(self, comp_count: int, comp_type: rd.CompType, cur_sub: rd.Subresource, test_mode: int,
                           tex: rd.TextureDescription, x: int, y: int, z: int):
        mp, sl, sm = cur_sub.mip, cur_sub.slice, cur_sub.sample

        if self.fake_msaa:
            sm = sl % 2
            sl = sl // 2

        # each 3D slice cycles the x. This only affects the primary diagonal
        offs_x = (x + z) % max(1, tex.width >> mp)

        # The diagonal inverts the colors
        inverted = (offs_x != y)

        # every other slice adds a coarse checkerboard pattern of inversion
        if tex.arraysize > 1 and ((sl % 2) == 1) and ((int(x / 2) % 2) != (int(y / 2) % 2)):
            inverted = not inverted

        if comp_type == rd.CompType.UInt or comp_type == rd.CompType.SInt or tex.format.type == rd.ResourceFormatType.S8:
            expected = [10.0, 40.0, 70.0, 100.0]

            if inverted:
                expected = list(reversed(expected))

            expected = [c + 10.0 * (sm + mp) for c in expected]

            # Normalise stencil value
            if tex.format.type == rd.ResourceFormatType.S8:
                expected[0] = expected[0] / 255.0
        elif (tex.format.type == rd.ResourceFormatType.D16S8 or
              tex.format.type == rd.ResourceFormatType.D24S8 or
              tex.format.type == rd.ResourceFormatType.D32S8):
            # depth/stencil is a bit special
            expected = [0.1, 10.0, 100.0, 0.85]

            if inverted:
                expected = list(reversed(expected))

            expected[0] += 0.075 * (sm + mp)
            expected[1] += 10.0 * (sm + mp)

            # Normalise stencil value
            expected[1] = expected[1] / 255.0
        else:
            expected = [0.1, 0.35, 0.6, 0.85]

            if inverted:
                expected = list(reversed(expected))

            expected = [c + 0.075 * (sm + mp) for c in expected]

        # SNorm/SInt is negative
        if comp_type == rd.CompType.SNorm or comp_type == rd.CompType.SInt:
            expected = [-c for c in expected]

            if test_mode == Texture_Zoo.TEST_PNG:
                if comp_type == rd.CompType.SInt:
                    expected = [x/255.0 for x in expected]
                expected = [1.0 + x for x in expected]
                expected[0:3] = [srgb2linear(x) for x in expected[0:3]]

        # BGRA textures have a swizzle applied
        if tex.format.BGRAOrder():
            expected[0:3] = reversed(expected[0:3])

        # alpha channel in 10:10:10:2 has extremely low precision, and the ULP requirements mean
        # we basically can't trust anything between 0 and 1 on float formats. Just round in that
        # case as it still lets us differentiate between alpha 0.0-0.5 and 0.5-1.0
        if tex.format.type == rd.ResourceFormatType.R10G10B10A2:
            if comp_type == rd.CompType.UInt:
                expected[3] = min(3.0, expected[3])
            else:
                expected[3] = round(expected[3]) * 1.0

        # Handle 1-bit alpha
        if tex.format.type == rd.ResourceFormatType.R5G5B5A1:
            expected[3] = 1.0 if expected[3] >= 0.5 else 0.0

        # Clamp to number of components in the texture
        expected = expected[0:comp_count]

        # For SRGB textures picked values will come out as linear
        if comp_type == rd.CompType.UNormSRGB and tex.format.type != rd.ResourceFormatType.A8:
            expected[0:3] = [srgb2linear(x) for x in expected[0:3]]

        return expected

    def get_picked_pixel_value(self, comp_count, comp_type, cur_sub, tex, tex_id, x, y):
        picked_combo: rd.PixelValue = self.pick(tex_id, x, y, cur_sub, comp_type)

        if comp_type == rd.CompType.SInt:
            picked = [float(a) for a in picked_combo.intValue]
        elif comp_type == rd.CompType.UInt:
            picked = [float(a) for a in picked_combo.uintValue]
        else:
            picked = list(picked_combo.floatValue)

        # alpha channel in 10:10:10:2 has extremely low precision, and the ULP requirements mean
        # we basically can't trust anything between 0 and 1 on float formats. Just round in that
        # case as it still lets us differentiate between alpha 0.0-0.5 and 0.5-1.0
        if tex.format.type == rd.ResourceFormatType.R10G10B10A2 and comp_type != rd.CompType.UInt:
            picked[3] = round(picked[3]) * 1.0

        # Handle 1-bit alpha
        if tex.format.type == rd.ResourceFormatType.R5G5B5A1:
            picked[3] = 1.0 if picked[3] >= 0.5 else 0.0

        # A8 picked values come out in alpha, but we want to compare against the single channel
        if tex.format.type == rd.ResourceFormatType.A8:
            picked[0] = picked[3]

        # Normalise stencil values
        if tex.format.type == rd.ResourceFormatType.S8:
            picked[0] /= 255.0

        # Clamp to number of components in the texture
        picked = picked[0:comp_count]

        return picked

    def check_capture_with_controller(self, proxy_api: str):
        self.controller: rd.ReplayController
        any_failed = False

        if proxy_api != '':
            rdtest.log.print('Running with {} local proxy'.format(proxy_api))
            self.proxied = True
        else:
            rdtest.log.print('Running on direct replay')
            self.proxied = False

        self.out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                                 rd.ReplayOutputType.Texture)

        for d in self.controller.GetRootActions():
            if 'slice tests' in d.customName:
                for sub in d.children:
                    if sub.flags & rd.ActionFlags.Drawcall:
                        self.controller.SetFrameEvent(sub.eventId, True)

                        pipe = self.controller.GetPipelineState()

                        tex_id = pipe.GetReadOnlyResources(rd.ShaderStage.Pixel)[0].descriptor.resource

                        for mip in [0, 1]:
                            for sl in [16, 17, 18]:
                                expected = [0.0, 0.0, 1.0, 1.0]
                                if sl == 17:
                                    expected = [0.0, 1.0, 0.0, 1.0]

                                cur_sub = rd.Subresource(mip, sl)
                                comp_type = rd.CompType.Typeless

                                # test that pixel picking sees the right things
                                picked = self.controller.PickPixel(tex_id, 15, 15, cur_sub, comp_type)

                                if not rdtest.value_compare(picked.floatValue, expected):
                                    raise rdtest.TestFailureException(
                                        "Expected to pick {} at slice {} mip {}, got {}"
                                        .format(expected, sl, mip, picked.floatValue))

                                rdtest.log.success('Picked pixel is correct at slice {} mip {}'.format(sl, mip))

                                # Render output texture a three scales - below 100%, 100%, above 100%
                                tex_display = rd.TextureDisplay()
                                tex_display.resourceId = tex_id
                                tex_display.subresource = cur_sub
                                tex_display.typeCast = comp_type

                                # convert the unorm values to byte values for comparison
                                expected = [int(a * 255) for a in expected[0:3]]

                                for scale in [0.9, 1.0, 1.1]:
                                    tex_display.scale = scale
                                    self.out.SetTextureDisplay(tex_display)
                                    self.out.Display()
                                    pixels: bytes = self.out.ReadbackOutputTexture()

                                    actual = [int(a) for a in pixels[0:3]]

                                    if not rdtest.value_compare(actual, expected):
                                        raise rdtest.TestFailureException(
                                            "Expected to display {} at slice {} mip {} scale {}%, got {}"
                                            .format(expected, sl, mip, int(scale * 100), actual))

                                    rdtest.log.success('Displayed pixel is correct at scale {}% in slice {} mip {}'
                                                       .format(int(scale * 100), sl, mip))
                    elif sub.flags & rd.ActionFlags.SetMarker:
                        rdtest.log.print('Checking {} for slice display'.format(sub.customName))

                continue

            # Check each region for the tests within
            if d.flags & rd.ActionFlags.PushMarker:
                name = ''
                tests_run = 0

                failed = False

                # Iterate over actions in this region
                for sub in d.children:
                    sub: rd.ActionDescription

                    if sub.flags & rd.ActionFlags.SetMarker:
                        name = sub.customName

                    # Check this action
                    if sub.flags & rd.ActionFlags.Drawcall:
                        tests_run = tests_run + 1
                        try:
                            # Set this event as current
                            self.controller.SetFrameEvent(sub.eventId, True)

                            self.filename = (d.customName + '@' + name).replace('->', '_')

                            self.check_test(d.customName, name, Texture_Zoo.TEST_CAPTURE)
                        except rdtest.TestFailureException as ex:
                            failed = any_failed = True
                            rdtest.log.error(str(ex))

                if not failed:
                    rdtest.log.success("All {} texture tests for {} are OK".format(tests_run, d.customName))

        self.out.Shutdown()
        self.out = None

        if not any_failed:
            if proxy_api != '':
                rdtest.log.success(
                    'All textures are OK with {} as local proxy'.format(proxy_api))
            else:
                rdtest.log.success("All textures are OK on direct replay")
        else:
            raise rdtest.TestFailureException("Some tests were not as expected")

    def check_capture(self, capture_filename: str, controller: rd.ReplayController):
        self.controller = controller

        self.pipeType = self.controller.GetAPIProperties().pipelineType
        self.opengl_mode = (self.controller.GetAPIProperties().pipelineType == rd.GraphicsAPI.OpenGL)
        self.d3d_mode = rd.IsD3D(self.controller.GetAPIProperties().pipelineType)

        failed = False

        rdtest.log.begin_section("Local test")
        try:
            # First check with the local controller
            self.check_capture_with_controller('')
        except rdtest.TestFailureException as ex:
            rdtest.log.error(str(ex))
            failed = True
        rdtest.log.end_section("Local test")

        # Now shut it down
        self.controller.Shutdown()
        self.controller = None

        # Launch a remote server
        rdtest.launch_remote_server()

        # Wait for it to start
        time.sleep(0.5)

        ret: Tuple[rd.ResultDetails, rd.RemoteServer] = rd.CreateRemoteServerConnection('localhost')
        result, remote = ret

        if result != rd.ResultCode.Succeeded:
            time.sleep(2)

            ret: Tuple[rd.ResultDetails, rd.RemoteServer] = rd.CreateRemoteServerConnection('localhost')
            result, remote = ret

        if result != rd.ResultCode.Succeeded:
            raise rdtest.TestFailureException("Couldn't connect to remote server: {}".format(str(result)))

        proxies = remote.LocalProxies()

        try:
            # Try D3D11 and GL as proxies, D3D12/Vulkan technically don't have proxying implemented even though they
            # will be listed in proxies
            for api in ['D3D11', 'OpenGL']:
                if api not in proxies:
                    continue

                rdtest.log.begin_section("{} proxy".format(api))
                try:
                    ret: Tuple[rd.ResultDetails, rd.ReplayController] = remote.OpenCapture(proxies.index(api),
                                                                                        capture_filename,
                                                                                        rd.ReplayOptions(), None)
                    result, self.controller = ret

                    # Now check with the proxy
                    self.check_capture_with_controller(api)
                except ValueError:
                    continue
                except rdtest.TestFailureException as ex:
                    rdtest.log.error(str(ex))
                    failed = True
                finally:
                    rdtest.log.end_section("{} proxy".format(api))
                    remote.CloseCapture(self.controller)
                    self.controller = None
        finally:
            remote.ShutdownServerAndConnection()

        # Now iterate over all the temp images saved out, load them as captures, and check the texture.
        dir_path = rdtest.get_tmp_path('')

        was_opengl = self.opengl_mode

        # We iterate in filename order, so that dds files get opened before png files.
        rdtest.log.begin_section("loading files")
        for file in os.scandir(dir_path):
            if '.dds' not in file.name and '.png' not in file.name:
                continue

            cap = rd.OpenCaptureFile()
            result = cap.OpenFile(file.path, 'rdc', None)

            if result != rd.ResultCode.Succeeded:
                rdtest.log.error("Couldn't open {}".format(file.name))
                failed = True
                continue

            ret: Tuple[rd.ResultDetails, rd.ReplayController] = cap.OpenCapture(rd.ReplayOptions(), None)
            result, self.controller = ret

            if result != rd.ResultCode.Succeeded:
                rdtest.log.error("Couldn't open {}".format(file.name))
                failed = True
                continue

            self.filename = file.name.replace('.dds', '').replace('.png', '')

            [a, b] = file.name.replace('.dds', ' (DDS)').replace('.png', ' (PNG)').split('@')

            self.controller.SetFrameEvent(self.controller.GetRootActions()[0].eventId, True)

            try:
                self.opengl_mode = False
                fmt: rd.ResourceFormat = self.controller.GetTextures()[0].format

                is_compressed = (rd.ResourceFormatType.BC1 <= fmt.type <= rd.ResourceFormatType.BC7 or
                                 fmt.type == rd.ResourceFormatType.EAC or fmt.type == rd.ResourceFormatType.ETC2 or
                                 fmt.type == rd.ResourceFormatType.ASTC or fmt.type == rd.ResourceFormatType.PVRTC)

                # OpenGL saves all non-compressed images to disk with a flip, since that's the expected order for
                # most formats. The effect of this is that we should apply the opengl_mode workaround for all files
                # *except* compressed textures
                if was_opengl and not is_compressed:
                    self.opengl_mode = True

                self.out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

                rdtest.log.print("Checking {}".format(file.name))

                self.check_test(a, b, Texture_Zoo.TEST_DDS if '.dds' in file.name else Texture_Zoo.TEST_PNG)

                self.out.Shutdown()
                self.out = None

                rdtest.log.success("{} loaded with the correct data".format(file.name))
            except rdtest.TestFailureException as ex:
                rdtest.log.error(str(ex))
                failed = True

            self.controller.Shutdown()
            self.controller = None

        rdtest.log.end_section("loading files")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")
