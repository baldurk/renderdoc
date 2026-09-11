from __future__ import annotations
import renderdoc as rd
import qrenderdoc as qrd

from typing import IO, Any, Dict, TYPE_CHECKING

if TYPE_CHECKING:
    pyrenderdoc = qrd.CaptureContext()

importerror = None

try:
    import rdtest
    from tests import *

    import os

    root = os.path.dirname(os.path.dirname(os.path.realpath(rdtest.__file__)))

    rdtest.set_root_dir(root)
    rdtest.set_artifact_dir(os.path.join(root, "artifacts"))
    rdtest.set_temp_dir(os.path.join(root, "tmp"))
    rdtest.set_demos_binary("")
    rdtest.set_demos_timeout(90)
    rdtest.set_remote_server(None)

except ImportError as ex:
    importerror = ex
    if TYPE_CHECKING:
        import rdtest

        TestCase = rdtest.testcase.TestCase

class AbortTestException(BaseException):
    pass


class Outputter(IO[str]):
    def __init__(self):
        self.wid: qrd.QWidget | None = None
        self.abort = False

    def write(self, t: str):
        ext = pyrenderdoc.Extensions()
        mqt = ext.GetMiniQtHelper()

        if self.abort:
            self.abort = False
            mqt.AppendText(self.wid, "\n")
            raise AbortTestException()

        if self.wid is not None:
            mqt.AppendText(self.wid, t)

        # real IO returns number of bytes, this just keeps types happy
        return 0

    def clear(self):
        ext = pyrenderdoc.Extensions()
        mqt = ext.GetMiniQtHelper()

        if self.wid is not None:
            mqt.SetWidgetText(self.wid, "")

    def flush(self):
        pass


outputter = Outputter()


class TestRunner(qrd.CaptureViewer):
    def __init__(self):
        self.test: TestCase | None = None
        self.thread = None
        self.relaunch: qrd.QWidget | None = None
        self.abort: qrd.QWidget | None = None
        super().__init__()

    def OnCaptureLoaded(self):
        ext = pyrenderdoc.Extensions()
        self.mqt = ext.GetMiniQtHelper()

        if self.test is not None:
            self.run_test()

    def run_test(self):
        if self.test is not None:
            import threading

            outputter.clear()

            self.mqt.SetWidgetEnabled(self.relaunch, False)
            self.mqt.SetWidgetEnabled(self.abort, True)

            # if this is a re-run, clean up the previous thread
            if self.thread is not None:
                self.thread.join()

            self.thread = threading.Thread(target=lambda: self.check())
            self.thread.start()

    def OnCaptureClosed(self):
        if self.thread is not None:
            self.thread.join()
            self.thread = None

        if self.relaunch is not None:
            self.mqt.SetWidgetEnabled(self.relaunch, False)
        if self.abort is not None:
            self.mqt.SetWidgetEnabled(self.abort, False)

    def check(self):
        self.thread.name = "Test thread"

        success = True
        debugging = False

        try:
            import debugpy  # type: ignore

            debugging = bool(debugpy.is_client_connected()) # type: ignore
        except ImportError:
            pass

        rdtest.log.set_context(lambda: self.test.log_context())

        self.test.controller = pyrenderdoc.GetBlockingController()
        self.test.sdfile = pyrenderdoc.GetStructuredFile()

        rdtest.log.begin_test(rdtest.get_current_test_name(), False)

        if debugging:
            self.test.check_capture()
        else:
            try:
                self.test.check_capture()
            except AbortTestException as ex:
                pass  # don't open any contexts
            except rdtest.TestFailureException as ex:
                outputter.write("\n")
                rdtest.log.failure(ex)
                success = False
            except Exception as ex:
                outputter.write("\n")
                rdtest.log.failure(ex)
                success = False

        rdtest.log.end_test(rdtest.get_current_test_name(), False)

        if not success:
            pipe = self.test.controller.GetPipelineState()

            for c in self.test.contexts:
                if isinstance(c, rdtest.HistoryContext):
                    disp = rd.TextureDisplay()
                    disp.typeCast = c.cast
                    view = pyrenderdoc.ViewPixelHistory(
                        c.tex, c.x, c.y, c.sub.slice, disp
                    )
                    modifs = self.test.controller.PixelHistory(
                        c.tex, c.x, c.y, c.sub, c.cast
                    )
                    view.SetHistory(modifs)
                    pyrenderdoc.AddDockWindow(
                        view.Widget(), qrd.DockReference.TransientPopupArea, None
                    )

                if isinstance(c, rdtest.VertexDebugContext):
                    trace = self.test.controller.DebugVertex(
                        c.vtx, c.inst, c.idx, c.view
                    )
                    refl = pipe.GetShaderReflection(rd.ShaderStage.Vertex)
                    if refl is None:
                        outputter.write("Tried to debug with no vertex shader bound")
                    else:
                        pipeline = pipe.GetGraphicsPipelineObject()
                        view = pyrenderdoc.DebugShader(
                            refl, pipeline, trace, "Failed debug"
                        )
                        pyrenderdoc.AddDockWindow(
                            view.Widget(), qrd.DockReference.MainToolArea, None
                        )

                if isinstance(c, rdtest.PixelDebugContext):
                    trace = self.test.controller.DebugPixel(c.x, c.y, c.inputs)
                    refl = pipe.GetShaderReflection(rd.ShaderStage.Pixel)
                    if refl is None:
                        outputter.write("Tried to debug with no pixel shader bound")
                    else:
                        pipeline = pipe.GetGraphicsPipelineObject()
                        view = pyrenderdoc.DebugShader(
                            refl, pipeline, trace, "Failed debug"
                        )
                        pyrenderdoc.AddDockWindow(
                            view.Widget(), qrd.DockReference.MainToolArea, None
                        )

                if isinstance(c, rdtest.ComputeDebugContext):
                    trace = self.test.controller.DebugThread(c.group, c.thread)
                    refl = pipe.GetShaderReflection(rd.ShaderStage.Compute)
                    if refl is None:
                        outputter.write("Tried to debug with no compute shader bound")
                    else:
                        pipeline = pipe.GetComputePipelineObject()
                        view = pyrenderdoc.DebugShader(
                            refl, pipeline, trace, "Failed debug"
                        )
                        pyrenderdoc.AddDockWindow(
                            view.Widget(), qrd.DockReference.MainToolArea, None
                        )

            for c in self.test.contexts:
                if isinstance(c, rdtest.PickContext):
                    tex = pyrenderdoc.GetTextureViewer()
                    pyrenderdoc.ShowTextureViewer()

                    tex.ViewTexture(c.tex, rd.CompType.Typeless, True)
                    tex.SetSelectedSubresource(c.sub)
                    tex.GotoLocation(c.x, c.y)

        rdtest.log.set_context(None)

        pyrenderdoc.InvokeOntoUIThread(
            lambda: self.mqt.SetWidgetEnabled(self.relaunch, True)
            and self.mqt.SetWidgetEnabled(self.abort, False)
        )

        self.test = None


watcher = TestRunner()


def run_test(pyrenderdoc: qrd.CaptureContext, widget: qrd.QWidget, data: str):
    ext = pyrenderdoc.Extensions()

    name = rdtest.get_current_test_name()

    if name == "":
        return

    if name not in rdtest.fetch_tests():
        ext.ErrorDialog(f"{name} unsupported")
        return

    watcher.test = rdtest.get_current_test()()

    pyrenderdoc.ShowCaptureDialog()
    dialog = pyrenderdoc.GetCaptureDialog()
    s = dialog.Settings()
    s.executable = rdtest.get_demos_binary()
    s.commandLine = f"{name} --frames 10"
    s.numQueuedFrames = 1
    s.queuedFrameCap = 5
    dialog.SetSettings(s)
    dialog.Launch()


def rerun_test(pyrenderdoc: qrd.CaptureContext, widget: qrd.QWidget, data: str):
    rdtest.reload_test(rdtest.get_current_test_name())

    watcher.test = rdtest.get_current_test()()

    watcher.run_test()


def abort_test(pyrenderdoc: qrd.CaptureContext, widget: qrd.QWidget, data: str):
    outputter.abort = True


def fast_fail_changed(pyrenderdoc: qrd.CaptureContext, widget: qrd.QWidget, data: str):
    ext = pyrenderdoc.Extensions()
    mqt = ext.GetMiniQtHelper()

    rdtest.log.fast_fail = mqt.IsWidgetChecked(widget)


def filter_updated(selector: qrd.QWidget, filter: qrd.QWidget):
    ext = pyrenderdoc.Extensions()
    mqt = ext.GetMiniQtHelper()

    test_names = sorted([x.__name__ for x in rdtest.get_tests()]) 
    filter_text = mqt.GetWidgetText(filter)

    import re

    cur = mqt.GetWidgetText(selector)

    tests = [x for x in test_names if re.search(filter_text, x) is not None]

    mqt.SetComboOptions(selector, tests)

    if cur in tests:
        mqt.SelectComboOption(selector, cur)

def harness(pyrenderdoc: qrd.CaptureContext, params: Dict[str, Any]):
    ext = pyrenderdoc.Extensions()
    mqt = ext.GetMiniQtHelper()

    top = mqt.CreateToplevelWidget("Test Harness")

    filter = mqt.CreateTextBox(True, lambda ctx, wdg, name: filter_updated(selector, wdg))

    selector = mqt.CreateComboBox(
        False, lambda ctx, wdg, name: rdtest.set_current_test(name)
    )

    filter_updated(selector, filter)

    launch = mqt.CreateButton(run_test)
    mqt.SetWidgetText(launch, "Launch test")

    relaunch = mqt.CreateButton(rerun_test)
    mqt.SetWidgetText(relaunch, "Re-run")
    mqt.SetWidgetEnabled(relaunch, False)

    abort = mqt.CreateButton(abort_test)
    mqt.SetWidgetText(abort, "Abort")
    mqt.SetWidgetEnabled(abort, False)

    watcher.relaunch = relaunch
    watcher.abort = abort

    outputWidget = mqt.CreateTextBox(False)

    outputter.wid = outputWidget

    mqt.SetWidgetFont(outputWidget, "_fixed", 0, False, False)

    fast_fail = mqt.CreateCheckbox(fast_fail_changed)
    mqt.SetWidgetText(fast_fail, "Fast fail (exception on first error print)")
    mqt.SetWidgetChecked(fast_fail, True)

    vlayout = mqt.CreateVerticalContainer()

    hlayout = mqt.CreateHorizontalContainer()
    mqt.SetLayoutMargins(hlayout, 0, 0)

    mqt.AddWidget(hlayout, filter)
    mqt.AddWidget(hlayout, selector)
    mqt.AddWidget(vlayout, hlayout)
    
    hlayout = mqt.CreateHorizontalContainer()
    mqt.SetLayoutMargins(hlayout, 0, 0)

    mqt.AddWidget(hlayout, launch)
    mqt.AddWidget(hlayout, abort)
    mqt.AddWidget(hlayout, relaunch)
    mqt.AddWidget(vlayout, hlayout)

    mqt.AddWidget(vlayout, fast_fail)
    mqt.AddWidget(vlayout, outputWidget)

    mqt.AddWidget(top, vlayout)

    pyrenderdoc.AddDockWindow(
        top, qrd.DockReference.RightWindowSide, pyrenderdoc.GetMainWindow().Widget()
    )


def register(version: str, pyrenderdoc: qrd.CaptureContext):
    print(f"Test Harness loaded in RenderDoc {version}")

    # only register the menu etc if we imported the tests project
    if "rdtest" in globals():
        pyrenderdoc.Extensions().RegisterWindowMenu(
            qrd.WindowMenu.Tools, ["Test Harness"], harness
        )

        rdtest.log.add_output(outputter)
        rdtest.set_capture_context(pyrenderdoc)

        pyrenderdoc.AddCaptureViewer(watcher)
    else:
        print(f"Test import failed: {importerror!s}")


def unregister():
    print(f"Test Harness being unloaded")

    outputter.wid = None

    pyrenderdoc.RemoveCaptureViewer(watcher)
