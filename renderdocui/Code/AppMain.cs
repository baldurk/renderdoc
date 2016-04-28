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
using System.Windows.Forms;
using System.Text.RegularExpressions;
using renderdoc;

namespace renderdocui.Code
{
    class AppMain
    {
        [System.Runtime.ExceptionServices.HandleProcessCorruptedStateExceptions] 
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            Application.ThreadException += new System.Threading.ThreadExceptionEventHandler(Application_ThreadException);
            AppDomain.CurrentDomain.UnhandledException += new UnhandledExceptionEventHandler(CurrentDomain_UnhandledException);
            Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);

            // command line arguments that we can call when we temporarily elevate the process
            if(args.Contains("--registerRDCext"))
            {
                Helpers.InstallRDCAssociation();
                return;
            }

            if(args.Contains("--registerCAPext"))
            {
                Helpers.InstallCAPAssociation();
                return;
            }

            if (args.Contains("--registerVKLayer"))
            {
                Helpers.RegisterVulkanLayer();
                return;
            }

            Win32PInvoke.LoadLibrary("renderdoc.dll");

            // clean up any update that just happened
            string updateFilesPath = Path.Combine(Path.GetTempPath(), "RenderDocUpdate");

            try
            {
                if (Directory.Exists(updateFilesPath))
                    Directory.Delete(updateFilesPath, true);
            }
            catch (Exception)
            {
                // ignore any exceptions from this
            }

            string filename = "";

            bool temp = false;

            // not real command line argument processing, but allow an argument to indicate we're being passed
            // a temporary filename that we should take ownership of to delete when we're done (if the user doesn't
            // save it)
            foreach(var a in args)
            {
                if(a.ToUpperInvariant() == "--TEMPFILE")
                    temp = true;
            }

            string remoteHost = "";
            uint remoteIdent = 0;

            for (int i = 0; i < args.Length; i++)
            {
                if (args[i].ToUpperInvariant() == "--REMOTEACCESS" && i + 1 < args.Length)
                {
                    var regexp = @"^([a-zA-Z0-9_-]+:)?([0-9]+)$";

                    var match = Regex.Match(args[i+1], regexp);

                    if (match.Success)
                    {
                        var host = match.Groups[1].Value;
                        if (host.Length > 0 && host[host.Length - 1] == ':')
                            host = host.Substring(0, host.Length - 1);
                        uint ident = 0;
                        if (uint.TryParse(match.Groups[2].Value, out ident))
                        {
                            remoteHost = host;
                            remoteIdent = ident;
                        }
                    }
                }
            }

            if (args.Length > 0 && File.Exists(args[args.Length - 1]))
            {
                filename = args[args.Length - 1];
            }

            var cfg = new PersistantConfig();

            // load up the config from user folder, handling errors if it's malformed and falling back to defaults
            if (File.Exists(Core.ConfigFilename))
            {
                try
                {
                    cfg = PersistantConfig.Deserialize(Core.ConfigFilename);
                }
                catch (System.Xml.XmlException)
                {
                    MessageBox.Show(String.Format("Error loading config file\n{0}\nA default config is loaded and will be saved out.", Core.ConfigFilename));
                }
                catch (System.InvalidOperationException)
                {
                    MessageBox.Show(String.Format("Error loading config file\n{0}\nA default config is loaded and will be saved out.", Core.ConfigFilename));
                }
                catch (System.IO.IOException ex)
                {
                    MessageBox.Show(String.Format("Error loading config file: {1}\n{0}\nA default config is loaded and will be saved out.", Core.ConfigFilename, ex.Message));
                }
            }

            // propogate float formatting settings to the Formatter class used globally to format float values
            cfg.SetupFormatting();

            Application.CurrentCulture = new System.Globalization.CultureInfo("en-GB");

            var core = new Core(filename, remoteHost, remoteIdent, temp, cfg);

            foreach (var a in args)
            {
                if (a.ToUpperInvariant() == "--UPDATEDONE")
                {
                    cfg.CheckUpdate_UpdateAvailable = false;
                    cfg.CheckUpdate_UpdateResponse = "";

                    bool hasOtherJSON;
                    bool thisRegistered;
                    string[] otherJSONs;

                    bool configured = Helpers.CheckVulkanLayerRegistration(out hasOtherJSON, out thisRegistered, out otherJSONs);

                    // if nothing is configured (ie. no other JSON files), then set up our layer
                    // as part of the update process.
                    if (!configured && !hasOtherJSON && !thisRegistered)
                    {
                        Helpers.RegisterVulkanLayer();
                    }

                    Helpers.UpdateInstalledVersionNumber();
                }
            }

            try
            {
                Application.Run(core.AppWindow);
            }
            catch (Exception e)
            {
                HandleException(e);
            }

            cfg.Serialize(Core.ConfigFilename);
        }

        static void LogException(Exception ex)
        {
            StaticExports.LogText(ex.ToString());

            if (ex.InnerException != null)
            {
                StaticExports.LogText("InnerException:");
                LogException(ex.InnerException);
            }
        }

        static void HandleException(Exception ex)
        {
            // we log out this string, which is matched against in renderdoccmd to pull out the callstack
            // from the log even in the case where the user chooses not to submit the error log
            StaticExports.LogText("\n\n");
            StaticExports.LogText("--- Begin C# Exception Data ---");
            if (ex != null)
            {
                LogException(ex);

                StaticExports.TriggerExceptionHandler(System.Runtime.InteropServices.Marshal.GetExceptionPointers(), true);
            }
            else
            {
                StaticExports.LogText("Exception is NULL");

                StaticExports.TriggerExceptionHandler(IntPtr.Zero, true);
            }

            System.Diagnostics.Process.GetCurrentProcess().Kill();
        }

        static void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            if (e.ExceptionObject is Exception)
                HandleException(e.ExceptionObject as Exception);
            else
                HandleException(null);
        }

        static void Application_ThreadException(object sender, System.Threading.ThreadExceptionEventArgs e)
        {
            HandleException(e.Exception);
        }
    }
}
