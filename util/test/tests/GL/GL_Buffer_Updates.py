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

                view: rd.Viewport = self.controller.GetPipelineState().GetViewport(0)

                x,y = int(view.x + view.width / 2), int(view.y + view.height / 2)

                # convert to top-left co-ordinates for use with PickPixel
                y = self.get_texture(tex).height - y

                self.check_pixel_value(tex, x, y, [0.0, 1.0, 0.0, 1.0])

            action = action.next

        rdtest.log.success("Draws are all green")

        # Now save the backbuffer to disk
        ref_path = rdtest.get_tmp_path('backbuffer.png')

        save_data = rd.TextureSave()
        save_data.resourceId = tex
        save_data.destType = rd.FileType.PNG

        self.controller.SaveTexture(save_data, ref_path)

        # Open the capture and grab the thumbnail, check that it is all green too (dirty way of verifying we didn't
        # break in-app updates but somehow end up with the right data)
        cap = rd.OpenCaptureFile()

        # Open a particular file
        result = cap.OpenFile(self.capture_filename, '', None)

        # Make sure the file opened successfully
        if result != rd.ResultCode.Succeeded:
            cap.Shutdown()
            raise rdtest.TestFailureException("Couldn't open '{}': {}".format(self.capture_filename, str(result)))

        thumb: rd.Thumbnail = cap.GetThumbnail(rd.FileType.PNG, 0)

        tmp_path = rdtest.get_tmp_path('thumbnail.png')

        with open(tmp_path, 'wb') as f:
            f.write(thumb.data)

        # The original thumbnail should also be identical, since we have the uncompressed extended thumbnail.

        if not rdtest.png_compare(tmp_path, ref_path):
            raise rdtest.TestFailureException("Reference backbuffer and thumbnail image differ", tmp_path, ref_path)

        rdtest.log.success("Thumbnail is identical to reference")
