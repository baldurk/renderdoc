#include "EventBrowser.h"
#include "ui_EventBrowser.h"
#include "renderdoc_replay.h"

extern ReplayOutput *out;
extern ReplayRenderer *renderer;
extern TextureDisplay d;
extern QWidget *texviewer;

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

EventBrowser::EventBrowser(QWidget *parent) :
  QFrame(parent),
  ui(new Ui::EventBrowser)
{
  ui->setupUi(this);

  rdctype::array<FetchDrawcall> draws;
  ReplayRenderer_GetDrawcalls(renderer, 0, &draws);

  rdctype::array<FetchFrameInfo> frameInfo;
  ReplayRenderer_GetFrameInfo(renderer, &frameInfo);

  QTreeWidgetItem *frame = new QTreeWidgetItem((QTreeWidget *)NULL, QStringList{QString("Frame #%1").arg(frameInfo[0].frameNumber), "", ""});

  QTreeWidgetItem *framestart = new QTreeWidgetItem(frame, QStringList{"Frame Start", "0", "0.0"});
  framestart->setData(0, Qt::UserRole, QVariant(0));

  uint lastEID = AddDrawcalls(frame, draws);
  frame->setData(0, Qt::UserRole, QVariant(lastEID));

  ui->events->insertTopLevelItem(0, frame);

  ui->events->expandItem(frame);

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
  delete ui;
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

    ReplayRenderer_SetFrameEvent(renderer, 0, EID);

    D3D11PipelineState state;
    ReplayRenderer_GetD3D11PipelineState(renderer, &state);

    d.texid = state.m_OM.RenderTargets[0].Resource;
    ReplayOutput_SetTextureDisplay(out, d);

    texviewer->update();
}
