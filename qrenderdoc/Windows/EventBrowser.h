#ifndef EVENTBROWSER_H
#define EVENTBROWSER_H

#include <QFrame>

#include "Code/Core.h"

namespace Ui {
class EventBrowser;
}

class EventBrowser : public QFrame, public ILogViewerForm
{
    Q_OBJECT

  public:
    explicit EventBrowser(Core *core, QWidget *parent = 0);
    ~EventBrowser();

    void OnLogfileLoaded();
    void OnLogfileClosed();
    void OnEventSelected(uint32_t frameID, uint32_t eventID);

  private slots:
    void on_find_clicked();

    void on_gotoEID_clicked();

    void on_events_itemSelectionChanged();

    void on_timeDraws_clicked();

    void on_toolButton_clicked();

    void hideFindJump();

    void on_jumpToEID_returnPressed();

    void on_findEvent_returnPressed();

    private:
    Ui::EventBrowser *ui;
    Core *m_Core;
};

#endif // EVENTBROWSER_H
