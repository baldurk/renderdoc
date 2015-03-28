#ifndef EVENTBROWSER_H
#define EVENTBROWSER_H

#include <QFrame>

namespace Ui {
class EventBrowser;
}

class EventBrowser : public QFrame
{
    Q_OBJECT

  public:
    explicit EventBrowser(QWidget *parent = 0);
    ~EventBrowser();

  private slots:
    void on_find_clicked();

    void on_gotoEID_clicked();

    void on_events_itemSelectionChanged();

  private:
    Ui::EventBrowser *ui;
};

#endif // EVENTBROWSER_H
