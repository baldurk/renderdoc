/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Pavel Strakhov
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
#ifndef TOOLWINDOWMANAGERAREA_H
#define TOOLWINDOWMANAGERAREA_H

#include <QTabWidget>
#include <QVariantMap>

class ToolWindowManager;

/*!
 * \brief The ToolWindowManagerArea class is a tab widget used to store tool windows.
 * It implements dragging of its tab or the whole tab widget.
 */
class ToolWindowManagerArea : public QTabWidget {
  Q_OBJECT
public:
  //! Creates new area.
  explicit ToolWindowManagerArea(ToolWindowManager* manager, QWidget *parent = 0);
  //! Destroys the area.
  virtual ~ToolWindowManagerArea();

  /*!
   * Add \a toolWindow to this area.
   */
  void addToolWindow(QWidget* toolWindow);

  /*!
   * Add \a toolWindows to this area.
   */
  void addToolWindows(const QList<QWidget*>& toolWindows);

  void enableUserDrop() { m_userCanDrop = true; }
  void disableUserDrop() { m_userCanDrop = false; }

  bool allowUserDrop() { return m_userCanDrop; }

  /*!
   * Returns a list of all tool windows in this area.
   */
  QList<QWidget*> toolWindows();

  ToolWindowManager* manager() { return m_manager; }

  /*!
   * Updates the \a toolWindow to its current properties and title.
   */
  void updateToolWindow(QWidget* toolWindow);

protected:
  //! Reimplemented from QTabWidget::mousePressEvent.
  virtual void mousePressEvent(QMouseEvent *);
  //! Reimplemented from QTabWidget::mouseReleaseEvent.
  virtual void mouseReleaseEvent(QMouseEvent *);
  //! Reimplemented from QTabWidget::mouseMoveEvent.
  virtual void mouseMoveEvent(QMouseEvent *);
  //! Reimplemented from QTabWidget::eventFilter.
  virtual bool eventFilter(QObject *object, QEvent *event);

private:
  ToolWindowManager* m_manager;
  bool m_dragCanStart; // indicates that user has started mouse movement on QTabWidget
                       // that can be considered as dragging it if the cursor will leave
                       // its area

  bool m_tabDragCanStart; // indicates that user has started mouse movement on QTabWidget
                          // that can be considered as dragging current tab
                          // if the cursor will leave the tab bar area

  bool m_userCanDrop; // indictes the user is allowed to drop things on this area

  bool m_inTabMoved; // if we're in the tabMoved() function (so if we call tabMove to cancel
                     // the movement, we shouldn't re-check the tabMoved behaviour)

  QVariantMap saveState(); // dump contents to variable
  void restoreState(const QVariantMap& data); //restore contents from given variable

  //check if mouse left tab widget area so that dragging should start
  void check_mouse_move();

  friend class ToolWindowManager;
  friend class ToolWindowManagerWrapper;

private slots:
  void tabMoved(int from, int to);
};

#endif // TOOLWINDOWMANAGERAREA_H
