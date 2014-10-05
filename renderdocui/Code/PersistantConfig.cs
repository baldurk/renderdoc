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
using System.Xml;
using System.Xml.Serialization;
using renderdoc;
using System.Windows.Forms;

namespace renderdocui.Code
{
    [Serializable]
    public class PersistantConfig
    {
        public string LastLogPath = "";
        public List<string> RecentLogFiles = new List<string>();
        public string LastCapturePath = "";
        public List<string> RecentCaptureSettings = new List<string>();
        public int CallstackLevelSkip = 0;

        public string CaptureSavePath = "";

        public bool TextureViewer_ResetRange = false;
        public bool TextureViewer_DisableThumbnails = false;
        public bool ShaderViewer_FriendlyNaming = true;

        public List<string> RecentHosts = new List<string>();

        public int LocalProxy = 0;

        [XmlIgnore] // not directly serializable
        public Dictionary<string, string> ReplayHosts = new Dictionary<string, string>();
        public List<SerializableKeyValuePair<string, string>> ReplayHostKeyValues = new List<SerializableKeyValuePair<string, string>>();

        public List<SerializableKeyValuePair<string, string>> PreviouslyUsedHosts = new List<SerializableKeyValuePair<string, string>>();

        public enum TimeUnit
        {
            Seconds = 0,
            Milliseconds,
            Microseconds,
            Nanoseconds,
        };

        public static String UnitPrefix(TimeUnit t)
        {
            if (t == TimeUnit.Seconds)
                return "s";
            else if (t == TimeUnit.Milliseconds)
                return "ms";
            else if (t == TimeUnit.Microseconds)
                return "µs";
            else if (t == TimeUnit.Nanoseconds)
                return "ns";

            return "s";
        }

        public TimeUnit EventBrowser_TimeUnit = TimeUnit.Microseconds;
        public bool EventBrowser_HideEmpty = false;

        public int Formatter_MinFigures = 2;
        public int Formatter_MaxFigures = 5;
        public int Formatter_NegExp = 5;
        public int Formatter_PosExp = 7;

        public bool CheckUpdate_AllowChecks = true;
        public bool CheckUpdate_UpdateAvailable = false;
        public DateTime CheckUpdate_LastUpdate = new DateTime(2012, 06, 27);

        public bool AllowGlobalHook = false;

        public void SetupFormatter()
        {
            Formatter.MinFigures = Formatter_MinFigures;
            Formatter.MaxFigures = Formatter_MaxFigures;
            Formatter.ExponentialNegCutoff = Formatter_NegExp;
            Formatter.ExponentialPosCutoff = Formatter_PosExp;
        }

        public void AddRecentFile(List<string> recentList, string file, int maxItems)
        {
            if (!recentList.Contains(Path.GetFullPath(file)))
            {
                recentList.Add(Path.GetFullPath(file));
                if (recentList.Count >= maxItems)
                    recentList.RemoveAt(0);
            }
            else
            {
                recentList.Remove(Path.GetFullPath(file));
                recentList.Add(Path.GetFullPath(file));
            }
        }

        public PersistantConfig()
        {
            CallstackLevelSkip = 0;
            RecentLogFiles.Clear();
            RecentCaptureSettings.Clear();
        }

        public bool ReadOnly = false;

        public void Serialize(string file)
        {
            if (ReadOnly) return;

            try
            {
                ReplayHostKeyValues.Clear();
                foreach (var kv in ReplayHosts)
                    ReplayHostKeyValues.Add(new SerializableKeyValuePair<string, string>(kv.Key, kv.Value));

                XmlSerializer xs = new XmlSerializer(this.GetType());
                StreamWriter writer = File.CreateText(file);
                xs.Serialize(writer, this);
                writer.Flush();
                writer.Close();
            }
            catch (System.IO.IOException ex)
            {
                // Can't recover, but let user know that we couldn't save their settings.
                MessageBox.Show(String.Format("Error saving config file: {1}\n{0}", file, ex.Message));
            }
        }

        public static PersistantConfig Deserialize(string file)
        {
            XmlSerializer xs = new XmlSerializer(typeof(PersistantConfig));
            StreamReader reader = File.OpenText(file);
            PersistantConfig c = (PersistantConfig)xs.Deserialize(reader);
            reader.Close();

            foreach (var kv in c.ReplayHostKeyValues)
            {
                if (kv.Key != null && kv.Key.Length > 0 &&
                    kv.Value != null)
                c.ReplayHosts.Add(kv.Key, kv.Value);
            }

            return c;
        }
    }
}
