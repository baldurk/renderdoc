import rdtest
import renderdoc as rd


class GL_Buffer_Updates(rdtest.TestCase):
    def get_capture(self):
        return rdtest.run_and_capture("demos_x64", "GL_Buffer_Updates", 5)

    def check_capture(self):
        self.check_final_backbuffer()

        # Open the capture and grab the thumbnail, check that it is all green too (dirty way of verifying we didn't
        # break in-app updates but somehow end up with the right data)
        cap = rd.OpenCaptureFile()

        # Open a particular file
        status = cap.OpenFile(self.capture_filename, '', None)

        # Make sure the file opened successfully
        if status != rd.ReplayStatus.Succeeded:
            cap.Shutdown()
            raise rdtest.TestFailureException("Couldn't open '{}': {}".format(self.capture_filename, str(status)))

        thumb: rd.Thumbnail = cap.GetThumbnail(rd.FileType.PNG, 0)

        tmp_path = rdtest.get_tmp_path('thumbnail.png')

        with open(tmp_path, 'wb') as f:
            f.write(thumb.data)

        # The original thumbnail should also be identical, since we have the uncompressed extended thumbnail.
        ref_path = self.get_ref_path('backbuffer.png')

        if not rdtest.png_compare(tmp_path, ref_path):
            raise rdtest.TestFailureException("Reference backbuffer and thumbnail image differ", tmp_path, ref_path)

        rdtest.log.success("Thumbnail is identical to reference")

