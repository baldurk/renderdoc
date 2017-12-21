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
#include "ToolWindowManagerSplitter.h"
#include <QChildEvent>
#include <QDebug>

ToolWindowManagerSplitter::ToolWindowManagerSplitter(QWidget *parent) : QSplitter(parent)
{
}

ToolWindowManagerSplitter::~ToolWindowManagerSplitter()
{
}

void ToolWindowManagerSplitter::childEvent(QChildEvent *event)
{
  QList<int> s = sizes();

  QWidget *w = qobject_cast<QWidget *>(event->child());
  int idx = -1;
  if(w)
    idx = indexOf(w);

  QSplitter::childEvent(event);

  if(event->type() == QEvent::ChildRemoved && idx >= 0 && idx < s.count())
  {
    int removedSize = s[idx];

    s.removeAt(idx);

    // if we removed an item at one extreme or another, the new end should get all the space
    // (unless the list is now empty)
    if(idx == 0)
    {
      if(!s.isEmpty())
        s[0] += removedSize;
    }
    else if(idx == s.count())
    {
      if(!s.isEmpty())
        s[s.count() - 1] += removedSize;
    }
    else
    {
      // we removed an item in the middle, share the space between its previous neighbours, now in
      // [idx-1] and [idx], and we know they're valid since if there were only two elements before
      // the removal one or the other case above would have matched. So there are at least two
      // elements now and idx > 0

      s[idx - 1] += removedSize / 2;
      s[idx] += removedSize / 2;
    }

    setSizes(s);
  }
}
