/******************************************************************************
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

#include "Resources.h"
#include <QDebug>
#include <QDirIterator>

Resources::ResourceSet *Resources::resources = NULL;

void Resources::Initialise()
{
  QList<QString> filenames;

  resources = new Resources::ResourceSet();

#undef RESOURCE_DEF
#define RESOURCE_DEF(name, filename)                                         \
  filenames.push_back(":/" filename);                                        \
  resources->name##_data.pixmap = QPixmap(QString::fromUtf8(":/" filename)); \
  resources->name##_data.icon = QIcon(resources->name##_data.pixmap);

  RESOURCE_LIST();

  QDirIterator it(":");
  while(it.hasNext())
  {
    QString filename = it.next();
    if(filenames.contains(filename))
      continue;
    if(!filename.contains(".png"))
      continue;

    qCritical() << "Resource not configured for" << filename;
  }
}

Resources::~Resources()
{
  delete resources;
}
