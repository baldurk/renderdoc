import renderdoc as rd
import struct
import rdtest


class VK_Descriptor_Reuse(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Reuse'
    demos_frame_cap = 100

    def check_capture(self):
        last_action = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        action = self.find_action('Duration')

        min_duration = float(action.customName.split(' = ')[1])

        if rd.IsReleaseBuild():
            if min_duration >= 15.0:
                raise rdtest.TestFailureException(f"Minimum duration noted {min_duration} ms is too high")
            rdtest.log.success(f"Minimum duration ({min_duration}) is OK")
        else:
            rdtest.log.print(f"Not checking duration ({min_duration}) in non-release build")

        resources = self.controller.GetResources()
        for i in range(8):
            res = [r for r in resources if r.name == f'Offscreen{i}'][0]
            tex = self.get_texture(res.resourceId)

            data = self.controller.GetTextureData(res.resourceId, rd.Subresource(0, 0, 0))

            pixels = [struct.unpack_from("4f", data, 16 * p) for p in range(tex.width * tex.height)]

            unique_pixels = list(set(pixels))

            if len(unique_pixels) > 2:
                raise rdtest.TestFailureException(f"Too many pixel values found ({len(unique_pixels)})")

            if (0.0, 0.0, 0.0, 1.0) not in unique_pixels:
                raise rdtest.TestFailureException("Didn't find background colour in unique pixels list")

            unique_pixels.remove((0.0, 0.0, 0.0, 1.0))

            if not rdtest.value_compare((0.8, 0.8, 0.8, 0.4), unique_pixels[0]):
                raise rdtest.TestFailureException("Didn't find foreground colour in unique pixels list")

            rdtest.log.success(f"{res.name} has correct contents")
