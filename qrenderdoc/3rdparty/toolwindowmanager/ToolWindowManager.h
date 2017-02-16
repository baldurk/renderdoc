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

#include <QWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QRubberBand>
#include <QHash>
#include <QVariant>
#include <QLabel>

#include <functional>

class ToolWindowManagerArea;
class ToolWindowManagerWrapper;


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
class ToolWindowManager : public QWidget {
  Q_OBJECT
  /*!
   * \brief The delay between showing the next suggestion of drop location in milliseconds.
   *
   * When user starts a tool window drag and moves mouse pointer to a position, there can be
   * an ambiguity in new position of the tool window. If user holds the left mouse button and
   * stops mouse movements, all possible suggestions will be indicated periodically, one at a time.
   *
   * Default value is 1000 (i.e. 1 second).
   *
   * Access functions: suggestionSwitchInterval, setSuggestionSwitchInterval.
   *
   */
  Q_PROPERTY(int suggestionSwitchInterval READ suggestionSwitchInterval WRITE setSuggestionSwitchInterval)
  /*!
   * \brief Maximal distance in pixels between mouse position and area border that allows
   * to display a suggestion.
   *
   * Default value is 12.
   *
   * Access functions: borderSensitivity, setBorderSensitivity.
   */
  Q_PROPERTY(int borderSensitivity READ borderSensitivity WRITE setBorderSensitivity)
  /*!
   * \brief Visible width of rubber band line that is used to display drop suggestions.
   *
   * Default value is the same as QSplitter::handleWidth default value on current platform.
   *
   * Access functions: rubberBandLineWidth, setRubberBandLineWidth.
   *
   */
  Q_PROPERTY(int rubberBandLineWidth READ rubberBandLineWidth WRITE setRubberBandLineWidth)
  /*!
   * \brief Whether or not to allow floating windows to be created.
   *
   * Default value is to allow it.
   *
   * Access functions: allowFloatingWindow, setAllowFloatingWindow.
   *
   */
  Q_PROPERTY(int allowFloatingWindow READ allowFloatingWindow WRITE setAllowFloatingWindow)

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
  enum ToolWindowProperty {
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
  };

  //! Type of AreaReference.
  enum AreaReferenceType {
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
    BottomOf
  };

  /*!
   * \brief The AreaReference class represents a place where tool windows should be moved.
   */
  class AreaReference {
  public:
    /*!
     * Creates an area reference of the given \a type. If \a type requires specifying
     * area, it should be given in \a area argument. Otherwise \a area should have default value (0).
     */
    AreaReference(AreaReferenceType type = NoArea, ToolWindowManagerArea* area = 0, float percentage = 0.5f);
    //! Returns type of the reference.
    AreaReferenceType type() const { return m_type; }
    //! Returns area of the reference, or 0 if it was not specified.
    ToolWindowManagerArea* area() const;

  private:
    AreaReferenceType m_type;
    QWidget* m_widget;
    float m_percentage;
    QWidget* widget() const { return m_widget; }
    float percentage() const { return m_percentage; }
    AreaReference(AreaReferenceType type, QWidget* widget);
    void setWidget(QWidget* widget);

    friend class ToolWindowManager;

  };

  /*!
   * Adds \a toolWindow to the manager and moves it to the position specified by
   * \a area. This function is a shortcut for ToolWindowManager::addToolWindows.
   */
  void addToolWindow(QWidget* toolWindow, const AreaReference& area);

  /*!
   * Sets the set of \a properties on \a toolWindow that is already added to the manager.
   */
  void setToolWindowProperties(QWidget* toolWindow, ToolWindowProperty properties);

  /*!
   * Returns the set of \a properties on \a toolWindow.
   */
  ToolWindowProperty toolWindowProperties(QWidget* toolWindow);

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
  void addToolWindows(QList<QWidget*> toolWindows, const AreaReference& area);

  /*!
   * Returns area that contains \a toolWindow, or 0 if \a toolWindow is hidden.
   */
  ToolWindowManagerArea* areaOf(QWidget* toolWindow);

  /*!
   * \brief Moves \a toolWindow to the position specified by \a area.
   *
   * \a toolWindow must be added to the manager prior to calling this function.
   */
  void moveToolWindow(QWidget* toolWindow, AreaReference area);

  /*!
   * \brief Moves \a toolWindows to the position specified by \a area.
   *
   * \a toolWindows must be added to the manager prior to calling this function.
   */
  void moveToolWindows(QList<QWidget*> toolWindows, AreaReference area);

  /*!
   * \brief Removes \a toolWindow from the manager. \a toolWindow becomes a hidden
   * top level widget. The ownership of \a toolWindow is returned to the caller.
   */
  void removeToolWindow(QWidget* toolWindow);

  /*!
   * \brief Returns all tool window added to the manager.
   */
  const QList<QWidget*>& toolWindows() { return m_toolWindows; }

  /*!
   * Hides \a toolWindow.
   *
   * \a toolWindow must be added to the manager prior to calling this function.
   */
  void hideToolWindow(QWidget* toolWindow) { moveToolWindow(toolWindow, NoArea); }
  
  static ToolWindowManager* managerOf(QWidget* toolWindow);
  static void closeToolWindow(QWidget *toolWindow);
  static void raiseToolWindow(QWidget *toolWindow);

  /*!
   * \brief saveState
   */
  QVariantMap saveState();

  /*!
   * \brief restoreState
   */
  void restoreState(const QVariantMap& data);

  typedef std::function<QWidget*(const QString &)> CreateCallback;

  void setToolWindowCreateCallback(const CreateCallback &cb) { m_createCallback = cb; }
  QWidget *createToolWindow(const QString& objectName);

  bool checkValidSplitter(QWidget *w);

  /*! \cond PRIVATE */
  void setSuggestionSwitchInterval(int msec);
  int suggestionSwitchInterval();
  int borderSensitivity() { return m_borderSensitivity; }
  void setBorderSensitivity(int pixels);
  void setRubberBandLineWidth(int pixels);
  int rubberBandLineWidth() { return m_rubberBandLineWidth; }
  void setAllowFloatingWindow(bool pixels);
  bool allowFloatingWindow() { return m_allowFloatingWindow; }
  /*! \endcond */

  /*!
   * Returns the widget that is used to display rectangular drop suggestions.
   */
  QRubberBand* rectRubberBand() { return m_rectRubberBand; }

  /*!
   * Returns the widget that is used to display line drop suggestions.
   */
  QRubberBand* lineRubberBand() { return m_lineRubberBand; }


signals:
  /*!
   * \brief This signal is emitted when \a toolWindow may be hidden or shown.
   * \a visible indicates new visibility state of the tool window.
   */
  void toolWindowVisibilityChanged(QWidget* toolWindow, bool visible);

private:
  QList<QWidget*> m_toolWindows; // all added tool windows
  QHash<QWidget*, ToolWindowProperty> m_toolWindowProperties; // all tool window properties
  QList<ToolWindowManagerArea*> m_areas; // all areas for this manager
  QList<ToolWindowManagerWrapper*> m_wrappers; // all wrappers for this manager
  int m_borderSensitivity;
  int m_rubberBandLineWidth;
  // list of tool windows that are currently dragged, or empty list if there is no current drag
  QList<QWidget*> m_draggedToolWindows;
  QLabel* m_dragIndicator; // label used to display dragged content

  QRubberBand* m_rectRubberBand; // placeholder objects used for displaying drop suggestions
  QRubberBand* m_lineRubberBand;
  bool m_allowFloatingWindow; // Allow floating windows from this docking area
  QList<AreaReference> m_suggestions; //full list of suggestions for current cursor position
  int m_dropCurrentSuggestionIndex; // index of currently displayed drop suggestion
                                    // (e.g. always 0 if there is only one possible drop location)
  QTimer m_dropSuggestionSwitchTimer; // used for switching drop suggestions

  CreateCallback m_createCallback;

  // last widget used for adding tool windows, or 0 if there isn't one
  // (warning: may contain pointer to deleted object)
  ToolWindowManagerArea* m_lastUsedArea;
  void handleNoSuggestions();
  //remove tool window from its area (if any) and set parent to 0
  void releaseToolWindow(QWidget* toolWindow);
  void simplifyLayout(); //remove constructions that became useless
  void startDrag(const QList<QWidget*>& toolWindows);

  QVariantMap saveSplitterState(QSplitter* splitter);
  QSplitter* restoreSplitterState(const QVariantMap& data);
  void findSuggestions(ToolWindowManagerWrapper *wrapper);
  QRect sideSensitiveArea(QWidget* widget, AreaReferenceType side);
  QRect sidePlaceHolderRect(QWidget* widget, AreaReferenceType side);

  void updateDragPosition();
  void finishDrag();
  bool dragInProgress() { return !m_draggedToolWindows.isEmpty(); }

  friend class ToolWindowManagerArea;
  friend class ToolWindowManagerWrapper;

protected:
  /*!
   * \brief Creates new splitter and sets its default properties. You may reimplement
   * this function to change properties of all splitters used by this class.
   */
  virtual QSplitter* createSplitter();
  /*!
   * \brief Creates new area and sets its default properties. You may reimplement
   * this function to change properties of all tab widgets used by this class.
   */
  virtual ToolWindowManagerArea *createArea();
  /*!
   * \brief Generates a pixmap that is used to represent the data in a drag and drop operation
   * near the mouse cursor.
   * You may reimplement this function to use different pixmaps.
   */
  virtual QPixmap generateDragPixmap(const QList<QWidget *> &toolWindows);

private slots:
  void showNextDropSuggestion();
  void tabCloseRequested(int index);
  void windowTitleChanged(const QString &title);

};

inline ToolWindowManager::ToolWindowProperty operator|(ToolWindowManager::ToolWindowProperty a, ToolWindowManager::ToolWindowProperty b)
{ return ToolWindowManager::ToolWindowProperty(int(a) | int(b)); }

#endif // TOOLWINDOWMANAGER_H
