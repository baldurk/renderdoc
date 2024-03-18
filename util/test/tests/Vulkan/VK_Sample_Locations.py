import renderdoc as rd
import rdtest


class VK_Sample_Locations(rdtest.TestCase):
    demos_test_name = 'VK_Sample_Locations'

    def check_capture(self):
        action: rd.ActionDescription = self.find_action("Degenerate")
        self.controller.SetFrameEvent(action.next.eventId, True)
        pipe: rd.VKState = self.controller.GetVulkanPipelineState()

        if pipe.multisample.rasterSamples != 4:
            raise rdtest.TestFailureException("MSAA sample count is {}, not 1".format(pipe.multisample.rasterSamples))

        sampleLoc: rd.VKSampleLocations = pipe.multisample.sampleLocations

        if sampleLoc.gridWidth != 1:
            raise rdtest.TestFailureException("Sample locations grid width is {}, not 1".format(sampleLoc.gridWidth))
        if sampleLoc.gridHeight != 1:
            raise rdtest.TestFailureException("Sample locations grid height is {}, not 1".format(sampleLoc.gridHeight))

        # [0] and [1] should be identical, as should [2] and [3], but they should be different from each other
        if not sampleLoc.customLocations[0] == sampleLoc.customLocations[1]:
            raise rdtest.TestFailureException("In degenerate case, sample locations [0] and [1] don't match: {} vs {}"
                                              .format(sampleLoc.customLocations[0], sampleLoc.customLocations[1]))

        if not sampleLoc.customLocations[2] == sampleLoc.customLocations[3]:
            raise rdtest.TestFailureException("In degenerate case, sample locations [2] and [3] don't match: {} vs {}"
                                              .format(sampleLoc.customLocations[2], sampleLoc.customLocations[3]))

        if sampleLoc.customLocations[1] == sampleLoc.customLocations[2]:
            raise rdtest.TestFailureException("In degenerate case, sample locations [1] and [2] DO match: {} vs {}"
                                              .format(sampleLoc.customLocations[1], sampleLoc.customLocations[2]))

        action: rd.ActionDescription = self.find_action("Rotated")
        self.controller.SetFrameEvent(action.next.eventId, True)
        pipe: rd.VKState = self.controller.GetVulkanPipelineState()

        if pipe.multisample.rasterSamples != 4:
            raise rdtest.TestFailureException("MSAA sample count is {}, not 1".format(pipe.multisample.rasterSamples))

        sampleLoc: rd.VKSampleLocations = pipe.multisample.sampleLocations

        if sampleLoc.gridWidth != 1:
            raise rdtest.TestFailureException("Sample locations grid width is {}, not 1".format(sampleLoc.gridWidth))
        if sampleLoc.gridHeight != 1:
            raise rdtest.TestFailureException("Sample locations grid height is {}, not 1".format(sampleLoc.gridHeight))

        # All sample locations should be unique
        if sampleLoc.customLocations[0] == sampleLoc.customLocations[1]:
            raise rdtest.TestFailureException("In rotated case, sample locations [0] and [1] DO match: {} vs {}"
                                              .format(sampleLoc.customLocations[0], sampleLoc.customLocations[1]))

        if sampleLoc.customLocations[1] == sampleLoc.customLocations[2]:
            raise rdtest.TestFailureException("In rotated case, sample locations [1] and [2] DO match: {} vs {}"
                                              .format(sampleLoc.customLocations[1], sampleLoc.customLocations[2]))

        if sampleLoc.customLocations[2] == sampleLoc.customLocations[3]:
            raise rdtest.TestFailureException("In rotated case, sample locations [2] and [3] DO match: {} vs {}"
                                              .format(sampleLoc.customLocations[2], sampleLoc.customLocations[3]))

        rdtest.log.success("Pipeline state is correct")

        # Grab the multisampled image's ID here
        save_data = rd.TextureSave()
        curpass: rd.VKCurrentPass = pipe.currentPass
        save_data.resourceId = curpass.framebuffer.attachments[curpass.renderpass.colorAttachments[0]].resource
        save_data.destType = rd.FileType.PNG
        save_data.sample.mapToArray = False

        dim = (0, 0)
        fmt: rd.ResourceFormat = None
        texs = self.controller.GetTextures()
        for tex in texs:
            tex: rd.TextureDescription
            if tex.resourceId == save_data.resourceId:
                dim = (tex.width, tex.height)
                fmt = tex.format

        if dim == (0,0):
            raise rdtest.TestFailureException("Couldn't get dimensions of texture")

        halfdim = (dim[0] >> 1, dim[1])

        if (fmt.type != rd.ResourceFormatType.Regular or fmt.compByteWidth != 1 or fmt.compCount != 4):
            raise rdtest.TestFailureException("Texture is not RGBA8 as expected: {}".format(fmt.Name()))

        stride = fmt.compByteWidth * fmt.compCount * dim[0]

        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        # Due to the variability of rasterization between implementations or even drivers,
        # we don't want to check against a 'known good'.
        # So instead we verify that at the first degenerate action each pair of two sample's images are identical and that
        # in the rotated grid case each sample's image is distinct.
        # In future we could also check that the degenerate case 'stretches' the triangle up, as with the way the
        # geometry is defined the second sample image should be a superset (i.e. strictly more samples covered).
        rotated_paths = []
        degenerate_paths = []

        for sample in range(0, 4):
            tmp_path = rdtest.get_tmp_path('sample{}.png'.format(sample))
            degenerate_path = rdtest.get_tmp_path('degenerate{}.png'.format(sample))
            rotated_path = rdtest.get_tmp_path('rotated{}.png'.format(sample))

            rotated_paths.append(rotated_path)
            degenerate_paths.append(degenerate_path)

            save_data.sample.sampleIndex = sample
            self.controller.SaveTexture(save_data, tmp_path)

            combined_data = rdtest.png_load_data(tmp_path)

            # crop left for degenerate, and crop right for rotated
            degenerate = []
            rotated = []
            for row in range(0, dim[1]):
                srcstart = row * stride

                len = halfdim[0] * fmt.compCount

                degenerate.append(combined_data[row][0:len])
                rotated.append(combined_data[row][len:])

            rdtest.png_save(degenerate_path, degenerate, halfdim, True)
            rdtest.png_save(rotated_path, rotated, halfdim, True)

        # first two degenerate images should be identical, as should the last two, and they should be different.
        if not rdtest.png_compare(degenerate_paths[0], degenerate_paths[1], 0):
            raise rdtest.TestFailureException("Degenerate grid sample 0 and 1 are different",
                                              degenerate_paths[0], degenerate_paths[1])

        if not rdtest.png_compare(degenerate_paths[2], degenerate_paths[3], 0):
            raise rdtest.TestFailureException("Degenerate grid sample 2 and 3 are different",
                                              degenerate_paths[2], degenerate_paths[3])

        if rdtest.png_compare(degenerate_paths[1], degenerate_paths[2], 0):
            raise rdtest.TestFailureException("Degenerate grid sample 1 and 2 are identical",
                                              degenerate_paths[1], degenerate_paths[2])

        rdtest.log.success("Degenerate grid sample images are as expected")

        # all rotated images should be different
        for A in range(0, 4):
            for B in range(A+1, 4):
                if rdtest.png_compare(rotated_paths[A], rotated_paths[B], 0):
                    raise rdtest.TestFailureException("Rotated grid sample {} and {} are identical".format(A, B),
                                                      rotated_paths[A], rotated_paths[B])

        rdtest.log.success("Rotated grid sample images are as expected")
