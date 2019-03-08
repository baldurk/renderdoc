/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "TipsDialog.h"
#include "Code/QRDUtils.h"
#include "ui_TipsDialog.h"

#include <stdlib.h>
#include <time.h>

TipsDialog::TipsDialog(ICaptureContext &Ctx, QWidget *parent)
    : m_Ctx(Ctx), QDialog(parent), ui(new Ui::TipsDialog), m_currentTip(0)
{
  ui->setupUi(this);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  initialize();

  if(m_Ctx.Config().Tips_HasSeenFirst)
  {
    showRandomTip();
  }
  else
  {
    showTip(m_currentTip);
  }

  m_Ctx.Config().Tips_HasSeenFirst = true;
}

TipsDialog::~TipsDialog()
{
  delete ui;
}

void TipsDialog::initialize()
{
  ///////////////////////////////////////////////////////////
  // This section of code is auto-generated. Modifications //
  // will be lost if made by hand.                         //
  //                                                       //
  // If you have a tip you'd like to add, email it to me   //
  // or open an issue on github to suggest it.             //
  ///////////////////////////////////////////////////////////

  // Tip 1
  m_tips.push_back(
      Tip(tr("Talk to me!"),
          tr("RenderDoc is a labour of love and has been developed from scratch entirely in my "
             "spare time. If you run into a bug, have a feature request or just have a question, "
             "please feel free to get in touch and I'm always happy to talk and help out in any "
             "way I can - baldurk@baldurk.org.")));

  // Tip 2
  m_tips.push_back(Tip(tr("Quick channel toggling"),
                       tr("Right clicking on a channel button in the texture viewer will select it "
                          "alone. If it is already the only channel selected, the meaning is "
                          "inverted and all others will be selected.\n\nThis is most useful for "
                          "quickly toggling between RGB and Alpha-only views.")));

  // Tip 3
  m_tips.push_back(
      Tip(tr("Quick range autofitting"),
          tr("The magic wand auto-fitting button in the texture viewer automatically calculates "
             "the min and max values of any visible channel in the texture, and rescales the "
             "visible range to include them.\n\nIf you right click on it, it will toggle on a mode "
             "to automatically rescale every time the viewed texture changes.\n")));

  // Tip 4
  m_tips.push_back(
      Tip(tr("Choosing mesh elements to visualise"),
          tr("In the mesh viewer, you can right click on any of the element columns to select that "
             "element as either position or secondary property to render. This way you can view a "
             "mesh in UV space, or visualise normals.")));

  // Tip 5
  m_tips.push_back(Tip(tr("Visualising secondary mesh properties"),
                       tr("In the mesh viewer, you can select an element as 'secondary', and in "
                          "the solid shading dropdown choosing secondary will display the element "
                          "as RGB colour on top of the mesh.")));

  // Tip 6
  m_tips.push_back(
      Tip(tr("Register highlighting in the shader debugger"),
          tr("While debugging a shader, clicking on a register or constant buffer variable in the "
             "disassembly will highlight all other uses of that register or variable in the "
             "disassembly. It will also highlight the matching row in the watch windows.")));

  // Tip 7
  m_tips.push_back(
      Tip(tr("Shader register value display"),
          tr("If you want to see to see a register reinterpreted as different types, you can hover "
             "over it either in the disassembly or in the watch windows. A tooltip will show it "
             "interpreted as float, uint decimal, int decimal or hexadecimal.")));

  // Tip 8
  m_tips.push_back(
      Tip(tr("Custom shader watch expressions"),
          tr("In addition to the pre-filled watch windows while shader debugging, you can also "
             "enter custom watch expressions. This takes the form of e.g. r0.xyz. You can append "
             ",x or ,b to specify the type - full list in the docs.\n")));

  // Tip 9
  m_tips.push_back(Tip(tr("Shader debugger float/int toggle"),
                       tr("By default register values are shown as floats, but you can toggle this "
                          "to default to ints either in the shader debugger toolbar, or by right "
                          "clicking and toggling int/float display.\n")));

  // Tip 10
  m_tips.push_back(Tip(tr("D3D11 shader debug information"),
                       tr("You'll get the best results in RenderDoc by stripping as little as "
                          "possible from D3D11 shaders. Reflection data is used all over the place "
                          "to produce a nicer debugging experience.\n")));

  // Tip 11
  m_tips.push_back(
      Tip(tr("Shader editing & Replacement"),
          tr("RenderDoc has the ability to edit and replace shaders and see the results live in "
             "the replay. On the pipeline state view, click the edit icon next to the shader. If "
             "source is available, it will be compiled, otherwise an empty stub with resources "
             "will be generated.\n\nThe shader will be replaced everywhere it is used in the "
             "frame, the original will be restored when the edit window is closed.\n")));

  // Tip 12
  m_tips.push_back(
      Tip(tr("Linear/Gamma display of textures"),
          tr("RenderDoc interprets all textures in gamma space - even if the data is linear such "
             "as a normal map. This is by convention, since typically external image viewers will "
             "display a normal map as gamma data. This can be overridden by toggling the gamma "
             "button in the texture viewer.\n")));

  // Tip 13
  m_tips.push_back(
      Tip(tr("Seeing texture usage in a capture"),
          tr("RenderDoc has a list of how each texture is bound and used - whether as a shader "
             "resource, an output target, or a copy source. When the texture is active in the "
             "texture viewer this usage will be displayed on the timeline bar at the top.\n\nYou "
             "can also right click on the thumbnails in the texture viewer to see a list of this "
             "usage, and clicking any entry will jump to that event.\n")));

  // Tip 14
  m_tips.push_back(
      Tip(tr("Custom buffer formatting"),
          tr("When opening a raw view of a buffer, such as a vertex buffer or compute read/write "
             "buffer resource, you can apply custom formatting to it to dictate the layout of the "
             "elements in typical shader syntax.\n\nThis formatting is also available for constant "
             "buffers to override the layout reflected from the shader.\n")));

  // Tip 15
  m_tips.push_back(
      Tip(tr("Pipeline HTML export"),
          tr("The pipeline view contains an HTML export function, which dumps the raw state of the "
             "entire pipeline out to a specified file. This can be useful for comparison between "
             "two events, or for having all information available in a unified text format.\n")));

  // Tip 16
  m_tips.push_back(Tip(tr("Python scripting"),
                       tr("RenderDoc supports some amount of Python scripting. Open up the Python "
                          "shell in the UI to either use it interactively or load and execute "
                          "python scripts.\n\nThe 'renderdoc' object is an instance of the Core "
                          "class - see the RenderDoc source for more information.")));

  // Tip 17
  m_tips.push_back(Tip(
      tr("Pixel history view"),
      tr("RenderDoc supports a pixel history view, showing the list of all modification events "
         "that happened to a specified pixel. To launch it, simply pick the pixel you would like "
         "to view the history of in the texture viewer, and click the 'history' button underneath "
         "the zoomed-in pixel context.\n\nEach event will show up red or green depending on "
         "whether it affected or didn't affect the pixel. By expanding the event, you can see the "
         "possibly several primitives within the draw that overdrew the pixel.\n")));

  // Tip 18
  m_tips.push_back(
      Tip(tr("List of active textures"),
          tr("On the texture viewer, the texture list button under the 'Actions' section will open "
             "a filterable list of all texture resources. Clicking on any of the entries will open "
             "a locked tab of that texture, always showing the contents of the texture at the "
             "current event regardless of whether or not it is bound to the pipeline.\n")));

  // Tip 19
  m_tips.push_back(Tip(
      tr("Locked texture tabs"),
      tr("You can open a locked texture tab from the texture list, or by right or double clicking "
         "on a texture's thumbnail.\n\nWith a locked tab of a texture, that tab will always show "
         "that texture, regardless of what is bound to the pipeline. This way you can track the "
         "updates of a texture through binds and unbinds, e.g. ping-pong rendertarget use.\n")));

  // Tip 20
  m_tips.push_back(
      Tip(tr("Gathering of per-event callstacks"),
          tr("RenderDoc is able to gather callstacks either per-drawcall or per-API event. You can "
             "do this by enabling the option before launching an application capture.\n\nWhen "
             "loading the log, initially the callstacks will not be available until symbols are "
             "resolved. Go to tools -> resolve symbols to load up the pdbs matching the modules "
             "from the application.\n")));

  // Tip 21
  m_tips.push_back(Tip(
      tr("Texture debugging overlays"),
      tr("In the texture viewer, you can select from several helpful debugging overlays over the "
         "current view. This can show wireframe or solid coloour overlays of the current drawcall, "
         "as well as showing depth pass/fail or even representing quad overdraw as a heatmap.\n")));

  // Tip 22
  m_tips.push_back(
      Tip(tr("Custom texture display shaders"),
          tr("RenderDoc supports writing custom shaders to decode the viewed texture, which can be "
             "useful to e.g. colourise stencil values or decode a packed gbuffer texture.\n\nIn "
             "the toolbar in the texture viewer, select custom instead of RGBA on the left side, "
             "and use the UI to create a new shader. The docs contain full listings of available "
             "constants and resources to bind.\n")));

  // Tip 23
  m_tips.push_back(
      Tip(tr("Texture histogram"),
          tr("RenderDoc can display a channel histogram showing the distribution of values within "
             "the visible range. Simply click the graph button on the texture viewer to the right "
             "of the range control, and it will expand to show the histogram.\n")));

  // Tip 24
  m_tips.push_back(
      Tip(tr("Attaching to a running instance"),
          tr("If you have launched a program via RenderDoc or your program integrates RenderDoc, "
             "the UI can connect to it once it is running via File -> Attach to Running Instance, "
             "and everything works as if you had launched it.\n\nYou can even do this across a "
             "network, by adding a remote IP or hostname. You will connect over the network and "
             "can remotely trigger captures - any files will be copied back across the network, to "
             "be saved and replayed locally as normal.\n")));

  // Tip 25
  m_tips.push_back(
      Tip(tr("Event browser bookmarks"),
          tr("In the event browser you can bookmark useful events by clicking the asterisk. This "
             "can let you quickly jump back and forth through the log between important "
             "points.\n\nWhen you have some bookmarks, shortcut buttons will appear in a small bar "
             "at the top of the browser, and the shortcut keys Ctrl-1 through Ctrl-0 jump to the "
             "first 10 bookmarks - these shortcuts are global regardless of which RenderDoc window "
             "is currently in focus.\n")));

  // Tip 26
  m_tips.push_back(
      Tip(tr("Mousewheel for scrolling"),
          tr("Anywhere you need to use the mousewheel to scroll, it will work simply by hovering "
             "over the panel and scrolling, no need to click to change focus.\n")));

  // Tip 27
  m_tips.push_back(
      Tip(tr("Event browser keyboard shortcuts"),
          tr("In the event browser Ctrl-F opens up the find bar, to locate an event by its name. "
             "Ctrl-G opens the jump-to-event to jump to the closest drawcall to a numbered event. "
             "Ctrl-B will toggle a bookmark at the current event.\n")));

  // Tip 28
  m_tips.push_back(
      Tip(tr("Mesh VS Output camera settings"),
          tr("The VS Output pane in the mesh viewer will attempt to guess your projection matrix "
             "to unproject the vertices into camera space. It assumes a perspective projection and "
             "guesses the near and far planes, and matches the aspect ratio to the current output "
             "target.\n\nIf these parameters are incorrect - e.g. you are using an orthographic "
             "projection or the near/far guesses are wrong, you can override them by opening the "
             "view settings with the cog icon.\n")));

  // Tip 29
  m_tips.push_back(
      Tip(tr("Global process hook"),
          tr("Sometimes a particular program is difficult to launch directly through RenderDoc. In "
             "these cases, RenderDoc can install a global system hook that will insert a tiny shim "
             "DLL into every newly-created process on the system. This shim will identify if it is "
             "in the target application and either inject RenderDoc, or unload itself.\n\nNote: "
             "Since it is a global hook this is not without risks, only use if it's the only "
             "alternative, and read the documentation carefully.\n")));

  ///////////////////////////////////////////////////////////
  // This section of code is auto-generated. Modifications //
  // will be lost if made by hand.                         //
  //                                                       //
  // If you have a tip you'd like to add, email it to me   //
  // or open an issue on github to suggest it.             //
  ///////////////////////////////////////////////////////////
}

void TipsDialog::showTip(int i)
{
  if(i >= m_tips.size() || i < 0)
    return;

  Tip &tip = m_tips[i];
  ++i;
  ui->tipTextLabel->setText(tip.tip);
  QString url = lit("https://renderdoc.org/tips/%1").arg(i);
  ui->tipUrlLabel->setText(lit("<a href='%1'>%1</a>").arg(url));
  ui->tipsGroupBox->setTitle(tr("Tip #%1").arg(i));
  ui->titleLabel->setText(tr("Tip #%1: %2").arg(i).arg(tip.title));
}

void TipsDialog::showRandomTip()
{
  int i = m_currentTip;
  srand(time(NULL));
  while(i == m_currentTip)
  {
    i = rand() % m_tips.size();
  }
  m_currentTip = i;
  showTip(m_currentTip);
}

void TipsDialog::on_nextButton_clicked()
{
  m_currentTip++;
  m_currentTip %= m_tips.size();
  showTip(m_currentTip);
}

void TipsDialog::on_closeButton_clicked()
{
  close();
}

void TipsDialog::on_randomButton_clicked()
{
  showRandomTip();
}
