/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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
using System.Reflection;

namespace renderdocplugin
{
    class PluginHelpers
    {
        private static Dictionary<string, Assembly> m_LoadedPlugins = null;

        public static String PluginDirectory
        {
            get
            {
                var dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                dir = Path.Combine(dir, "plugins");

                return dir;
            }
        }

        public static List<string> GetPlugins()
        {
            if (m_LoadedPlugins != null)
                return new List<string>(m_LoadedPlugins.Keys);

            m_LoadedPlugins = new Dictionary<string, Assembly>();

            if(!Directory.Exists(PluginDirectory))
                return new List<string>();

            foreach (string pluginFile in Directory.EnumerateFiles(PluginDirectory, "*.dll"))
            {
                try
                {
                    Assembly myDllAssembly = Assembly.LoadFile(pluginFile);

                    var basename = Path.GetFileNameWithoutExtension(pluginFile);

                    m_LoadedPlugins.Add(basename, myDllAssembly);
                }
                catch (Exception)
                {
                    // silently ignore invalid/bad assemblies.
                }
            }

            return new List<string>(m_LoadedPlugins.Keys);
        }

        public static T GetPluginInterface<T>(string assemblyName)
        {
            if (!m_LoadedPlugins.ContainsKey(assemblyName))
                return default(T);

            var type = typeof(T);

            var ass = m_LoadedPlugins[assemblyName];

            return (T)ass.CreateInstance(assemblyName + "." + type.Name);
        }
    }
}
