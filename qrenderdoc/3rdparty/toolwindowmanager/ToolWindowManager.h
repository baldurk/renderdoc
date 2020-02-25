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
#ifndef TOOLWINDOWMANAGER_H
#define TOOLWINDOWMANAGER_H

#include <QHash>
#include <QLabel>
#include <QVariant>
#include <QWidget>

#include <functional>

class ToolWindowManagerArea;
class ToolWindowManagerWrapper;

class QLabel;
class QSplitter;

/*!
 * \brief The ToolWindowManager class provides docking tool behavior.
 *
 * The behavior is similar to tool windows mechanism in Visual Studio or Eclipse.
 * User can arrange tool windows
 * in tabs, dock it to any border, split with vertical and horizontal splitters,
 * tabify them together and detach to floating windows.
 *
 * See https://github.com/Riateche/toolwindowmanager for detailed description.
 */
class ToolWindowManager : public QWidget
{
  Q_OBJECT
  /*!
   * \brief Whether or not to allow floating windows to be created.
   *
   * Default value is to allow it.
   *
   * Access functions: allowFloatingWindow, setAllowFloatingWindow.
   *
   */
  Q_PROPERTY(int allowFloatingWindow READ allowFloatingWindow WRITE setAllowFloatingWindow)

  /*!
   * \brief How much of a margin should be placed between drop hotspots.
   *
   * Default value is 4.
   *
   * Access functions: dropHotspotMargin, setDropHotspotMargin.
   *
   */
  Q_PROPERTY(int dropHotspotMargin READ dropHotspotMargin WRITE setDropHotspotMargin)

  /*!
   * \brief How wide and heigh each drop hotspot icon should be drawn at, in pixels.
   *
   * Default value is 32.
   *
   * Access functions: dropHotspotDimension, setDropHotspotDimension.
   *
   */
  Q_PROPERTY(int dropHotspotDimension READ dropHotspotDimension WRITE setDropHotspotDimension)

public:
  /*!
   * \brief Creates a manager with given \a parent.
   */
  explicit ToolWindowManager(QWidget *parent = 0);
  /*!
   * \brief Destroys the widget. Additionally all tool windows and all floating windows
   * created by this widget are destroyed.
   */
  virtual ~ToolWindowManager();

  //! Toolwindow properties
  enum ToolWindowProperty
  {
    //! Disables all drag/docking ability by the user
    DisallowUserDocking = 0x1,
    //! Hides the close button on the tab for this tool window
    HideCloseButton = 0x2,
    //! Disable the user being able to drag this tab in the tab bar, to rearrange
    DisableDraggableTab = 0x4,
    //! When the tool window is closed, hide it instead of removing it
    HideOnClose = 0x8,
    //! Don't allow this tool window to be floated
    DisallowFloatWindow = 0x10,
    //! When displaying this tool window in tabs, always display the tabs even if there's only one
    AlwaysDisplayFullTabs = 0x20,
  };

  //! Type of AreaReference.
  enum AreaReferenceType
  {
    //! The area tool windows has been added to most recently.
    LastUsedArea,
    //! New area in a detached window.
    NewFloatingArea,
    //! Area inside the manager widget (only available when there is no tool windows in it).
    EmptySpace,
    //! Tool window is hidden.
    NoArea,
    //! Existing area specified in AreaReference argument.
    AddTo,
    //! New area to the left of the area specified in AreaReference argument.
    LeftOf,
    //! New area to the right of the area specified in AreaReference argument.
    RightOf,
    //! New area to the top of the area specified in AreaReference argument.
    TopOf,
    //! New area to the bottom of the area specified in AreaReference argument.
    BottomOf,
    //! New area to the left of the window containing the specified in AreaReference argument.
    LeftWindowSide,
    //! New area to the right of the window containing the specified in AreaReference argument.
    RightWindowSide,
    //! New area to the top of the window containing the specified in AreaReference argument.
    TopWindowSide,
    //! New area to the bottom of the window containing the specified in AreaReference argument.
    BottomWindowSide,
    //! Invalid value, just indicates the number of types available
    NumReferenceTypes
  };

  /*!
   * \brief The AreaReference class represents a place where tool windows should be moved.
   */
  class AreaReference
  {
  public:
    /*!
     * Creates an area reference of the given \a type. If \a type requires specifying
     * area, it should be given in \a area argument. Otherwise \a area should have default value
     * (0).
     */
    AreaReference(AreaReferenceType type = NoArea, ToolWindowManagerArea *area = 0,
                  float percentage = 0.5f);
    //! Returns type of the reference.
    AreaReferenceType type() const { return m_type; }
    //! Returns area of the reference, or 0 if it was not specified.
    ToolWindowManagerArea *area() const;

  private:
    AreaReferenceType m_type;
    QWidget *m_widget;
    float m_percentage;
    bool dragResult;
    QWidget *widget() const { return m_widget; }
    float percentage() const { return m_percentage; }
    AreaReference(AreaReferenceType type, QWidget *widget);
    void setWidget(QWidget *widget);

    friend class ToolWindowManager;
  };

  /*!
   * Adds \a toolWindow to the manager and moves it to the position specified by
   * \a area. This function is a shortcut for ToolWindowManager::addToolWindows.
   */
  void addToolWindow(QWidget *toolWindow, const AreaReference &area,
                     ToolWindowProperty properties = ToolWindowProperty(0));

  /*!
   * Sets the set of \a properties on \a toolWindow that is already added to the manager.
   */
  void setToolWindowProperties(QWidget *toolWindow, ToolWindowProperty properties);

  /*!
   * Returns the set of \a properties on \a toolWindow.
   */
  ToolWindowProperty toolWindowProperties(QWidget *toolWindow);

  /*!
   * \brief Adds \a toolWindows to the manager and moves it to the position specified by
   * \a area.
   * The manager takes ownership of the tool windows and will delete them upon destruction.
   *
   * toolWindow->windowIcon() and toolWindow->windowTitle() will be used as the icon and title
   * of the tab that represents the tool window.
   *
   * If you intend to use ToolWindowManager::saveState
   * and ToolWindowManager::restoreState functions, you must set objectName() of each added
   * tool window to a non-empty unique string.
   */
  void addToolWindows(QList<QWidget *> toolWindows, const AreaReference &area,
                      ToolWindowProperty properties = ToolWindowProperty(0));

  /*!
   * Returns area that contains \a toolWindow, or 0 if \a toolWindow is hidden.
   */
  ToolWindowManagerArea *areaOf(QWidget *toolWindow);

  /*!
   * \brief Moves \a toolWindow to the position specified by \a area.
   *
   * \a toolWindow must be added to the manager prior to calling this function.
   */
  void moveToolWindow(QWidget *toolWindow, AreaReference area);

  /*!
   * \brief Moves \a toolWindows to the position specified by \a area.
   *
   * \a toolWindows must be added to the manager prior to calling this function.
   */
  void moveToolWindows(QList<QWidget *> toolWindows, AreaReference area);

  /*!
   * \brief Removes \a toolWindow from the manager. \a toolWindow becomes a hidden
   * top level widget. The ownership of \a toolWindow is returned to the caller.
   */
  void removeToolWindow(QWidget *toolWindow) { removeToolWindow(toolWindow, false); }
  /*!
  * Returns if \a toolWindow is floating instead of being docked.
  */
  bool isFloating(QWidget *toolWindow);

  /*!
   * \brief Returns all tool window added to the manager.
   */
  const QList<QWidget *> &toolWindows() { return m_toolWindows; }
  /*!
   * Hides \a toolWindow.
   *
   * \a toolWindow must be added to the manager prior to calling this function.
   */
  void hideToolWindow(QWidget *toolWindow) { moveToolWindow(toolWindow, NoArea); }
  static ToolWindowManager *managerOf(QWidget *toolWindow);
  static void closeToolWindow(QWidget *toolWindow);
  static void raiseToolWindow(QWidget *toolWindow);

  /*!
   * \brief saveState
   */
  QVariantMap saveState();

  /*!
   * \brief restoreState
   */
  void restoreState(const QVariantMap &data);

  typedef std::function<QWidget *(const QString &)> CreateCallback;

  void setToolWindowCreateCallback(const CreateCallback &cb) { m_createCallback = cb; }
  QWidget *createToolWindow(const QString &objectName);

  void setHotspotPixmap(AreaReferenceType ref, const QPixmap &pix) { m_pixmaps[ref] = pix; }
  void setDropHotspotMargin(int pixels);
  bool dropHotspotMargin() { return m_dropHotspotMargin; }
  void setDropHotspotDimension(int pixels);
  bool dropHotspotDimension() { return m_dropHotspotDimension; }
  /*! \cond PRIVATE */
  void setAllowFloatingWindow(bool pixels);
  bool allowFloatingWindow() { return m_allowFloatingWindow; }
  /*! \endcond */

signals:
  /*!
   * \brief This signal is emitted when \a toolWindow may be hidden or shown.
   * \a visible indicates new visibility state of the tool window.
   */
  void toolWindowVisibilityChanged(QWidget *toolWindow, bool visible);

private:
  QList<QWidget *> m_toolWindows;                                 // all added tool windows
  QHash<QWidget *, ToolWindowProperty> m_toolWindowProperties;    // all tool window properties
  QList<ToolWindowManagerArea *> m_areas;                         // all areas for this manager
  QList<ToolWindowManagerWrapper *> m_wrappers;                   // all wrappers for this manager
  // list of tool windows that are currently dragged, or empty list if there is no current drag
  QList<QWidget *> m_draggedToolWindows;
  ToolWindowManagerWrapper
      *m_draggedWrapper;                 // the wrapper if a whole float window is being dragged
  ToolWindowManagerArea *m_hoverArea;    // the area currently being hovered over in a drag
  // a semi-transparent preview of where the dragged toolwindow(s) will be docked
  QWidget *m_previewOverlay;
  QWidget *m_previewTabOverlay;
  QLabel *m_dropHotspots[NumReferenceTypes];
  QPixmap m_pixmaps[NumReferenceTypes];

  bool m_allowFloatingWindow;    // Allow floating windows from this docking area
  int m_dropHotspotMargin;       // The pixels between drop hotspot icons
  int m_dropHotspotDimension;    // The pixel dimension of the hotspot icons

  CreateCallback m_createCallback;

  ToolWindowManagerWrapper *wrapperOf(QWidget *toolWindow);

  void drawHotspotPixmaps();

  bool allowClose(QWidget *toolWindow);

  void removeToolWindow(QWidget *toolWindow, bool allowCloseAlreadyChecked);

  // last widget used for adding tool windows, or 0 if there isn't one
  // (warning: may contain pointer to deleted object)
  ToolWindowManagerArea *m_lastUsedArea;
  // remove tool window from its area (if any) and set parent to 0
  void releaseToolWindow(QWidget *toolWindow);
  void simplifyLayout();    // remove constructions that became useless
  void startDrag(const QList<QWidget *> &toolWindows, ToolWindowManagerWrapper *wrapper);

  QVariantMap saveSplitterState(QSplitter *splitter);
  QSplitter *restoreSplitterState(const QVariantMap &data);

  AreaReferenceType currentHotspot();

  void updateDragPosition();
  void abortDrag();
  void finishDrag();
  bool dragInProgress() { return !m_draggedToolWindows.isEmpty(); }
  friend class ToolWindowManagerArea;
  friend class ToolWindowManagerWrapper;

protected:
  //! Event filter for grabbing and processing drag aborts.
  virtual bool eventFilter(QObject *object, QEvent *event);

  /*!
   * \brief Creates new splitter and sets its default properties. You may reimplement
   * this function to change properties of all splitters used by this class.
   */
  virtual QSplitter *createSplitter();
  /*!
   * \brief Creates new area and sets its default properties. You may reimplement
   * this function to change properties of all tab widgets used by this class.
   */
  virtual ToolWindowManagerArea *createArea();

private slots:
  void tabCloseRequested(int index);
  void windowTitleChanged(const QString &title);
};

inline ToolWindowManager::ToolWindowProperty operator|(ToolWindowManager::ToolWindowProperty a,
                                                       ToolWindowManager::ToolWindowProperty b)
{
  return ToolWindowManager::ToolWindowProperty(int(a) | int(b));
}

#endif    // TOOLWINDOWMANAGER_H
