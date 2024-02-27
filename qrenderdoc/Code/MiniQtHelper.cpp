/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "MiniQtHelper.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>
#include "Code/QRDUtils.h"
#include "Code/pyrenderdoc/PythonContext.h"
#include "Widgets/CollapseGroupBox.h"
#include "Widgets/CustomPaintWidget.h"
#include "Widgets/Extended/RDDoubleSpinBox.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDLineEdit.h"
#include "Widgets/Extended/RDTextEdit.h"
#include "Widgets/Extended/RDToolButton.h"
#include "toolwindowmanager/ToolWindowManager.h"

MiniQtHelper::MiniQtHelper(ICaptureContext &ctx) : m_Ctx(ctx)
{
}

MiniQtHelper::~MiniQtHelper()
{
  PythonContext::ProcessExtensionWork([this]() {
    for(rdcpair<QWidget *, QMetaObject::Connection> &conn : m_Connections)
    {
      if(conn.second)
        QObject::disconnect(conn.second);
    }
  });
}

void MiniQtHelper::InvokeOntoUIThread(std::function<void()> callback)
{
  GUIInvoke::call(m_Ctx.GetMainWindow()->Widget(), callback);
}

void MiniQtHelper::AddWidgetCallback(QWidget *widget, QMetaObject::Connection connection)
{
  // remember the connection and delete it python-safely at shutdown if it's still there
  m_Connections.push_back({widget, connection});

  // when this widget is destroyed otherwise, delete it python-safely then.
  QObject::connect(widget, &QWidget::destroyed, [this, widget]() {
    PythonContext::ProcessExtensionWork([this, widget]() {
      for(int i = 0; i < m_Connections.count();)
      {
        if(m_Connections[i].first == widget)
        {
          QObject::disconnect(m_Connections[i].second);
          m_Connections.takeAt(i);
          continue;
        }

        i++;
      }
    });
  });
}

QWidget *MiniQtHelper::CreateToplevelWidget(const rdcstr &windowTitle, WidgetCallback closed)
{
  QWidget *ret = new QWidget();
  ret->setWindowTitle(windowTitle);
  ret->setLayout(new QVBoxLayout());
  if(closed)
    AddWidgetCallback(ret, QObject::connect(ret, &QWidget::destroyed, [this, ret, closed]() {
                        closed(&m_Ctx, ret, rdcstr());
                      }));
  return ret;
}

void MiniQtHelper::CloseToplevelWidget(QWidget *widget)
{
  if(widget)
    ToolWindowManager::closeToolWindow(widget);
}

void MiniQtHelper::SetWidgetName(QWidget *widget, const rdcstr &name)
{
  if(widget)
    widget->setObjectName(name);
}

rdcstr MiniQtHelper::GetWidgetName(QWidget *widget)
{
  if(widget)
    return widget->objectName();

  return rdcstr();
}

rdcstr MiniQtHelper::GetWidgetType(QWidget *widget)
{
  if(widget)
    return widget->metaObject()->className();

  return rdcstr();
}

QWidget *MiniQtHelper::FindChildByName(QWidget *parent, const rdcstr &name)
{
  if(!parent)
    return NULL;

  return parent->findChild<QWidget *>(name);
}

QWidget *MiniQtHelper::GetParent(QWidget *widget)
{
  if(!widget)
    return NULL;

  return widget->parentWidget();
}

int32_t MiniQtHelper::GetNumChildren(QWidget *widget)
{
  if(!widget)
    return 0;

  QLayout *layout = widget->layout();
  if(!layout)
    return 0;

  return layout->count();
}

QWidget *MiniQtHelper::GetChild(QWidget *parent, int32_t index)
{
  if(!parent)
    return NULL;

  QLayout *layout = parent->layout();
  if(!layout)
    return NULL;

  QLayoutItem *item = layout->itemAt(index);
  if(!item)
    return NULL;

  return item->widget();
}

void MiniQtHelper::DestroyWidget(QWidget *widget)
{
  widget->deleteLater();
}

bool MiniQtHelper::ShowWidgetAsDialog(QWidget *widget)
{
  QWidget *mainWindow = m_Ctx.GetMainWindow()->Widget();

  m_CurrentDialog = new QDialog(mainWindow);
  m_CurrentDialog->setWindowFlags(m_CurrentDialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  m_CurrentDialog->setWindowIcon(mainWindow->windowIcon());
  m_CurrentDialog->setWindowTitle(widget->windowTitle());
  m_CurrentDialog->setModal(true);

  QVBoxLayout l;
  l.addWidget(widget);
  l.setMargin(3);

  m_CurrentDialog->setLayout(&l);

  bool success = (RDDialog::show(m_CurrentDialog) == QDialog::Accepted);

  m_CurrentDialog->deleteLater();
  m_CurrentDialog = NULL;

  return success;
}

void MiniQtHelper::CloseCurrentDialog(bool success)
{
  if(m_CurrentDialog)
  {
    if(success)
      m_CurrentDialog->accept();
    else
      m_CurrentDialog->reject();
  }
}

QWidget *MiniQtHelper::CreateHorizontalContainer()
{
  QWidget *ret = new QWidget();
  ret->setLayout(new QHBoxLayout(ret));
  return ret;
}

QWidget *MiniQtHelper::CreateVerticalContainer()
{
  QWidget *ret = new QWidget();
  ret->setLayout(new QVBoxLayout(ret));
  return ret;
}

QWidget *MiniQtHelper::CreateGridContainer()
{
  QWidget *ret = new QWidget();
  ret->setLayout(new QGridLayout(ret));
  return ret;
}

QWidget *MiniQtHelper::CreateSpacer(bool horizontal)
{
  QWidget *ret = new QWidget();
  ret->setSizePolicy(horizontal ? QSizePolicy::Expanding : QSizePolicy::Preferred,
                     horizontal ? QSizePolicy::Preferred : QSizePolicy::Expanding);
  ret->setMinimumSize(1, 1);
  return ret;
}

void MiniQtHelper::ClearContainedWidgets(QWidget *parent)
{
  if(!parent)
    return;

  QLayout *layout = parent->layout();
  if(!layout)
    return;

  while(layout->count())
  {
    layout->takeAt(0)->widget()->hide();
  }
}

void MiniQtHelper::AddGridWidget(QWidget *parent, int32_t row, int32_t column, QWidget *child,
                                 int32_t rowSpan, int32_t columnSpan)
{
  if(!parent || !child)
    return;

  QLayout *layout = parent->layout();
  if(!layout)
    return;

  QGridLayout *grid = qobject_cast<QGridLayout *>(layout);
  if(!grid)
    return;

  grid->addWidget(child, row, column, rowSpan, columnSpan);
}

void MiniQtHelper::AddWidget(QWidget *parent, QWidget *child)
{
  if(!parent)
    return;

  QLayout *layout = parent->layout();
  if(!layout)
    return;

  QBoxLayout *box = qobject_cast<QBoxLayout *>(layout);
  if(!box)
    return;

  box->addWidget(child);
}

void MiniQtHelper::InsertWidget(QWidget *parent, int32_t index, QWidget *child)
{
  if(!parent)
    return;

  QLayout *layout = parent->layout();
  if(!layout)
    return;

  QBoxLayout *box = qobject_cast<QBoxLayout *>(layout);
  if(!box)
    return;

  box->insertWidget(qMin(qMax(0, index), box->count()), child);
}

void MiniQtHelper::SetWidgetText(QWidget *widget, const rdcstr &text)
{
  if(!widget)
    return;

  widget->setWindowTitle(text);

#define SET_TEXT(TextWidget)                            \
  {                                                     \
    TextWidget *w = qobject_cast<TextWidget *>(widget); \
    if(w)                                               \
      return w->setText(text);                          \
  }

  // setting text on a QLabel removes its pixmap
  {
    QLabel *label = qobject_cast<QLabel *>(widget);
    if(label)
    {
      label->setMinimumSize(QSize(0, 0));
      label->setMaximumSize(QSize(10000, 10000));
      label->setPixmap(QPixmap());
    }
  }
  {
    RDLabel *label = qobject_cast<RDLabel *>(widget);
    if(label)
    {
      label->setMinimumSize(QSize(0, 0));
      label->setMaximumSize(QSize(10000, 10000));
      label->setPixmap(QPixmap());
    }
  }

  SET_TEXT(RDLabel);
  SET_TEXT(QLabel);
  SET_TEXT(RDLineEdit);
  SET_TEXT(RDTextEdit);
  SET_TEXT(QLineEdit);
  SET_TEXT(QTextEdit);
  SET_TEXT(QPushButton);
  SET_TEXT(RDToolButton);
  SET_TEXT(QToolButton);
  SET_TEXT(QCheckBox);
  SET_TEXT(QRadioButton);

  {
    QGroupBox *w = qobject_cast<QGroupBox *>(widget);
    if(w)
      return w->setTitle(text);
  }
  {
    CollapseGroupBox *w = qobject_cast<CollapseGroupBox *>(widget);
    if(w)
      return w->setTitle(text);
  }
}

rdcstr MiniQtHelper::GetWidgetText(QWidget *widget)
{
  if(!widget)
    return rdcstr();

#define GET_TEXT(TextWidget)                            \
  {                                                     \
    TextWidget *w = qobject_cast<TextWidget *>(widget); \
    if(w)                                               \
      return w->text();                                 \
  }

  GET_TEXT(RDLabel);
  GET_TEXT(QLabel);
  GET_TEXT(RDLineEdit);
  GET_TEXT(QLineEdit);
  GET_TEXT(QPushButton);
  GET_TEXT(RDToolButton);
  GET_TEXT(QToolButton);
  GET_TEXT(QCheckBox);
  GET_TEXT(QRadioButton);

  {
    QTextEdit *w = qobject_cast<QTextEdit *>(widget);
    if(w)
      return w->toPlainText();
  }
  {
    RDTextEdit *w = qobject_cast<RDTextEdit *>(widget);
    if(w)
      return w->toPlainText();
  }

  {
    QGroupBox *w = qobject_cast<QGroupBox *>(widget);
    if(w)
      return w->title();
  }
  {
    CollapseGroupBox *w = qobject_cast<CollapseGroupBox *>(widget);
    if(w)
      return w->title();
  }

  // if all else failed, return the window title of the widget
  return widget->windowTitle();
}

void MiniQtHelper::SetWidgetFont(QWidget *widget, const rdcstr &font, int32_t fontSize, bool bold,
                                 bool italic)
{
  if(!widget)
    return;

  QFont f = widget->font();

  if(!font.empty())
    f.setFamily(font);
  if(fontSize != 0)
    f.setPointSize(fontSize);
  f.setBold(bold);
  f.setItalic(italic);

  widget->setFont(f);
}

void MiniQtHelper::SetWidgetEnabled(QWidget *widget, bool enabled)
{
  if(!widget)
    return;

  widget->setEnabled(enabled);
}

bool MiniQtHelper::IsWidgetEnabled(QWidget *widget)
{
  if(!widget)
    return false;

  return widget->isEnabled();
}

void MiniQtHelper::SetWidgetVisible(QWidget *widget, bool visible)
{
  if(!widget)
    return;

  widget->setVisible(visible);
}

bool MiniQtHelper::IsWidgetVisible(QWidget *widget)
{
  if(!widget)
    return false;

  return widget->isVisible();
}

QWidget *MiniQtHelper::CreateGroupBox(bool collapsible)
{
  QWidget *ret;
  if(collapsible)
    ret = new CollapseGroupBox();
  else
    ret = new QGroupBox();
  ret->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  ret->setLayout(new QVBoxLayout());
  return ret;
}

QWidget *MiniQtHelper::CreateButton(WidgetCallback pressed)
{
  QPushButton *w = new QPushButton();
  if(pressed)
    AddWidgetCallback(w, QObject::connect(w, &QPushButton::pressed,
                                          [this, w, pressed]() { pressed(&m_Ctx, w, rdcstr()); }));
  return w;
}

QWidget *MiniQtHelper::CreateLabel()
{
  return new RDLabel();
}

void MiniQtHelper::SetLabelImage(QWidget *widget, const bytebuf &data, int32_t width,
                                 int32_t height, bool alpha)
{
  if(!widget)
    return;

  RDLabel *label = qobject_cast<RDLabel *>(widget);

  if(label)
  {
    QPixmap pixmap;

    int32_t bpp = alpha ? 4 : 3;
    if(width > 0 && height > 0 && size_t(width * height * bpp) == data.size())
    {
      label->setFixedSize(width, height);
      label->setPixmap(
          QPixmap::fromImage(QImage(data.data(), width, height, width * bpp,
                                    alpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888)
                                 .copy(0, 0, width, height)));
    }
    else
    {
      label->setMinimumSize(QSize());
      label->setMaximumSize(QSize(10000, 10000));
      label->setPixmap(QPixmap());
    }
  }
}

QWidget *MiniQtHelper::CreateOutputRenderingWidget()
{
  CustomPaintWidget *widget = new CustomPaintWidget(NULL);
  widget->SetContext(m_Ctx);
  widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  return widget;
}

WindowingData MiniQtHelper::GetWidgetWindowingData(QWidget *widget)
{
  if(!widget)
    return {};

  CustomPaintWidget *paintWidget = qobject_cast<CustomPaintWidget *>(widget);

  if(paintWidget)
    return paintWidget->GetWidgetWindowingData();

  return {};
}

void MiniQtHelper::SetWidgetReplayOutput(QWidget *widget, IReplayOutput *output)
{
  if(!widget)
    return;

  CustomPaintWidget *paintWidget = qobject_cast<CustomPaintWidget *>(widget);

  if(paintWidget)
    paintWidget->SetOutput(output);
}

void MiniQtHelper::SetWidgetBackgroundColor(QWidget *widget, float red, float green, float blue)
{
  if(!widget)
    return;

  CustomPaintWidget *paintWidget = qobject_cast<CustomPaintWidget *>(widget);

  if(paintWidget)
    paintWidget->SetBackCol(red < 0.0 || green < 0.0 || blue < 0.0
                                ? QColor()
                                : QColor::fromRgb(qMin<int>(red * 255, 255),
                                                  qMin<int>(green * 255, 255),
                                                  qMin<int>(blue * 255, 255)));
}

QWidget *MiniQtHelper::CreateCheckbox(WidgetCallback changed)
{
  QCheckBox *w = new QCheckBox();
  if(changed)
    AddWidgetCallback(w, QObject::connect(w, &QCheckBox::stateChanged,
                                          [this, w, changed]() { changed(&m_Ctx, w, rdcstr()); }));
  return w;
}

QWidget *MiniQtHelper::CreateRadiobox(WidgetCallback changed)
{
  QRadioButton *w = new QRadioButton();
  if(changed)
    AddWidgetCallback(w, QObject::connect(w, &QRadioButton::toggled,
                                          [this, w, changed]() { changed(&m_Ctx, w, rdcstr()); }));
  return w;
}

void MiniQtHelper::SetWidgetChecked(QWidget *checkableWidget, bool checked)
{
  if(!checkableWidget)
    return;

  QCheckBox *check = qobject_cast<QCheckBox *>(checkableWidget);
  QRadioButton *radio = qobject_cast<QRadioButton *>(checkableWidget);
  CollapseGroupBox *group = qobject_cast<CollapseGroupBox *>(checkableWidget);

  if(check)
    check->setChecked(checked);
  else if(radio)
    radio->setChecked(checked);
  else if(group)
    group->setCollapsed(checked);
}

bool MiniQtHelper::IsWidgetChecked(QWidget *checkableWidget)
{
  if(!checkableWidget)
    return false;

  QCheckBox *check = qobject_cast<QCheckBox *>(checkableWidget);
  QRadioButton *radio = qobject_cast<QRadioButton *>(checkableWidget);
  CollapseGroupBox *group = qobject_cast<CollapseGroupBox *>(checkableWidget);

  if(check)
    return check->isChecked();
  else if(radio)
    return radio->isChecked();
  else if(group)
    return group->collapsed();

  return false;
}

QWidget *MiniQtHelper::CreateSpinbox(int32_t decimalPlaces, double step)
{
  RDDoubleSpinBox *ret = new RDDoubleSpinBox();
  ret->setSingleStep(step);
  ret->setDecimals(decimalPlaces);
  return ret;
}

void MiniQtHelper::SetSpinboxBounds(QWidget *spinbox, double minVal, double maxVal)
{
  if(!spinbox)
    return;

  RDDoubleSpinBox *spin = qobject_cast<RDDoubleSpinBox *>(spinbox);
  if(spin)
    spin->setRange(minVal, maxVal);
}

void MiniQtHelper::SetSpinboxValue(QWidget *spinbox, double value)
{
  if(!spinbox)
    return;

  RDDoubleSpinBox *spin = qobject_cast<RDDoubleSpinBox *>(spinbox);
  if(spin)
    spin->setValue(value);
}

double MiniQtHelper::GetSpinboxValue(QWidget *spinbox)
{
  if(!spinbox)
    return 0.0;

  RDDoubleSpinBox *spin = qobject_cast<RDDoubleSpinBox *>(spinbox);
  if(spin)
    return spin->value();

  return 0.0;
}

QWidget *MiniQtHelper::CreateTextBox(bool singleLine, WidgetCallback changed)
{
  if(singleLine)
  {
    RDLineEdit *w = new RDLineEdit();
    if(changed)
      AddWidgetCallback(w, QObject::connect(w, &RDLineEdit::textEdited, [this, w, changed]() {
                          changed(&m_Ctx, w, w->text());
                        }));
    return w;
  }
  else
  {
    RDTextEdit *w = new RDTextEdit();
    if(changed)
      AddWidgetCallback(w, QObject::connect(w, &RDTextEdit::textChanged, [this, w, changed]() {
                          changed(&m_Ctx, w, w->toPlainText());
                        }));
    w->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    return w;
  }
}

QWidget *MiniQtHelper::CreateComboBox(bool editable, WidgetCallback changed)
{
  QComboBox *w = new QComboBox();
  if(changed)
    AddWidgetCallback(
        w, QObject::connect(w, &QComboBox::currentTextChanged,
                            [this, w, changed](QString str) { changed(&m_Ctx, w, str); }));
  w->setEditable(editable);
  return w;
}

void MiniQtHelper::SetComboOptions(QWidget *combo, const rdcarray<rdcstr> &options)
{
  if(!combo)
    return;
  QComboBox *comb = qobject_cast<QComboBox *>(combo);

  QStringList texts;

  for(const rdcstr &o : options)
    texts << o;

  if(comb)
  {
    comb->clear();
    comb->addItems(texts);
  }
}

size_t MiniQtHelper::GetComboCount(QWidget *combo)
{
  if(!combo)
    return 0;
  const QComboBox *comb = qobject_cast<const QComboBox *>(combo);

  if(comb)
    return comb->count();

  return 0;
}

void MiniQtHelper::SelectComboOption(QWidget *combo, const rdcstr &option)
{
  if(!combo)
    return;
  QComboBox *comb = qobject_cast<QComboBox *>(combo);

  if(comb)
    comb->setCurrentText(option);
}

QWidget *MiniQtHelper::CreateProgressBar(bool horizontal)
{
  QProgressBar *w = new QProgressBar();

  w->setOrientation(horizontal ? Qt::Horizontal : Qt::Vertical);

  return w;
}

void MiniQtHelper::ResetProgressBar(QWidget *pbar)
{
  if(!pbar)
    return;

  QProgressBar *pb = qobject_cast<QProgressBar *>(pbar);

  if(pb)
    pb->reset();
}

void MiniQtHelper::SetProgressBarValue(QWidget *pbar, int32_t value)
{
  if(!pbar)
    return;

  QProgressBar *pb = qobject_cast<QProgressBar *>(pbar);

  if(pb)
    pb->setValue(value);
}

void MiniQtHelper::UpdateProgressBarValue(QWidget *pbar, int32_t delta)
{
  if(!pbar)
    return;

  QProgressBar *pb = qobject_cast<QProgressBar *>(pbar);

  if(pb)
    pb->setValue(pb->value() + delta);
}

int32_t MiniQtHelper::GetProgressBarValue(QWidget *pbar)
{
  if(!pbar)
    return 0;

  QProgressBar *pb = qobject_cast<QProgressBar *>(pbar);

  if(pb)
    return pb->value();

  return 0;
}

void MiniQtHelper::SetProgressBarRange(QWidget *pbar, int32_t minimum, int32_t maximum)
{
  if(!pbar)
    return;

  QProgressBar *pb = qobject_cast<QProgressBar *>(pbar);

  if(pb)
    pb->setRange(minimum, maximum);
}

int32_t MiniQtHelper::GetProgressBarMinimum(QWidget *pbar)
{
  if(!pbar)
    return 0;

  QProgressBar *pb = qobject_cast<QProgressBar *>(pbar);

  if(pb)
    return pb->minimum();

  return 0;
}

int32_t MiniQtHelper::GetProgressBarMaximum(QWidget *pbar)
{
  if(!pbar)
    return 0;

  QProgressBar *pb = qobject_cast<QProgressBar *>(pbar);

  if(pbar)
    return pb->maximum();

  return 0;
}
