###############################################################################
# The MIT License (MIT)
#
# Copyright (c) 2021-2024 Baldur Karlsson
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
###############################################################################

import qrenderdoc as qrd
import renderdoc as rd
from typing import Optional


class Window(qrd.CaptureViewer):
    def __init__(self, ctx: qrd.CaptureContext, version: str):
        super().__init__()

        self.mqt: qrd.MiniQtHelper = ctx.Extensions().GetMiniQtHelper()

        self.ctx = ctx
        self.version = version
        self.topWindow = self.mqt.CreateToplevelWidget("Breadcrumbs", lambda c, w, d: window_closed())

        vert = self.mqt.CreateVerticalContainer()
        self.mqt.AddWidget(self.topWindow, vert)

        self.breadcrumbs = self.mqt.CreateLabel()

        self.mqt.AddWidget(vert, self.breadcrumbs)

        ctx.AddCaptureViewer(self)

    def OnCaptureLoaded(self):
        self.mqt.SetWidgetText(self.breadcrumbs, "Breadcrumbs:")

    def OnCaptureClosed(self):
        self.mqt.SetWidgetText(self.breadcrumbs, "Breadcrumbs:")

    def OnSelectedEventChanged(self, event):
        pass

    def OnEventChanged(self, event):
        action = self.ctx.GetAction(event)

        breadcrumbs = ''

        if action is not None:
            breadcrumbs = '@{}: {}'.format(action.eventId, action.name)

            while action.parent is not None:
                action = action.parent
                breadcrumbs = '@{}: {}'.format(action.eventId, action.name) + '\n' + breadcrumbs

        self.mqt.SetWidgetText(self.breadcrumbs, "Breadcrumbs:\n{}".format(breadcrumbs))


cur_window: Optional[Window] = None


def window_closed():
    global cur_window

    if cur_window is not None:
        cur_window.ctx.RemoveCaptureViewer(cur_window)

    cur_window = None


def window_callback(ctx: qrd.CaptureContext, data):
    global cur_window

    if cur_window is None:
        cur_window = Window(ctx, extiface_version)
        if ctx.HasEventBrowser():
            ctx.AddDockWindow(cur_window.topWindow, qrd.DockReference.TopOf, ctx.GetEventBrowser().Widget(), 0.1)
        else:
            ctx.AddDockWindow(cur_window.topWindow, qrd.DockReference.MainToolArea, None)

    ctx.RaiseDockWindow(cur_window.topWindow)


def menu_callback(ctx: qrd.CaptureContext, data):
    texid = rd.ResourceId.Null()
    depth = ctx.CurPipelineState().GetDepthTarget()

    # Prefer depth if possible
    if depth.resourceId != rd.ResourceId.Null():
        texid = depth.resourceId
    else:
        cols = ctx.CurPipelineState().GetOutputTargets()

        # See if we can get the first colour target instead
        if len(cols) > 1 and cols[0].resourceId != rd.ResourceId.Null():
            texid = cols[0].resourceId

    if texid == rd.ResourceId.Null():
        ctx.Extensions().MessageDialog("Couldn't find any bound target!", "Extension message")
        return
    else:
        mqt = ctx.Extensions().GetMiniQtHelper()
        texname = ctx.GetResourceName(texid)

        def get_minmax(r: rd.ReplayController):
            minvals, maxvals = r.GetMinMax(texid, rd.Subresource(), rd.CompType.Typeless)

            msg = '{} has min {:.4} and max {:.4} in red'.format(texname, minvals.floatValue[0], maxvals.floatValue[0])

            mqt.InvokeOntoUIThread(lambda: ctx.Extensions().MessageDialog(msg, "Extension message"))

        ctx.Replay().AsyncInvoke('', get_minmax)



extiface_version = ''


def register(version: str, ctx: qrd.CaptureContext):
    global extiface_version
    extiface_version = version

    print("Registering my extension for RenderDoc version {}".format(version))

    ctx.Extensions().RegisterWindowMenu(qrd.WindowMenu.Tools, ["My extension"], menu_callback)
    ctx.Extensions().RegisterWindowMenu(qrd.WindowMenu.Window, ["Extension Window"], window_callback)


def unregister():
    print("Unregistering my extension")

    global cur_window

    if cur_window is not None:
        # The window_closed() callback will unregister the capture viewer
        cur_window.ctx.Extensions().GetMiniQtHelper().CloseToplevelWidget(cur_window.topWindow)
        cur_window = None
