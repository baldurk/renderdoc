/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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

        public static float Area(this System.Drawing.PointF val)
        {
            return val.X * val.Y;
        }
        public static float Aspect(this System.Drawing.PointF val)
        {
            return val.X / val.Y;
        }

        public static uint AlignUp(this uint x, uint a)
        {
            return (x + (a - 1)) & (~(a - 1));
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

        public static string GetVulkanJSONPath(bool wow6432)
        {
            string basepath = Win32PInvoke.GetUniversalName(Path.GetDirectoryName(Application.ExecutablePath));
            if (wow6432)
                basepath = Path.Combine(basepath, "x86");

            return Path.Combine(basepath, "renderdoc.json");
        }

        private static RegistryKey GetVulkanImplicitLayersKey(bool write, bool wow6432)
        {
            try
            {
                string basepath = "SOFTWARE\\";
                if (wow6432)
                    basepath += "Wow6432Node\\";

                if(write)
                    return Registry.LocalMachine.CreateSubKey(basepath + "Khronos\\Vulkan\\ImplicitLayers");
                else
                    return Registry.LocalMachine.OpenSubKey(basepath + "Khronos\\Vulkan\\ImplicitLayers");
            }
            catch (Exception)
            {
            }
            return null;
        }

        public static bool CheckVulkanLayerRegistration(out bool hasOtherJSON, out bool thisRegistered, out string[] otherJSONs)
        {
            RegistryKey key = GetVulkanImplicitLayersKey(false, false);

            // if we couldn't even get the ImplicitLayers reg key the system doesn't have the
            // vulkan runtime, so we return as if we are not registered (as that's the case).
            // People not using vulkan can either ignore the message, or click to set it up
            // and it will go away as we'll have rights to create it.
            if (key == null)
            {
                hasOtherJSON = false;
                otherJSONs = new string[] { };
                thisRegistered = false;
                return false;
            }

            string myJSON = Path.GetFullPath(GetVulkanJSONPath(false));

            string[] names = key.GetValueNames();

            // defaults
            thisRegistered = false;
            hasOtherJSON = false;

            List<string> others = new List<string>();

            foreach (var n in names)
            {
                if(String.Compare(Path.GetFullPath(n), myJSON, StringComparison.CurrentCultureIgnoreCase) == 0)
                {
                    thisRegistered = true;
                }
                else if(n.IndexOf("renderdoc.json") > 0)
                {
                    hasOtherJSON = true;
                    others.Add(Path.GetFullPath(n));
                }
            }

            // if we're 64-bit update that too. For 32-bit the above path covers it.
            if (Environment.Is64BitProcess)
            {
                myJSON = Path.GetFullPath(GetVulkanJSONPath(true));
                key = GetVulkanImplicitLayersKey(false, true);

                if (key == null)
                {
                    thisRegistered = false;
                }
                else
                {
                    names = key.GetValueNames();

                    foreach (var n in names)
                    {
                        if (String.Compare(Path.GetFullPath(n), myJSON, StringComparison.CurrentCultureIgnoreCase) == 0)
                        {
                            thisRegistered = true;
                        }
                        else if (n.IndexOf("renderdoc.json") > 0)
                        {
                            hasOtherJSON = true;
                            others.Add(Path.GetFullPath(n));
                        }
                    }
                }
            }

            if(hasOtherJSON)
                otherJSONs = others.ToArray();
            else
                otherJSONs = new string[] {};

            // return true if all is OK
            return !hasOtherJSON && thisRegistered;
        }

        public static bool CheckVulkanLayerRegistration()
        {
            bool dummy1, dummy2;
            string[] dummy3;
            return CheckVulkanLayerRegistration(out dummy1, out dummy2, out dummy3);
        }

        public static void UpdateInstalledVersionNumber()
        {
            if (!IsElevated)
                return;

            try
            {
                string basepath = "SOFTWARE\\";

                RegistryKey key = Registry.LocalMachine.CreateSubKey(basepath + "Microsoft\\Windows\\CurrentVersion\\Uninstall");

                string[] subkeys = key.GetSubKeyNames();

                foreach (var sub in subkeys)
                {
                    RegistryKey prog = key.CreateSubKey(sub);

                    string[] values = prog.GetValueNames();

                    if (Array.IndexOf(values, "DisplayName") >= 0 &&
                        (string)prog.GetValue("DisplayName") == "RenderDoc" &&
                        Array.IndexOf(values, "Publisher") >= 0 &&
                        (string)prog.GetValue("Publisher") == "Baldur Karlsson")
                    {
                        var ver = System.Reflection.Assembly.GetEntryAssembly().GetName().Version;
                        uint majorversion = (uint)ver.Major;
                        uint minorversion = (uint)ver.Minor;
                        uint packedversion = (majorversion << 24) | (minorversion << 16);

                        prog.SetValue("Version", packedversion, RegistryValueKind.DWord);
                        prog.SetValue("VersionMajor", majorversion, RegistryValueKind.DWord);
                        prog.SetValue("VersionMinor", minorversion, RegistryValueKind.DWord);
                        prog.SetValue("DisplayVersion", String.Format("{0}.{1}.0", majorversion, minorversion), RegistryValueKind.String);
                    }
                }
            }
            catch (Exception)
            {
            }
        }

        public static void RegisterVulkanLayer()
        {
            if (!IsElevated)
            {
                var process = new Process();
                process.StartInfo = new ProcessStartInfo(Application.ExecutablePath, "--registerVKLayer");
                process.StartInfo.Verb = "runas";
                try
                {
                    process.Start();
                    // wait for process to finish
                    process.WaitForExit();
                }
                catch (Exception)
                {
                }
                return;
            }

            // we know we're elevated, so open the key for write
            RegistryKey key = GetVulkanImplicitLayersKey(true, false);

            if (key != null)
            {
                string[] names = key.GetValueNames();

                // for simplicity we just delete *all* renderdoc.json values, then
                // add our own, even if it was there before.
                foreach (var n in names)
                    if (n.IndexOf("renderdoc.json") > 0)
                        key.DeleteValue(n);

                key.SetValue(GetVulkanJSONPath(false), (uint)0, RegistryValueKind.DWord);
            }

            // if we're 64-bit update that too. For 32-bit the above path covers it.
            if (Environment.Is64BitProcess)
            {
                key = GetVulkanImplicitLayersKey(true, true);

                if (key != null)
                {
                    string[] names = key.GetValueNames();

                    // for simplicity we just delete *all* renderdoc.json values, then
                    // add our own, even if it was there before.
                    foreach (var n in names)
                        if (n.IndexOf("renderdoc.json") > 0)
                            key.DeleteValue(n);

                    key.SetValue(GetVulkanJSONPath(true), (uint)0, RegistryValueKind.DWord);
                }
            }
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
