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

namespace renderdoc
{
    public class Formatter
    {
        public static String Format(double f)
        {
            if (f != 0 && (Math.Abs(f) < m_ExponentialNegValue || Math.Abs(f) > m_ExponentialPosValue))
                return String.Format(m_EFormatter, f);

            return String.Format(m_FFormatter, f);
        }

        public static String Format(float f)
        {
            if (f != 0 && (Math.Abs(f) < m_ExponentialNegValue || Math.Abs(f) > m_ExponentialPosValue))
                return String.Format(m_EFormatter, f);

            return String.Format(m_FFormatter, f);
        }

        public static String Format(UInt32 u)
        {
            return String.Format("{0}", u);
        }

        public static String Format(UInt32 u, bool hex)
        {
            return String.Format(hex ? "{0:X8}" : "{0}", u);
        }

        public static String Format(Int32 i)
        {
            return String.Format("{0}", i);
        }

        public static int MaxFigures
        {
            get
            {
                return m_MaxFigures;
            }

            set
            {
                if (value >= 2)
                    m_MaxFigures = value;
                else
                    m_MaxFigures = 2;

                UpdateFormatters();
            }
        }

        public static int MinFigures
        {
            get
            {
                return m_MinFigures;
            }

            set
            {
                if (value >= 0)
                    m_MinFigures = value;
                else
                    m_MinFigures = 0;

                UpdateFormatters();
            }
        }

        public static int ExponentialNegCutoff
        {
            get
            {
                return m_ExponentialNegCutoff;
            }

            set
            {
                if (value >= 0)
                    m_ExponentialNegCutoff = value;
                else
                    m_ExponentialNegCutoff = 0;

                m_ExponentialNegValue = Math.Pow(10.0, -m_ExponentialNegCutoff);
            }
        }

        public static int ExponentialPosCutoff
        {
            get
            {
                return m_ExponentialPosCutoff;
            }

            set
            {
                if (value >= 0)
                    m_ExponentialPosCutoff = value;
                else
                    m_ExponentialPosCutoff = 0;

                m_ExponentialPosValue = Math.Pow(10.0, m_ExponentialPosCutoff);
            }
        }

        private static int m_MinFigures = 2;
        private static int m_MaxFigures = 5;
        private static int m_ExponentialNegCutoff = 5;
        private static int m_ExponentialPosCutoff = 7;

        private static double m_ExponentialNegValue = 0.00001; // 10(-5)
        private static double m_ExponentialPosValue = 10000000.0; // 10(7)
        private static string m_EFormatter = "{0:E5}";
        private static string m_FFormatter = "{0:0.00###}";

        private static void UpdateFormatters()
        {
            m_FFormatter = "{0:0.";

            int i = 0;

            for (; i < m_MinFigures; i++) m_FFormatter += "0";
            for (; i < m_MaxFigures; i++) m_FFormatter += "#";

            m_EFormatter = m_FFormatter + "e+00}";

            m_FFormatter += "}";
        }
    };
}
