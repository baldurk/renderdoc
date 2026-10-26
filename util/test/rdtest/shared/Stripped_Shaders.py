import rdtest
import shutil
import renderdoc as rd
import re


# Not a real test, re-used by API-specific tests
class Stripped_Shaders(rdtest.TestCase):
    internal = True
    demos_test_name = None

    def get_capture_options(self):
        # We don't actually need to set any capture-time options but we'll use this to change the search dir paths
        paths = rd.SetConfigSetting("DXBC.Debug.SearchDirPaths")

        paths.DeleteChildren()
        paths.AddChild(rd.makeSDString("path", rdtest.get_tmp_dir()))

        # this won't automatically save and we won't call SaveConfigSettings()

        return rd.CaptureOptions()

    def check_capture(self):
        self.check_debug_symbols(False)

        filename = self.retrieve_capture()
        embed_path = rdtest.get_tmp_path("embedded.rdc")

        shutil.copy(filename, embed_path)

        # shut down the controller that is for the old file, to keep it the same
        self.controller.Shutdown()
        self.controller = None
        if self.cap is not None:
            self.cap.Shutdown()
            self.cap = None

        # open the version we're going to embed into
        self.cap, self.controller = rdtest.open_capture(embed_path)

        assert self.cap is not None
        assert len(self.cap.GetPendingDependenciesNicknames()) > 0

        # embed the dependencies
        assert self.cap.EmbedDependenciesIntoCapture().OK()

        self.controller.Shutdown()
        self.cap.Shutdown()

        # now clear the search paths, this should make at least one fail
        rd.SetConfigSetting("DXBC.Debug.SearchDirPaths").DeleteChildren()

        self.cap, self.controller = rdtest.open_capture(embed_path)
        self.sdfile = self.controller.GetStructuredFile()

        # check everything is still OK
        self.check_debug_symbols(True)

    def check_debug_symbols(self, embedded: bool):
        test = self.find_action("test:", 0)

        while test is not None:
            pix_cfg = self.get_action(test.eventId + 2)
            assert pix_cfg is not None
            draw = self.get_action(test.eventId + 3)
            assert draw is not None

            search_text = test.customName.replace("test: ", "")
            assert "pixel" in pix_cfg.customName

            self.set_event(draw.eventId, False)

            pipe = self.controller.GetPipelineState()

            stages = [
                (1, rd.ShaderStage.Vertex, "vertex", "shader_vs.dbg"),
                (2, rd.ShaderStage.Pixel, "pixel", "shader_ps.dbg"),
            ]

            for offs, stage, name, manual_filename in stages:
                cfg = self.get_action(test.eventId + offs)
                assert cfg is not None

                assert name in cfg.customName

                refl = pipe.GetShaderReflection(stage)

                assert len(refl.debugInfo.files) == 1

                src = refl.debugInfo.files[0].contents

                assert f"// {search_text} // {name}" in src

                log = refl.debugInfo.debugInfoLoadingLog

                assert re.match("Found ", log, re.IGNORECASE)

                if "auto" in cfg.customName:
                    assert re.search("[a-f0-9]{32}.pdb", log) is not None
                elif "manual" in cfg.customName:
                    assert f"{manual_filename}'" in log
                else:
                    raise rdtest.TestFailureException(
                        f"Unexpected {name} config {cfg.customName}"
                    )

                filepath = re.search("Shader filepath '([^']*)'", log)
                assert filepath is not None

                filepath = filepath[1].replace("\\", "/")
                tmp_dir = rdtest.get_tmp_dir().replace("\\", "/")

                if "absolute" in cfg.customName:
                    assert tmp_dir in filepath
                    assert "File found using filepath" in log
                    assert "File found in search directory" not in log
                    assert "File found in recursive directory search" not in log
                    assert "embedded in the capture using nickname" not in log
                elif "relative" in cfg.customName:
                    assert tmp_dir not in filepath
                    assert "File found using filepath" not in log
                    if embedded:
                        assert "embedded in the capture using nickname" in log
                        assert "File found in search directory" not in log
                        assert "File found in recursive directory search" not in log
                    else:
                        assert (
                            "File found in search directory" in log
                            or "File found in recursive directory search" in log
                        )
                else:
                    raise rdtest.TestFailureException(
                        f"Unexpected {name} config {cfg.customName}"
                    )

                rdtest.log.success(f"{name} shader debug info and loading log looks OK")

            test = self.find_action("test:", draw.eventId)

        rdtest.log.success("All draws had proper shader debug info")
