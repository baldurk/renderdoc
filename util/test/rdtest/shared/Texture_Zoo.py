import renderdoc as rd
import rdtest
from typing import List, Tuple
import time
import os


# Not a real test, re-used by API-specific tests
class Texture_Zoo():
    def __init__(self):
        self.proxied = False
        self.fake_msaa = False
        self.textures = {}
        self.filename = ''
        self.textures = {}
        self.controller: rd.ReplayController
        self.controller = None
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
            bound_res: rd.BoundResource = pipe.GetOutputTargets()[0]
        else:
            bound_res: rd.BoundResource = pipe.GetReadOnlyResources(rd.ShaderStage.Pixel)[0].resources[0]

        texs = self.controller.GetTextures()
        for t in texs:
            self.textures[t.resourceId] = t

        tex_id: rd.ResourceId = bound_res.resourceId
        tex: rd.TextureDescription = self.textures[tex_id]

        comp_type: rd.CompType = tex.format.compType
        if bound_res.typeCast != rd.CompType.Typeless:
            comp_type = bound_res.typeCast

        # When not running proxied, save non-typecasted textures to disk
        if not image_view and not self.proxied and (tex.format.compType == comp_type or
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
            save_data.slice.sliceIndex = 0
            save_data.sample.sampleIndex = 0
            path = path.replace('.dds', '.png')

            if comp_type == rd.CompType.UInt:
                save_data.comp.blackPoint = 0.0
                save_data.comp.whitePoint = 255.0
            elif comp_type == rd.CompType.SInt:
                save_data.comp.blackPoint = -255.0
                save_data.comp.whitePoint = 0.0
            elif comp_type == rd.CompType.SNorm:
                save_data.comp.blackPoint = -1.0
                save_data.comp.whitePoint = 0.0

            success: bool = self.controller.SaveTexture(save_data, path)

            if not success:
                try:
                    os.remove(path)
                except Exception:
                    pass

        value0 = []

        comp_count = tex.format.compCount

        # When viewing PNGs only compare the components that the original texture had
        if test_mode == Texture_Zoo.TEST_PNG:
            comp_count = self.textures[self.filename]
            tex.msSamp = 0
            tex.arraysize = 1
            tex.depth = 1
            self.fake_msaa = 'MSAA' in name
        elif test_mode == Texture_Zoo.TEST_DDS:
            tex.arraysize = self.textures[self.filename].arraysize
            tex.msSamp = self.textures[self.filename].msSamp
            self.fake_msaa = 'MSAA' in name

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

        if comp_type == rd.CompType.UNormSRGB:
            eps_significand = 2.5
        # Similarly SNorm essentially loses a bit of accuracy due to us only using negative values
        elif comp_type == rd.CompType.SNorm:
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
        elif comp_type == rd.CompType.Depth and tex.format.compCount == 2:
            eps = (eps_significand / 255.0)  # stencil is only 8-bit
        elif tex.format.type == rd.ResourceFormatType.A8:
            eps = (eps_significand / 255.0)
        elif tex.format.type == rd.ResourceFormatType.R10G10B10A2:
            eps = (eps_significand / 1023.0)
        else:
            # half-floats have 11-bit mantissa. This epsilon is tight enough that we can be sure
            # any remaining errors are implementation inaccuracy and not our bug
            eps = (eps_significand / 2047.0)

        for mp in range(tex.mips):
            for sl in range(max(tex.arraysize, max(1, tex.depth >> mp))):
                z = 0
                if tex.depth > 1:
                    z = sl

                for sm in range(tex.msSamp):
                    for x in range(max(1, tex.width >> mp)):
                        for y in range(max(1, tex.height >> mp)):
                            picked: rd.PixelValue = self.pick(tex_id, x, y, self.sub(mp, sl, sm), comp_type)

                            # each 3D slice cycles the x. This only affects the primary diagonal
                            offs_x = (x + z) % max(1, tex.width >> mp)

                            # The diagonal inverts the colors
                            inverted = (offs_x != y)

                            # second slice adds a coarse checkerboard pattern of inversion
                            if tex.arraysize > 1 and sl == 1 and ((int(x / 2) % 2) != (int(y / 2) % 2)):
                                inverted = not inverted

                            if comp_type == rd.CompType.UInt or comp_type == rd.CompType.SInt:
                                expected = [10, 40, 70, 100]

                                if inverted:
                                    expected = list(reversed(expected))

                                expected = [c + 10 * (sm + mp) for c in expected]

                                if comp_type == rd.CompType.SInt:
                                    picked = picked.intValue
                                else:
                                    picked = picked.uintValue
                            elif (tex.format.type == rd.ResourceFormatType.D16S8 or
                                  tex.format.type == rd.ResourceFormatType.D24S8 or
                                  tex.format.type == rd.ResourceFormatType.D32S8):
                                # depth/stencil is a bit special
                                expected = [0.1, 10, 100, 0.85]

                                if inverted:
                                    expected = list(reversed(expected))

                                expected[0] += 0.075 * (sm + mp)
                                expected[1] += 10 * (sm + mp)

                                # Normalise stencil value
                                expected[1] = expected[1] / 255.0

                                picked = picked.floatValue
                            else:
                                expected = [0.1, 0.35, 0.6, 0.85]

                                if inverted:
                                    expected = list(reversed(expected))

                                expected = [c + 0.075 * (sm + mp) for c in expected]

                                picked = picked.floatValue

                            # SNorm/SInt is negative
                            if comp_type == rd.CompType.SNorm or comp_type == rd.CompType.SInt:
                                expected = [-c for c in expected]

                            # BGRA textures have a swizzle applied
                            if tex.format.BGRAOrder():
                                expected[0:3] = reversed(expected[0:3])

                            # alpha channel in 10:10:10:2 has extremely low precision, and the ULP requirements mean
                            # we basically can't trust anything between 0 and 1 on float formats. Just round in that
                            # case as it still lets us differentiate between alpha 0.0-0.5 and 0.5-1.0
                            if tex.format.type == rd.ResourceFormatType.R10G10B10A2:
                                if comp_type == rd.CompType.UInt:
                                    expected[3] = min(3, expected[3])
                                else:
                                    expected[3] = round(expected[3]) * 1.0
                                    picked[3] = round(picked[3]) * 1.0

                            # Handle 1-bit alpha
                            if tex.format.type == rd.ResourceFormatType.R5G5B5A1:
                                expected[3] = 1.0 if expected[3] >= 0.5 else 0.0
                                picked[3] = 1.0 if picked[3] >= 0.5 else 0.0

                            # A8 picked values come out in alpha, but we want to compare against the single channel
                            if tex.format.type == rd.ResourceFormatType.A8:
                                picked[0] = picked[3]

                            # Clamp to number of components in the texture
                            expected = expected[0:comp_count]
                            picked = picked[0:comp_count]

                            if mp == 0 and sl == 0 and sm == 0 and x == 0 and y == 0:
                                value0 = picked

                            # For SRGB textures picked values will come out as linear
                            def srgb2linear(f):
                                if f <= 0.04045:
                                    return f / 12.92
                                else:
                                    return ((f + 0.055) / 1.055) ** 2.4

                            if comp_type == rd.CompType.UNormSRGB:
                                expected[0:3] = [srgb2linear(x) for x in expected[0:3]]

                            if test_mode == Texture_Zoo.TEST_PNG:
                                orig_comp = self.textures[self.filename].format.compType
                                if orig_comp == rd.CompType.SNorm or orig_comp == rd.CompType.SInt:
                                    expected = [1.0 - x for x in expected]

                            if not rdtest.value_compare(picked, expected, eps):
                                raise rdtest.TestFailureException(
                                    "At ({},{}) of slice {}, mip {}, sample {} of {} {} got {}. Expected {}".format(
                                        x, y, sl, mp, sm, name, fmt_name, picked, expected))

        if not image_view:
            output_tex = pipe.GetOutputTargets()[0].resourceId

            # in the test captures pick the output texture, it should be identical to the
            # (0,0) pixel in slice 0, mip 0, sample 0
            view: rd.Viewport = pipe.GetViewport(0)

            val: rd.PixelValue = self.pick(pipe.GetOutputTargets()[0].resourceId, int(view.x + view.width / 2),
                                           int(view.y + view.height / 2), rd.Subresource(), rd.CompType.Typeless)

            picked = val.floatValue

            # A8 picked values come out in alpha, but we want to compare against the single channel
            if tex.format.type == rd.ResourceFormatType.A8:
                picked[0] = picked[3]

            # Clamp to number of components in the texture
            picked = picked[0:comp_count]

            # Up-convert any non-float expected values to floats
            value0 = [float(x) for x in value0]

            # For depth/stencil images, one of either depth or stencil should match
            if comp_type == rd.CompType.Depth and len(value0) == 2:
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

            if not rdtest.value_compare(picked, value0, eps):
                raise rdtest.TestFailureException(
                    "In {} {} Top-left pixel as rendered is {}. Expected {}".format(name, fmt_name, picked, value0))

    def check_capture_with_controller(self, proxy_api: str):
        any_failed = False

        if proxy_api != '':
            rdtest.log.print('Running with {} local proxy'.format(proxy_api))
            self.proxied = True
        else:
            rdtest.log.print('Running on direct replay')
            self.proxied = False

        for d in self.controller.GetDrawcalls():

            # Check each region for the tests within
            if d.flags & rd.DrawFlags.PushMarker:
                name = ''
                tests_run = 0

                failed = False

                # Iterate over drawcalls in this region
                for sub in d.children:
                    sub: rd.DrawcallDescription

                    if sub.flags & rd.DrawFlags.SetMarker:
                        name = sub.name

                    # Check this draw
                    if sub.flags & rd.DrawFlags.Drawcall:
                        tests_run = tests_run + 1
                        try:
                            # Set this event as current
                            self.controller.SetFrameEvent(sub.eventId, True)

                            self.filename = (d.name + '@' + name).replace('->', '_')

                            self.check_test(d.name, name, Texture_Zoo.TEST_CAPTURE)
                        except rdtest.TestFailureException as ex:
                            failed = any_failed = True
                            rdtest.log.error(str(ex))

                if not failed:
                    rdtest.log.success("All {} texture tests for {} are OK".format(tests_run, d.name))

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

        try:
            # First check with the local controller
            self.check_capture_with_controller('')
        except rdtest.TestFailureException as ex:
            rdtest.log.error(str(ex))
            failed = True

        # Now shut it down
        self.controller.Shutdown()
        self.controller = None

        # Launch a remote server
        rdtest.launch_remote_server()

        # Wait for it to start
        time.sleep(0.5)

        ret: Tuple[rd.ReplayStatus, rd.RemoteServer] = rd.CreateRemoteServerConnection('localhost')
        status, remote = ret

        proxies = remote.LocalProxies()

        try:
            # Try D3D11 and GL as proxies, D3D12/Vulkan technically don't have proxying implemented even though they
            # will be listed in proxies
            for api in ['D3D11', 'OpenGL']:
                if api not in proxies:
                    continue

                try:
                    ret: Tuple[rd.ReplayStatus, rd.ReplayController] = remote.OpenCapture(proxies.index(api),
                                                                                          capture_filename,
                                                                                          rd.ReplayOptions(), None)
                    status, self.controller = ret

                    # Now check with the proxy
                    self.check_capture_with_controller(api)
                except ValueError:
                    continue
                except rdtest.TestFailureException as ex:
                    rdtest.log.error(str(ex))
                    failed = True
                finally:
                    self.controller.Shutdown()
                    self.controller = None
        finally:
            remote.ShutdownServerAndConnection()

        # Now iterate over all the temp images saved out, load them as captures, and check the texture.
        dir_path = rdtest.get_tmp_path('')

        was_opengl = self.opengl_mode

        # We iterate in filename order, so that dds files get opened before png files.
        for file in os.scandir(dir_path):
            if '.dds' not in file.name and '.png' not in file.name:
                continue

            cap = rd.OpenCaptureFile()
            status = cap.OpenFile(file.path, 'rdc', None)

            if status != rd.ReplayStatus.Succeeded:
                rdtest.log.error("Couldn't open {}".format(file.name))
                failed = True
                continue

            ret: Tuple[rd.ReplayStatus, rd.ReplayController] = cap.OpenCapture(rd.ReplayOptions(), None)
            status, self.controller = ret

            if status != rd.ReplayStatus.Succeeded:
                rdtest.log.error("Couldn't open {}".format(file.name))
                failed = True
                continue

            self.filename = file.name.replace('.dds', '').replace('.png', '')

            [a, b] = file.name.replace('.dds', ' (DDS)').replace('.png', ' (PNG)').split('@')

            self.controller.SetFrameEvent(self.controller.GetDrawcalls()[0].eventId, True)

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

                self.check_test(a, b, Texture_Zoo.TEST_DDS if '.dds' in file.name else Texture_Zoo.TEST_PNG)

                rdtest.log.success("{} loaded with the correct data".format(file.name))
            except rdtest.TestFailureException as ex:
                rdtest.log.error(str(ex))
                failed = True

            self.controller.Shutdown()
            self.controller = None

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")
