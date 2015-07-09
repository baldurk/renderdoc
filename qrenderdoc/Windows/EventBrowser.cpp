#include "EventBrowser.h"
#include "ui_EventBrowser.h"

#include "Code/Core.h"

enum {
	COL_NAME = 0,
	COL_EID = 1,
	COL_DURATION = 2,
};

uint AddDrawcalls(QTreeWidgetItem *parent, const rdctype::array<FetchDrawcall> &draws)
{
	uint lastEID = 0;

	for(int32_t i = 0; i < draws.count; i++)
	{
		QTreeWidgetItem *child = new QTreeWidgetItem(parent, QStringList{ QString(draws[i].name.elems), QString("%1").arg(draws[i].eventID), "0.0" });
		lastEID = AddDrawcalls(child, draws[i].children);
		if(lastEID == 0) lastEID = draws[i].eventID;
		child->setData(COL_EID, Qt::UserRole, QVariant(lastEID));
	}

	return lastEID;
}

EventBrowser::EventBrowser(Core *core, QWidget *parent) :
QFrame(parent),
ui(new Ui::EventBrowser),
m_Core(core)
{
	ui->setupUi(this);

	m_Core->AddLogViewer(this);

	ui->events->header()->resizeSection(COL_EID, 80);

	ui->events->header()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
	ui->events->header()->setSectionResizeMode(COL_EID, QHeaderView::Interactive);
	ui->events->header()->setSectionResizeMode(COL_DURATION, QHeaderView::Interactive);

	// we set up the name column first, EID second, so that the name column gets the
	// expand/collapse widgets. Then we need to put them back in order
	ui->events->header()->moveSection(COL_NAME, COL_EID);

	// Qt doesn't allow moving the column with the expand/collapse widgets, so this
	// becomes quickly infuriating to rearrange, just disable until that can be fixed.
	ui->events->header()->setSectionsMovable(false);

	QObject::connect(ui->closeFind, &QToolButton::clicked, this, &EventBrowser::hideFindJump);
	QObject::connect(ui->closeJump, &QToolButton::clicked, this, &EventBrowser::hideFindJump);
	QObject::connect(ui->jumpToEID, &LineEditFocusWidget::leave, this, &EventBrowser::hideFindJump);
	QObject::connect(ui->findEvent, &LineEditFocusWidget::leave, this, &EventBrowser::hideFindJump);
	ui->jumpStrip->hide();
	ui->findStrip->hide();
	ui->bookmarkStrip->hide();
}

EventBrowser::~EventBrowser()
{
	m_Core->RemoveLogViewer(this);
	delete ui;
}

void EventBrowser::OnLogfileLoaded()
{
	QTreeWidgetItem *frame = new QTreeWidgetItem((QTreeWidget *)NULL, QStringList{ QString("Frame #%1").arg(m_Core->FrameInfo()[0].frameNumber), "", "" });

	QTreeWidgetItem *framestart = new QTreeWidgetItem(frame, QStringList{ "Frame Start", "0", "" });
	framestart->setData(COL_EID, Qt::UserRole, QVariant(0));

	uint lastEID = AddDrawcalls(frame, m_Core->CurDrawcalls(0));
	frame->setData(COL_EID, Qt::UserRole, QVariant(lastEID));

	ui->events->insertTopLevelItem(0, frame);

	ui->events->expandItem(frame);
}

void EventBrowser::OnLogfileClosed()
{
	ui->events->clear();
}

void EventBrowser::OnEventSelected(uint32_t frameID, uint32_t eventID)
{

}

void EventBrowser::on_find_clicked()
{
	ui->jumpStrip->hide();
	ui->findStrip->show();
	ui->bookmarkStrip->hide();
	ui->findEvent->setFocus();
}

void EventBrowser::on_gotoEID_clicked()
{
	ui->jumpStrip->show();
	ui->findStrip->hide();
	ui->bookmarkStrip->hide();
	ui->jumpToEID->setFocus();
}

void EventBrowser::on_toolButton_clicked()
{
	ui->jumpStrip->hide();
	ui->findStrip->hide();
	ui->bookmarkStrip->show();
}

static void SetDrawcallTimes(QTreeWidgetItem *node, const rdctype::array<CounterResult> &results)
{
	if(node == NULL) return;

	// parent nodes take the value of the sum of their children
	double duration = 0.0;

	// look up leaf nodes in the dictionary
	if(node->childCount() == 0)
	{
		uint eid = node->data(COL_EID, Qt::UserRole).toUInt();

		duration = -1.0;

		for(int32_t i = 0; i < results.count; i++)
		{
			if(results[i].eventID == eid)
				duration = results[i].value.d;
		}

		node->setText(COL_DURATION, duration < 0.0f ? "" : QString::number(duration*1000000.0));
		node->setData(COL_DURATION, Qt::UserRole, QVariant(duration));

		return;
	}

	for(int i = 0; i < node->childCount(); i++)
	{
		SetDrawcallTimes(node->child(i), results);

		double nd = node->child(i)->data(COL_DURATION, Qt::UserRole).toDouble();

		if(nd > 0.0)
			duration += nd;
	}

	node->setText(COL_DURATION, duration < 0.0f ? "" : QString::number(duration*1000000.0));
	node->setData(COL_DURATION, Qt::UserRole, QVariant(duration));
}

void EventBrowser::on_timeDraws_clicked()
{
	m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *r) {

		uint32_t counters[] = { eCounter_EventGPUDuration };

		rdctype::array<CounterResult> results;
		r->FetchCounters(m_Core->CurFrame(), 0, ~0U, counters, 1, &results);

		GUIInvoke::blockcall([this, results]() {
			SetDrawcallTimes(ui->events->topLevelItem(0), results);
			_CrtCheckMemory();
		});
	});
}

void EventBrowser::on_events_itemSelectionChanged()
{
	if(ui->events->selectedItems().empty()) return;

	uint EID = ui->events->selectedItems()[0]->data(COL_EID, Qt::UserRole).toUInt();

	m_Core->SetEventID(this, 0, EID);
}

void EventBrowser::hideFindJump()
{
	ui->jumpStrip->hide();
	ui->findStrip->hide();
}

void EventBrowser::on_jumpToEID_returnPressed()
{
	bool ok = false;
	uint eid = ui->findEvent->text().toUInt(&ok);
	if(ok)
	{
		//SelectEvent(0, eid);
	}
}

void EventBrowser::on_findEvent_returnPressed()
{

}
