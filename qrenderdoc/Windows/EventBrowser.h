#ifndef EVENTBROWSER_H
#define EVENTBROWSER_H

#include <QFrame>
#include <QIcon>

#include "Code/Core.h"

namespace Ui {
	class EventBrowser;
}

class QTreeWidgetItem;
class QTimer;

class EventBrowser : public QFrame, public ILogViewerForm
{
	private:
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

		void on_timeDraws_clicked();

		void on_toolButton_clicked();

		void on_HideFindJump();

		void on_jumpToEID_returnPressed();

		void on_findEvent_returnPressed();

		void on_findEvent_textEdited(const QString &arg1);

		void on_events_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

		void on_findNext_clicked();

		void on_findPrev_clicked();

		void on_findHighlight_timeout();

	private:
		uint AddDrawcalls(QTreeWidgetItem *parent, const rdctype::array<FetchDrawcall> &draws);
		void SetDrawcallTimes(QTreeWidgetItem *node, const rdctype::array<CounterResult> &results);

		void ExpandNode(QTreeWidgetItem *node);

		bool FindEventNode(QTreeWidgetItem *&found, QTreeWidgetItem *parent, uint32_t frameID, uint32_t eventID);
		bool SelectEvent(uint32_t frameID, uint32_t eventID);

		void ClearFindIcons(QTreeWidgetItem *parent);
		void ClearFindIcons();

		int SetFindIcons(QTreeWidgetItem *parent, QString filter);
		int SetFindIcons(QString filter);

		QTreeWidgetItem *FindNode(QTreeWidgetItem *parent, QString filter, uint32_t after);
		int FindEvent(QTreeWidgetItem *parent, QString filter, uint32_t after, bool forward);
		int FindEvent(QString filter, uint32_t after, bool forward);
		void Find(bool forward);

		QIcon m_CurrentIcon;
		QIcon m_FindIcon;
		QIcon m_BookmarkIcon;

		SizeDelegate *m_SizeDelegate;
		QTimer *m_FindHighlight;

		void RefreshIcon(QTreeWidgetItem *item);

		Ui::EventBrowser *ui;
		Core *m_Core;
};

#endif // EVENTBROWSER_H
