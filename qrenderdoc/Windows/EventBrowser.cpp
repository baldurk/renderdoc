#include "EventBrowser.h"
#include "ui_EventBrowser.h"

#include "Code/Core.h"

uint AddDrawcalls(QTreeWidgetItem *parent, const rdctype::array<FetchDrawcall> &draws)
{
  uint lastEID = 0;

  for(int32_t i=0; i < draws.count; i++)
  {
    QTreeWidgetItem *child = new QTreeWidgetItem(parent, QStringList{QString(draws[i].name.elems), QString("%1").arg(draws[i].eventID), "0.0"});
    lastEID = AddDrawcalls(child, draws[i].children);
    if(lastEID == 0) lastEID = draws[i].eventID;
    child->setData(0, Qt::UserRole, QVariant(lastEID));
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

  ui->events->header()->resizeSection(1, 80);

  ui->events->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->events->header()->setSectionResizeMode(1, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(2, QHeaderView::Interactive);

  // we set up the name column first, EID second, so that the name column gets the
  // expand/collapse widgets. Then we need to put them back in order
  ui->events->header()->moveSection(0, 1);

  // Qt doesn't allow moving the column with the expand/collapse widgets, so this
  // becomes quickly infuriating to rearrange, just disable until that can be fixed.
  ui->events->header()->setSectionsMovable(false);
}

EventBrowser::~EventBrowser()
{
  m_Core->RemoveLogViewer(this);
  delete ui;
}

void EventBrowser::OnLogfileLoaded()
{
  QTreeWidgetItem *frame = new QTreeWidgetItem((QTreeWidget *)NULL, QStringList{QString("Frame #%1").arg(m_Core->FrameInfo()[0].frameNumber), "", ""});

  QTreeWidgetItem *framestart = new QTreeWidgetItem(frame, QStringList{"Frame Start", "0", "0.0"});
  framestart->setData(0, Qt::UserRole, QVariant(0));

  uint lastEID = AddDrawcalls(frame, m_Core->CurDrawcalls(0));
  frame->setData(0, Qt::UserRole, QVariant(lastEID));

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
}

void EventBrowser::on_gotoEID_clicked()
{
}

void EventBrowser::on_events_itemSelectionChanged()
{
    if(ui->events->selectedItems().empty()) return;

    uint EID = ui->events->selectedItems()[0]->data(0, Qt::UserRole).toUInt();

    m_Core->SetEventID(this, 0, EID);
}
