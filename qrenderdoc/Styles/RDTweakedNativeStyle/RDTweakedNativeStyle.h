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

#pragma once

#include <QPalette>
#include <QProxyStyle>

class RDTweakedNativeStyle : public QProxyStyle
{
private:
  Q_OBJECT
public:
  RDTweakedNativeStyle(QStyle *parent);
  ~RDTweakedNativeStyle();

  virtual QRect subElementRect(SubElement element, const QStyleOption *option,
                               const QWidget *widget) const override;
  virtual QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                 const QWidget *widget) const override;
  virtual int pixelMetric(PixelMetric metric, const QStyleOption *option = NULL,
                          const QWidget *widget = NULL) const override;
  virtual int styleHint(StyleHint stylehint, const QStyleOption *opt = NULL,
                        const QWidget *widget = NULL,
                        QStyleHintReturn *returnData = NULL) const override;
  virtual QIcon standardIcon(StandardPixmap standardIcon, const QStyleOption *option = NULL,
                             const QWidget *widget = NULL) const override;
  virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                  QPainter *painter, const QWidget *widget = NULL) const override;
  virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                             QPainter *painter, const QWidget *widget = NULL) const override;
  virtual void drawControl(ControlElement control, const QStyleOption *option, QPainter *painter,
                           const QWidget *widget = NULL) const override;

protected:
};
