/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#ifndef TOOLWINDOWMANAGERSPLITTER_H
#define TOOLWINDOWMANAGERSPLITTER_H

#include <QSplitter>

/*!
 * \brief The ToolWindowManagerSplitter class is a splitter that tweaks how sizes are allocated in
 * children when a child is removed.
 */
class ToolWindowManagerSplitter : public QSplitter
{
  Q_OBJECT
public:
  //! Creates new tab bar.
  explicit ToolWindowManagerSplitter(QWidget *parent = 0);
  //! Destroys the tab bar.
  virtual ~ToolWindowManagerSplitter();

protected:
  //! Reimplemented from QSplitter to share excess space differently.
  void childEvent(QChildEvent *) Q_DECL_OVERRIDE;
};

#endif    // TOOLWINDOWMANAGERSPLITTER_H
