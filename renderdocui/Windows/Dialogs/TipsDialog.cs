using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;

namespace renderdocui.Windows.Dialogs
{
    public partial class TipsDialog : Form
    {
        struct Tip
        {
            public Tip(string tt, string tx)
            {
                title = tt;
                text = tx;
            }

            public string title;
            public string text;
        }
        private List<Tip> m_Tips = new List<Tip>();

        private Core m_Core;

        private Random m_Rand = new Random();
        private int m_CurrentTip = 0;

        public TipsDialog(Core core)
        {
            InitializeComponent();

            m_Core = core;

            Icon = global::renderdocui.Properties.Resources.icon;

            ///////////////////////////////////////////////////////////
            // This section of code is auto-generated. Modifications //
            // will be lost if made by hand.                         //
            //                                                       //
            // If you have a tip you'd like to add, email it to me.  //
            ///////////////////////////////////////////////////////////

            // Tip 1
            m_Tips.Add(new Tip(
                "Talk to me!",
                "RenderDoc is a labour of love and has been developed from scratch entirely in my spare time. If you run into a bug, have a feature request or just have a question, please feel free to get in touch and I'm always happy to talk and help out in any way I can - baldurk@baldurk.org."));

            // Tip 2
            m_Tips.Add(new Tip(
                "Quick channel toggling",
                "Right clicking on a channel button in the texture viewer will select it alone. If it is already the only channel selected, the meaning is inverted and all others will be selected. " + Environment.NewLine + " " + Environment.NewLine + "This is most useful for quickly toggling between RGB and Alpha-only views."));

            // Tip 3
            m_Tips.Add(new Tip(
                "Quick range autofitting",
                "The magic wand auto-fitting button in the texture viewer automatically calculates the min and max values of any visible channel in the texture, and rescales the visible range to include them. " + Environment.NewLine + " " + Environment.NewLine + "If you right click on it, it will toggle on a mode to automatically rescale every time the viewed texture changes. " + Environment.NewLine + ""));

            // Tip 4
            m_Tips.Add(new Tip(
                "Choosing mesh elements to visualise",
                "In the mesh output pane, you can right click on any of the element columns to select that element as either position or secondary property to render. This way you can view a mesh in UV space, or visualise normals."));

            // Tip 5
            m_Tips.Add(new Tip(
                "Visualising secondary mesh properties",
                "In the mesh output pane, you can select an element as 'secondary', and in the solid shading dropdown choosing secondary will display the element as RGB colour on top of the mesh."));

            // Tip 6
            m_Tips.Add(new Tip(
                "Register highlighting in the shader debugger",
                "While debugging a shader, clicking on a register or constant buffer variable in the disassembly will highlight all other uses of that register or variable in the disassembly. It will also highlight the matching row in the watch windows."));

            // Tip 7
            m_Tips.Add(new Tip(
                "Shader register value display",
                "If you want to see to see a register reinterpreted as different types, you can hover over it either in the disassembly or in the watch windows. A tooltip will show it interpreted as float, uint decimal, int decimal or hexadecimal."));

            // Tip 8
            m_Tips.Add(new Tip(
                "Custom shader watch expressions",
                "In addition to the pre-filled watch windows while shader debugging, you can also enter custom watch expressions. This takes the form of e.g. r0.xyz. You can append ,x or ,b to specify the type - full list in the docs. " + Environment.NewLine + ""));

            // Tip 9
            m_Tips.Add(new Tip(
                "Shader debugger float/int toggle",
                "By default register values are shown as floats, but you can toggle this to default to ints either in the shader debugger toolbar, or by right clicking and toggling int/float display. " + Environment.NewLine + ""));

            // Tip 10
            m_Tips.Add(new Tip(
                "D3D11 shader debug information",
                "You'll get the best results in RenderDoc by stripping as little as possible from D3D11 shaders. Reflection data is used all over the place to produce a nicer debugging experience. " + Environment.NewLine + ""));

            // Tip 11
            m_Tips.Add(new Tip(
                "Shader editing & Replacement",
                "RenderDoc has the ability to edit and replace shaders and see the results live in the replay. On the pipeline state view, click the edit icon next to the shader. If source is available, it will be compiled, otherwise an empty stub with resources will be generated. " + Environment.NewLine + " " + Environment.NewLine + "The shader will be replaced everywhere it is used in the frame, the original will be restored when the edit window is closed. " + Environment.NewLine + ""));

            // Tip 12
            m_Tips.Add(new Tip(
                "Linear/Gamma display of textures",
                "RenderDoc interprets all textures in gamma space - even if the data is linear such as a normal map. This is by convention, since typically external image viewers will display a normal map as gamma data. This can be overridden by toggling the gamma button in the texture viewer. " + Environment.NewLine + ""));

            // Tip 13
            m_Tips.Add(new Tip(
                "Seeing texture usage in a capture",
                "RenderDoc has a list of how each texture is bound and used - whether as a shader resource, an output target, or a copy source. When the texture is active in the texture viewer this usage will be displayed on the timeline bar at the top. " + Environment.NewLine + " " + Environment.NewLine + "You can also right click on the thumbnails in the texture viewer to see a list of this usage, and clicking any entry will jump to that event. " + Environment.NewLine + ""));

            // Tip 14
            m_Tips.Add(new Tip(
                "Custom buffer formatting",
                "When opening a raw view of a buffer, such as a vertex buffer or compute read/write buffer resource, you can apply custom formatting to it to dictate the layout of the elements in typical shader syntax. " + Environment.NewLine + " " + Environment.NewLine + "This formatting is also available for constant buffers to override the layout reflected from the shader. " + Environment.NewLine + ""));

            // Tip 15
            m_Tips.Add(new Tip(
                "Pipeline HTML export",
                "The pipeline view contains an HTML export function, which dumps the raw state of the entire pipeline out to a specified file. This can be useful for comparison between two events, or for having all information available in a unified text format. " + Environment.NewLine + ""));

            // Tip 16
            m_Tips.Add(new Tip(
                "Python scripting",
                "RenderDoc supports some amount of Python scripting. Open up the Python shell in the UI to either use it interactively or load and execute python scripts. " + Environment.NewLine + " " + Environment.NewLine + "The 'renderdoc' object is an instance of the Core class - see the RenderDoc source for more information."));

            // Tip 17
            m_Tips.Add(new Tip(
                "Pixel history view",
                "RenderDoc supports a pixel history view, showing the list of all modification events that happened to a specified pixel. To launch it, simply pick the pixel you would like to view the history of in the texture viewer, and click the 'history' button underneath the zoomed-in pixel context. " + Environment.NewLine + " " + Environment.NewLine + "Each event will show up red or green depending on whether it affected or didn't affect the pixel. By expanding the event, you can see the possibly several primitives within the draw that overdrew the pixel. " + Environment.NewLine + ""));

            // Tip 18
            m_Tips.Add(new Tip(
                "List of active textures",
                "On the texture viewer, the texture list button under the 'Actions' section will open a filterable list of all texture resources. Clicking on any of the entries will open a locked tab of that texture, always showing the contents of the texture at the current event regardless of whether or not it is bound to the pipeline. " + Environment.NewLine + ""));

            // Tip 19
            m_Tips.Add(new Tip(
                "Locked texture tabs",
                "You can open a locked texture tab from the texture list, or by right or double clicking on a texture's thumbnail. " + Environment.NewLine + " " + Environment.NewLine + "With a locked tab of a texture, that tab will always show that texture, regardless of what is bound to the pipeline. This way you can track the updates of a texture through binds and unbinds, e.g. ping-pong rendertarget use. " + Environment.NewLine + ""));

            // Tip 20
            m_Tips.Add(new Tip(
                "Gathering of per-event callstacks",
                "RenderDoc is able to gather callstacks either per-drawcall or per-API event. You can do this by enabling the option before launching an application capture. " + Environment.NewLine + " " + Environment.NewLine + "When loading the log, initially the callstacks will not be available until symbols are resolved. Go to tools -> resolve symbols to load up the pdbs matching the modules from the application. " + Environment.NewLine + ""));

            // Tip 21
            m_Tips.Add(new Tip(
                "Texture debugging overlays",
                "In the texture viewer, you can select from several helpful debugging overlays over the current view. This can show wireframe or solid coloour overlays of the current drawcall, as well as showing depth pass/fail or even representing quad overdraw as a heatmap. " + Environment.NewLine + ""));

            // Tip 22
            m_Tips.Add(new Tip(
                "Custom texture display shaders",
                "RenderDoc supports writing custom shaders to decode the viewed texture, which can be useful to e.g. colourise stencil values or decode a packed gbuffer texture. " + Environment.NewLine + " " + Environment.NewLine + "In the toolbar in the texture viewer, select custom instead of RGBA on the left side, and use the UI to create a new shader. The docs contain full listings of available constants and resources to bind. " + Environment.NewLine + ""));

            // Tip 23
            m_Tips.Add(new Tip(
                "Texture histogram",
                "RenderDoc can display a channel histogram showing the distribution of values within the visible range. Simply click the graph button on the texture viewer to the right of the range control, and it will expand to show the histogram. " + Environment.NewLine + ""));

            // Tip 24
            m_Tips.Add(new Tip(
                "Attaching to a running instance",
                "If you have launched a program via RenderDoc or your program integrates RenderDoc, the UI can connect to it once it is running via File -> Attach to Running Instance, and everything works as if you had launched it. " + Environment.NewLine + " " + Environment.NewLine + "You can even do this across a network, by adding a remote IP or hostname. You will connect over the network and can remotely trigger captures - any files will be copied back across the network, to be saved and replayed locally as normal. " + Environment.NewLine + ""));

            // Tip 25
            m_Tips.Add(new Tip(
                "Event browser bookmarks",
                "In the event browser you can bookmark useful events by clicking the asterisk. This can let you quickly jump back and forth through the log between important points. " + Environment.NewLine + " " + Environment.NewLine + "When you have some bookmarks, shortcut buttons will appear in a small bar at the top of the browser, and the shortcut keys Ctrl-1 through Ctrl-0 jump to the first 10 bookmarks - these shortcuts are global regardless of which RenderDoc window is currently in focus. " + Environment.NewLine + ""));

            // Tip 26
            m_Tips.Add(new Tip(
                "Mousewheel for scrolling",
                "Anywhere you need to use the mousewheel to scroll, it will work simply by hovering over the panel and scrolling, no need to click to change focus. " + Environment.NewLine + ""));

            // Tip 27
            m_Tips.Add(new Tip(
                "Event browser keyboard shortcuts",
                "In the event browser Ctrl-F opens up the find bar, to locate an event by its name. Ctrl-G opens the jump-to-event to jump to the closest drawcall to a numbered event. Ctrl-B will toggle a bookmark at the current event. " + Environment.NewLine + ""));

            // Tip 28
            m_Tips.Add(new Tip(
                "Mesh VS Output camera settings",
                "The VS Output pane in the mesh output window will attempt to guess your projection matrix to unproject the vertices into camera space. It assumes a perspective projection and guesses the near and far planes, and matches the aspect ratio to the current output target. " + Environment.NewLine + " " + Environment.NewLine + "If these parameters are incorrect - e.g. you are using an orthographic projection or the near/far guesses are wrong, you can override them by opening the view settings with the cog icon. " + Environment.NewLine + ""));

            // Tip 29
            m_Tips.Add(new Tip(
                "Global process hook",
                "Sometimes a particular program is difficult to launch directly through RenderDoc. In these cases, RenderDoc can install a global system hook that will insert a tiny shim DLL into every newly-created process on the system. This shim will identify if it is in the target application and either inject RenderDoc, or unload itself. " + Environment.NewLine + " " + Environment.NewLine + "Note: Since it is a global hook this is not without risks, only use if it's the only alternative, and read the documentation carefully. " + Environment.NewLine + ""));

            ///////////////////////////////////////////////////////////
            // This section of code is auto-generated. Modifications //
            // will be lost if made by hand.                         //
            //                                                       //
            // If you have a tip you'd like to add, email it to me.  //
            ///////////////////////////////////////////////////////////
        }

        private void LoadRandomTip(object sender, EventArgs e)
        {
            int cur = m_CurrentTip;

            // ensure we at least switch to a different tip
            while(m_CurrentTip == cur)
                m_CurrentTip = m_Rand.Next(m_Tips.Count);

            // the first time, show the first tip about contacting me
            if (!m_Core.Config.Tips_SeenFirst)
            {
                m_Core.Config.Tips_SeenFirst = true;
                m_CurrentTip = 0;
            }

            LoadTip();
        }

        private void LoadTip()
        {
            Tip tip = m_Tips[m_CurrentTip];

            tipBox.Text = String.Format("Tip #{0}", m_CurrentTip + 1);
            tipTitle.Text = String.Format("Tip #{0}: {1}", m_CurrentTip + 1, tip.title);
            tipText.Text = tip.text;
            tipLink.Text = String.Format("https://renderdoc.org/tips/{0}", m_CurrentTip + 1);

            tipPicture.Image = null;
            tipPicture.Visible = false;
        }

        private void LoadNextTip(object sender, EventArgs e)
        {
            m_CurrentTip++;

            if (m_CurrentTip >= m_Tips.Count)
                m_CurrentTip = 0;

            LoadTip();
        }

        private void tipLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            Process.Start(tipLink.Text);
        }
    }
}
