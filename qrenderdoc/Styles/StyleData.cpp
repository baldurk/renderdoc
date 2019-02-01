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

#include "StyleData.h"
#include <QApplication>
#include "Code/QRDUtils.h"
#include "RDStyle/RDStyle.h"
#include "RDTweakedNativeStyle/RDTweakedNativeStyle.h"

namespace StyleData
{
const ThemeDescriptor availStyles[] = {
    ThemeDescriptor(
        lit("RDLight"), QApplication::translate("RDStyle", "Light"),
        QApplication::translate(
            "RDStyle", "Light: Cross-platform custom RenderDoc dark theme (black-on-white)."),
        []() { return new RDStyle(RDStyle::Light); }),

    ThemeDescriptor(
        lit("RDDark"), QApplication::translate("RDStyle", "Dark"),
        QApplication::translate(
            "RDStyle", "Dark: Cross-platform custom RenderDoc dark theme (white-on-black)."),
        []() { return new RDStyle(RDStyle::Dark); }),

    ThemeDescriptor(
        lit("Native"), QApplication::translate("RDStyle", "Native"),
        QApplication::translate("RDStyle",
                                "Native: uses the built-in Qt native widgets for your platform."),
        []() { return new RDTweakedNativeStyle(NULL); }),
};

const int numAvailable = sizeof(availStyles) / sizeof(ThemeDescriptor);
};