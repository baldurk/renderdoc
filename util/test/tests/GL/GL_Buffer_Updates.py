import rdtest
import renderdoc as rd


class GL_Buffer_Updates(rdtest.TestCase):
    demos_test_name = 'GL_Buffer_Updates'

    def check_capture(self):
        tex = rd.TextureDisplay()

        # At each action, the centre pixel of the viewport should be green
        action = self.get_first_action()
        while action is not None:
            self.controller.SetFrameEvent(action.eventId, False)

            if action.flags & rd.ActionFlags.Drawcall:
                pipe = self.controller.GetPipelineState()
                tex = self.controller.GetPipelineState().GetOutputTargets()[0].resource

                view = self.controller.GetPipelineState().GetViewport(0)

                x,y = int(view.x + view.width / 2), int(view.y + view.height / 2)

                # convert to top-left co-ordinates for use with PickPixel
                y = self.get_texture(tex).height - y

                self.check_pixel_value(tex, x, y, [0.0, 1.0, 0.0, 1.0])

            action = action.nextAction

        rdtest.log.success("Draws are all green")

        # Open the capture and grab the thumbnail, check that it is all green too (dirty way of verifying we didn't
        # break in-app updates but somehow end up with the right data)
        cap = rd.OpenCaptureFile()

        # Open a particular file
        result = cap.OpenFile(self.capture_filename, '', None)

        # Make sure the file opened successfully
        if result != rd.ResultCode.Succeeded:
            cap.Shutdown()
            raise rdtest.TestFailureException(f"Couldn't open '{self.capture_filename}': {result!s}")

        thumb = cap.GetThumbnail(rd.FileType.PNG, 0)

        tmp_path = rdtest.get_tmp_path('thumbnail.png')

        with open(tmp_path, 'wb') as f:
            f.write(thumb.data)

        test_reader = rdtest.png.Reader(filename=tmp_path)

        test_w, test_h, test_data, test_info = test_reader.read()

        box_w = test_w//8
        rows = test_h//box_w

        offset = box_w//2

        rows_data = list(test_data)

        comps = 4 if test_info['alpha'] else 3

        for row in range(0,rows):
            y = row * box_w + offset

            row_data = rows_data[y]

            for col in range(0, 8):
                x = col * box_w + offset

                pixel = (row_data[x * comps + 0]/255, row_data[x * comps + 1]/255,
                         row_data[x * comps + 2]/255)

                if (not rdtest.value_compare((0.2, 0.2, 0.2), pixel, 1) and
                    not rdtest.value_compare((0.0, 1.0, 0.0), pixel, 1)):
                    raise rdtest.TestFailureException(
                        f"Thumbnail of backbuffer at {x},{y} has bad pixel: {pixel}", tmp_path)

        rdtest.log.success("Thumbnail is as expected")
