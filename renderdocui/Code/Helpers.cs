/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
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


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using Microsoft.Win32;
using System.Security.Principal;
using System.Diagnostics;
using System.Xml.Serialization;

namespace renderdocui.Code
{
    static class Helpers
    {
        // simple helpers to wrap a given control in a DockContent, so it can be docked into a panel.
        static public DockContent WrapDockContent(DockPanel panel, Control c)
        {
            return WrapDockContent(panel, c, c.Text);
        }

        static public DockContent WrapDockContent(DockPanel panel, Control c, string Title)
        {
            DockContent w = new DockContent();
            c.Dock = DockStyle.Fill;
            w.Controls.Add(c);
            w.DockAreas &= ~DockAreas.Float;
            w.Text = Title;
            w.DockPanel = panel;

            w.DockHandler.GetPersistStringCallback = new GetPersistStringCallback(() => { return c.Name; });

            Control win = panel as Control;

            while (win != null && !(win is Form))
                win = win.Parent;

            if (win != null && win is Form)
                w.Icon = (win as Form).Icon;

            return w;
        }

        public static T Clamp<T>(this T val, T min, T max) where T : IComparable<T>
        {
            if (val.CompareTo(min) < 0) return min;
            else if (val.CompareTo(max) > 0) return max;
            else return val;
        }

        public static bool IsElevated
        {
            get
            {
                return new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator);
            }
        }

        public static Thread NewThread(ParameterizedThreadStart s)
        {
            Thread ret = new Thread(s);
            ret.CurrentCulture = Application.CurrentCulture;
            return ret;
        }

        public static Thread NewThread(ThreadStart s)
        {
            Thread ret = new Thread(s);
            ret.CurrentCulture = Application.CurrentCulture;
            return ret;
        }

        public static void RefreshAssociations()
        {
            Win32PInvoke.SHChangeNotify(Win32PInvoke.HChangeNotifyEventID.SHCNE_ASSOCCHANGED,
                                        Win32PInvoke.HChangeNotifyFlags.SHCNF_IDLIST |
                                            Win32PInvoke.HChangeNotifyFlags.SHCNF_FLUSHNOWAIT |
                                            Win32PInvoke.HChangeNotifyFlags.SHCNF_NOTIFYRECURSIVE,
                                        IntPtr.Zero, IntPtr.Zero);
        }

        public static void InstallRDCAssociation()
        {
            if (!IsElevated)
            {
                var process = new Process();
                process.StartInfo = new ProcessStartInfo(Application.ExecutablePath, "--registerRDCext");
                process.StartInfo.Verb = "runas";
                try
                {
                    process.Start();
                }
                catch (Exception)
                {
                    // fire and forget - most likely caused by user saying no to UAC prompt
                }
                return;
            }

            var path = Path.GetFullPath(Application.ExecutablePath);

            RegistryKey key = Registry.ClassesRoot.CreateSubKey("RenderDoc.RDCCapture.1");
            key.SetValue("", "RenderDoc Capture Log (.rdc)");
            key.CreateSubKey("shell").CreateSubKey("open").CreateSubKey("command").SetValue("", "\"" + path + "\" \"%1\"");
            key.CreateSubKey("DefaultIcon").SetValue("", path);
            key.CreateSubKey("CLSID").SetValue("", "{5D6BF029-A6BA-417A-8523-120492B1DCE3}");
            key.CreateSubKey("ShellEx").CreateSubKey("{e357fccd-a995-4576-b01f-234630154e96}").SetValue("", "{5D6BF029-A6BA-417A-8523-120492B1DCE3}");
            key.Close();

            key = Registry.ClassesRoot.CreateSubKey(".rdc");
            key.SetValue("", "RenderDoc.RDCCapture.1");
            key.Close();

            var dllpath = Path.Combine(Path.GetDirectoryName(path), "renderdoc.dll");

            key = Registry.ClassesRoot.OpenSubKey("CLSID", true).CreateSubKey("{5D6BF029-A6BA-417A-8523-120492B1DCE3}");
            key.SetValue("", "RenderDoc Thumbnail Handler");
            key.CreateSubKey("InprocServer32").SetValue("", dllpath);
            key.Close();

            RefreshAssociations();
        }

        public static void InstallCAPAssociation()
        {
            if (!IsElevated)
            {
                var process = new Process();
                process.StartInfo = new ProcessStartInfo(Application.ExecutablePath, "--registerCAPext");
                process.StartInfo.Verb = "runas";
                try
                {
                    process.Start();
                }
                catch (Exception)
                {
                    // fire and forget - most likely caused by user saying no to UAC prompt
                }
                return;
            }

            var path = Path.GetFullPath(Application.ExecutablePath);

            RegistryKey key = Registry.ClassesRoot.CreateSubKey("RenderDoc.RDCSettings.1");
            key.SetValue("", "RenderDoc Capture Settings (.cap)");
            key.CreateSubKey("DefaultIcon").SetValue("", path);
            key.CreateSubKey("shell").CreateSubKey("open").CreateSubKey("command").SetValue("", "\"" + path + "\" \"%1\"");
            key.Close();

            key = Registry.ClassesRoot.CreateSubKey(".cap");
            key.SetValue("", "RenderDoc.RDCSettings.1");
            key.Close();

            RefreshAssociations();
        }
    }

    // KeyValuePair isn't serializable, so we make our own that is
    [Serializable]
    public struct SerializableKeyValuePair<K, V>
    {
        public SerializableKeyValuePair(K k, V v) : this() { Key = k; Value = v; }

        public K Key
        { get; set; }

        public V Value
        { get; set; }
    }
}
