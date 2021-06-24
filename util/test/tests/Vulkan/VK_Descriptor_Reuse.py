import renderdoc as rd
import struct
import rdtest


class VK_Descriptor_Reuse(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Reuse'
    demos_frame_cap = 100

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        action: rd.ActionDescription = self.find_action('Duration')

        min_duration = float(action.customName.split(' = ')[1])

        if rd.IsReleaseBuild():
            if min_duration >= 15.0:
                raise rdtest.TestFailureException("Minimum duration noted {} ms is too high".format(min_duration))
            rdtest.log.success("Minimum duration ({}) is OK".format(min_duration))
        else:
            rdtest.log.print("Not checking duration ({}) in non-release build".format(min_duration))

        resources = self.controller.GetResources()
        for i in range(8):
            res: rd.ResourceDescription = [r for r in resources if r.name == 'Offscreen{}'.format(i)][0]
            tex: rd.TextureDescription = self.get_texture(res.resourceId)

            data = self.controller.GetTextureData(res.resourceId, rd.Subresource(0, 0, 0))

            pixels = [struct.unpack_from("4f", data, 16 * p) for p in range(tex.width * tex.height)]

            unique_pixels = list(set(pixels))

            if len(unique_pixels) > 2:
                raise rdtest.TestFailureException("Too many pixel values found ({})".format(len(unique_pixels)))

            if (0.0, 0.0, 0.0, 1.0) not in unique_pixels:
                raise rdtest.TestFailureException("Didn't find background colour in unique pixels list")

            unique_pixels.remove((0.0, 0.0, 0.0, 1.0))

            if not rdtest.value_compare((0.8, 0.8, 0.8, 0.4), unique_pixels[0]):
                raise rdtest.TestFailureException("Didn't find foreground colour in unique pixels list")

            rdtest.log.success("{} has correct contents".format(res.name))
