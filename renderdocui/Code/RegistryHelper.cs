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
using Microsoft.Win32;

namespace renderdocui.Code
{
    class RegistryHelper
    {
        private RegistryKey subKey;

        public RegistryHelper()
        {
        }

        public void Open(string applicationKey)
        {
            subKey = Registry.CurrentUser.OpenSubKey(applicationKey, true);
        }

        public void Close()
        {
            subKey.Close();
        }

        private bool Read(string keyName, out object result)
        {
            result = null;

            
            if (subKey == null)
            {
                return false;
            }
            else
            {
                try
                {
                    result = subKey.GetValue(keyName);
                    if (result == null)
                        return false;
                }
                catch (Exception e)
                {
                    return false;
                }
            }

            return true;
        }

        private bool Write(string keyName, object value)
        {
            try
            {
                subKey.SetValue(keyName, value);

                return true;
            }
            catch (Exception e)
            {
                return false;
            }
        }

        public bool ReadValueAsString(string keyName, out string result)
        {
            object tmpResult;

            result = null;

            if (!Read(keyName, out tmpResult))
                return false;

            result = tmpResult as string;
            return result != null;
        }

        public bool WriteValueAsString(string keyName, string value)
        {
            return Write(keyName, value);
        }

        public bool ReadValueAsInt(string keyName, out int result)
        {
            object tmpResult;

            result = 0;

            if (!Read(keyName, out tmpResult))
                return false;

            try
            {
                result = Convert.ToInt32(tmpResult);
                return true;
            }
            catch (Exception e)
            {
                return false;
            }
        }

        public bool WriteValueAsInt(string keyName, int value)
        {
            return Write(keyName, value);
        }

        public bool ReadValueAsBool(string keyName, out bool result)
        {
            int tmpResult;

            result = false;

            if (!ReadValueAsInt(keyName, out tmpResult))
                return false;

            result = tmpResult != 0;
            return true;
        }

        public bool WriteValueAsBool(string keyName, bool value)
        {
            return Write(keyName, value ? 1 : 0);
        }
    }
}
