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
#ifndef TOOLWINDOWMANAGERTABBAR_H
#define TOOLWINDOWMANAGERTABBAR_H

#include <QIcon>
#include <QTabBar>

class ToolWindowManager;
class ToolWindowManagerArea;

/*!
 * \brief The ToolWindowManagerTabBar class is a tab bar used to customise the painting
 * in the case that there's only only one child widget.
 */
class ToolWindowManagerTabBar : public QTabBar
{
  Q_OBJECT
public:
  //! Creates new tab bar.
  explicit ToolWindowManagerTabBar(QWidget *parent = 0);
  //! Destroys the tab bar.
  virtual ~ToolWindowManagerTabBar();

  bool tabsClosable() const { return m_tabsClosable; }
  void setTabsClosable(bool closable)
  {
    m_tabsClosable = closable;
    updateClosable();
  }

  //! Reimplemented from QTabWidget::QTabBar to custom size for the single tab case.
  QSize sizeHint() const Q_DECL_OVERRIDE;

  bool useMinimalBar() const;

  QSize minimumSizeHint() const Q_DECL_OVERRIDE;

  //! is this point in a custom titlebar button
  bool inButton(QPoint pos);

protected:
  //! Reimplemented from QTabWidget::QTabBar to custom paint for the single tab case.
  void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;

  //! Reimplemented from QTabWidget::QTabBar to cache painting parameters
  void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
  //! Reimplemented from QTabWidget::QTabBar to implement hover/click status of buttons
  void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE;
  void mouseMoveEvent(QMouseEvent *) Q_DECL_OVERRIDE;
  void mouseReleaseEvent(QMouseEvent *) Q_DECL_OVERRIDE;
  void leaveEvent(QEvent *) Q_DECL_OVERRIDE;

  //! Reimplemented from QTabWidget::QTabBar to enable/disable 'real' closable tabs.
  virtual void tabInserted(int index) Q_DECL_OVERRIDE;
  virtual void tabRemoved(int index) Q_DECL_OVERRIDE;

  ToolWindowManagerArea *m_area;

  bool m_tabsClosable;

  struct ButtonData
  {
    QRect rect;
    QIcon icon;
    bool clicked;
    bool hover;

    bool operator==(const ButtonData &o)
    {
      return rect == o.rect && clicked == o.clicked && hover == o.hover;
    }

    bool operator!=(const ButtonData &o) { return !(*this == o); }
  } m_close, m_pin;

  QRect m_titleRect;

  void updateClosable();
  bool floatingWindowChild() const;
};

#endif    // TOOLWINDOWMANAGERTABBAR_H
