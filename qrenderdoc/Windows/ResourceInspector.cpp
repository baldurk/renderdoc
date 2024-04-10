/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "ResourceInspector.h"
#include <QCollator>
#include <QKeyEvent>
#include <QMenu>
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "toolwindowmanager/ToolWindowManagerArea.h"
#include "ui_ResourceInspector.h"

static const int ResourceIdRole = Qt::UserRole;
static const int FilterRole = Qt::UserRole + 1;
static const int LastAccessSortRole = Qt::UserRole + 2;

class ResourceListItemModel : public QAbstractItemModel
{
public:
  ResourceListItemModel(QWidget *parent, ICaptureContext &ctx)
      : QAbstractItemModel(parent), m_Ctx(ctx)
  {
  }

  void reset()
  {
    emit beginResetModel();
    emit endResetModel();
  }

  void bumpLastUse(ResourceId id) { m_LastUse[id] = ++m_LastUseIdx; }
  void resetLastUse()
  {
    m_LastUseIdx = 1;
    m_LastUse.clear();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount())
      return QModelIndex();

    return createIndex(row, 0);
  }

  QModelIndex parent(const QModelIndex &index) const override { return QModelIndex(); }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return m_Ctx.GetResources().count();
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 1; }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      const rdcarray<ResourceDescription> &resources = m_Ctx.GetResources();

      if(index.row() < resources.count())
      {
        const ResourceDescription &desc = resources[index.row()];

        if(role == Qt::DisplayRole)
          return m_Ctx.GetResourceName(desc.resourceId);

        if(role == ResourceIdRole)
          return QVariant::fromValue(desc.resourceId);

        if(role == FilterRole)
          return ToQStr(desc.type) + lit(" ") + m_Ctx.GetResourceName(desc.resourceId);

        if(role == LastAccessSortRole)
          return m_LastUse[desc.resourceId];
      }
    }

    return QVariant();
  }

private:
  ICaptureContext &m_Ctx;

  QMap<ResourceId, uint32_t> m_LastUse;
  uint32_t m_LastUseIdx = 1;
};

bool ResourceSorterModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
  if(m_Sort == SortType::Creation)
  {
    return source_left.data(ResourceIdRole).value<ResourceId>() <
           source_right.data(ResourceIdRole).value<ResourceId>();
  }
  else if(m_Sort == SortType::LastAccess)
  {
    uint a = source_left.data(LastAccessSortRole).toUInt();
    uint b = source_right.data(LastAccessSortRole).toUInt();

    // if they're different, sort by access. Otherwise fall through to alphabetical
    // we invert the reason, so that high values (recent access) are first
    if(a != b)
      return a > b;

    return QCollatorSortFilterProxyModel::lessThan(source_left, source_right);
  }

  return QCollatorSortFilterProxyModel::lessThan(source_left, source_right);
}

ResourceInspector::ResourceInspector(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::ResourceInspector), m_Ctx(ctx)
{
  ui->setupUi(this);

  SetResourceNameDisplay(tr("No Resource Selected"));

  ui->resetName->hide();
  ui->resourceNameEdit->hide();
  ui->renameResource->setEnabled(false);

  ui->viewContents->hide();

  m_ResourceModel = new ResourceListItemModel(this, m_Ctx);

  m_FilterModel = new ResourceSorterModel(this);
  m_FilterModel->setSourceModel(m_ResourceModel);
  m_FilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_FilterModel->setFilterRole(FilterRole);
  m_FilterModel->sort(0);
  m_FilterModel->collator()->setNumericMode(true);
  m_FilterModel->collator()->setCaseSensitivity(Qt::CaseInsensitive);

  ui->sortType->addItems(
      {tr("Sort alphabetically"), tr("Sort by creation time"), tr("Sort by recently viewed")});
  ui->sortType->adjustSize();

  ui->resourceList->setModel(m_FilterModel);

  m_ChunksModel = new StructuredDataItemModel(this);
  ui->initChunks->setModel(m_ChunksModel);
  m_ChunksModel->setColumns({tr("Parameter"), tr("Value")},
                            {StructuredDataItemModel::Name, StructuredDataItemModel::Value});

  m_delegate = new RichTextViewDelegate(ui->initChunks);
  ui->initChunks->setItemDelegate(m_delegate);

  ui->initChunks->header()->resizeSection(0, 200);

  ui->initChunks->setFont(Formatter::PreferredFont());
  ui->relatedResources->setFont(Formatter::PreferredFont());
  ui->resourceUsage->setFont(Formatter::PreferredFont());

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->relatedResources->setHeader(header);

    ui->relatedResources->setColumns({tr("Type"), tr("Resource")});
    header->setColumnStretchHints({-1, 1});
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->resourceUsage->setHeader(header);

    ui->resourceUsage->setColumns({tr("EID"), tr("Usage")});
    header->setColumnStretchHints({-1, 1});
  }

  QObject::connect(ui->resourceList, &QListView::activated, this,
                   &ResourceInspector::resource_doubleClicked);
  QObject::connect(ui->relatedResources, &QTreeView::activated, this,
                   &ResourceInspector::resource_doubleClicked);

  ui->dockarea->addToolWindow(ui->resourceListWidget, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(ui->resourceListWidget, ToolWindowManager::HideCloseButton);

  ui->dockarea->addToolWindow(
      ui->relatedResources,
      ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                       ui->dockarea->areaOf(ui->resourceListWidget), 0.75f));
  ui->dockarea->setToolWindowProperties(ui->relatedResources, ToolWindowManager::HideCloseButton);

  ui->dockarea->addToolWindow(ui->initChunks, ToolWindowManager::AreaReference(
                                                  ToolWindowManager::BottomOf,
                                                  ui->dockarea->areaOf(ui->relatedResources), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->initChunks, ToolWindowManager::HideCloseButton);

  ui->dockarea->addToolWindow(
      ui->resourceUsage,
      ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                       ui->dockarea->areaOf(ui->relatedResources), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->resourceUsage, ToolWindowManager::HideCloseButton);

  ui->dockarea->setAllowFloatingWindow(false);

  ui->relatedResources->setWindowTitle(tr("Related Resources"));
  ui->initChunks->setWindowTitle(tr("Resource Initialisation Parameters"));
  ui->resourceUsage->setWindowTitle(tr("Usage in Frame"));
  ui->resourceListWidget->setWindowTitle(tr("Resource List"));

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(3, 3, 3, 3);

  vertical->addWidget(ui->titleWidget);
  vertical->addWidget(ui->dockarea);

  ui->resourceListFilter->setPlaceholderText(tr("Filter..."));

  Inspect(ResourceId());

  m_Ctx.AddCaptureViewer(this);
}

ResourceInspector::~ResourceInspector()
{
  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void ResourceInspector::Inspect(ResourceId id)
{
  if(m_Resource == id)
    return;

  // cancel any rename in progress
  ui->resourceNameEdit->hide();
  ui->resourceName->show();

  if(m_Resource != ResourceId())
    ui->initChunks->saveExpansion(ui->initChunks->getInternalExpansion(qHash(ToQStr(m_Resource))), 0);

  m_Resource = id;

  ui->viewContents->setVisible(m_Ctx.GetTexture(id) || m_Ctx.GetBuffer(id) ||
                               m_Ctx.GetDescriptorStore(id));

  m_Entries.clear();

  if(m_ResourceCacheID != m_Ctx.ResourceNameCacheID())
  {
    m_ResourceCacheID = m_Ctx.ResourceNameCacheID();
    m_ResourceModel->reset();
  }

  m_ResourceModel->bumpLastUse(id);

  m_FilterModel->invalidate();
  m_FilterModel->sort(0);

  if(m_Ctx.HasResourceCustomName(id))
    ui->resetName->show();
  else
    ui->resetName->hide();

  ui->initChunks->setUpdatesEnabled(false);
  ui->resourceUsage->clear();

  const SDFile &file = m_Ctx.GetStructuredFile();
  const ResourceDescription *desc = m_Ctx.GetResource(id);

  m_Ctx.Replay().AsyncInvoke([this, id](IReplayController *r) {
    rdcarray<EventUsage> usage = r->GetUsage(id);

    rdcarray<ShaderEntryPoint> entries = r->GetShaderEntryPoints(id);

    GUIInvoke::call(this, [this, entries, usage] {
      if(!entries.isEmpty())
      {
        m_Entries = entries;
        ui->viewContents->setVisible(true);
      }

      ui->resourceUsage->beginUpdate();

      CombineUsageEvents(m_Ctx, usage, [this](uint32_t startEID, uint32_t endEID, ResourceUsage use) {
        QString text;

        if(startEID == endEID)
          text = QFormatStr("EID %1").arg(startEID);
        else
          text = QFormatStr("EID %1-%2").arg(startEID).arg(endEID);

        RDTreeWidgetItem *item =
            new RDTreeWidgetItem({text, ToQStr(use, m_Ctx.APIProps().pipelineType)});
        item->setData(0, ResourceIdRole, QVariant(endEID));
        item->setData(1, ResourceIdRole, QVariant(endEID));

        ui->resourceUsage->addTopLevelItem(item);
      });

      ui->resourceUsage->endUpdate();
    });
  });

  if(desc)
  {
    ANALYTIC_SET(UIFeatures.ResourceInspect, true);

    SetResourceNameDisplay(m_Ctx.GetResourceName(id));

    ui->relatedResources->beginUpdate();
    ui->relatedResources->clear();
    for(ResourceId parent : desc->parentResources)
    {
      RDTreeWidgetItem *item = new RDTreeWidgetItem({tr("Parent"), parent});
      item->setData(0, ResourceIdRole, QVariant::fromValue(parent));
      item->setData(1, ResourceIdRole, QVariant::fromValue(parent));
      ui->relatedResources->addTopLevelItem(item);
    }

    // sort the derived resources by name. Cache the names once, then sort by them
    QVector<QPair<ResourceId, QString>> derivedResources;

    for(ResourceId derived : desc->derivedResources)
      derivedResources.push_back(qMakePair(derived, m_Ctx.GetResourceName(derived)));

    std::sort(derivedResources.begin(), derivedResources.end(),
              [](const QPair<ResourceId, QString> &a, const QPair<ResourceId, QString> &b) -> bool {
                return a.second < b.second;
              });

    for(const QPair<ResourceId, QString> &derived : derivedResources)
    {
      RDTreeWidgetItem *item = new RDTreeWidgetItem({tr("Derived"), derived.first});
      item->setData(0, ResourceIdRole, QVariant::fromValue(derived.first));
      item->setData(1, ResourceIdRole, QVariant::fromValue(derived.first));
      ui->relatedResources->addTopLevelItem(item);
    }
    ui->relatedResources->endUpdate();

    rdcarray<SDObject *> objs;

    for(uint32_t chunk : desc->initialisationChunks)
    {
      if(chunk < file.chunks.size())
        objs.push_back(file.chunks[chunk]);
      else
        qCritical() << "Invalid chunk index" << chunk;
    }

    m_ChunksModel->setObjects(objs);
  }
  else
  {
    m_Resource = ResourceId();
    SetResourceNameDisplay(tr("No Resource Selected"));
  }

  ui->initChunks->setUpdatesEnabled(true);

  if(m_Resource != ResourceId())
    ui->initChunks->applyExpansion(ui->initChunks->getInternalExpansion(qHash(ToQStr(m_Resource))),
                                   0);
}

void ResourceInspector::RevealParameter(SDObject *param)
{
  if(!param)
    return;

  rdcarray<SDObject *> hierarchy;
  while(param)
  {
    hierarchy.push_back(param);
    param = param->GetParent();
  }

  SDObject *current = hierarchy.back();
  hierarchy.pop_back();

  int rootIdx = m_ChunksModel->objects().indexOf(current);

  if(rootIdx >= 0)
  {
    QModelIndex parent = m_ChunksModel->index(rootIdx, 0);

    while(parent.isValid())
    {
      ui->initChunks->expand(parent);

      SDObject *next = hierarchy.back();
      hierarchy.pop_back();

      QModelIndex item;

      for(size_t i = 0; i < current->NumChildren(); i++)
      {
        if(current->GetChild(i) == next)
        {
          current = next;
          item = parent.child((int)i, 0);
          break;
        }
      }

      parent = item;

      if(hierarchy.empty())
        break;
    }

    ui->initChunks->selectionModel()->select(
        parent, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    ui->initChunks->scrollTo(parent);
  }
}

void ResourceInspector::OnCaptureLoaded()
{
  ui->renameResource->setEnabled(true);

  m_ResourceModel->reset();
  m_ResourceCacheID = m_Ctx.ResourceNameCacheID();
}

void ResourceInspector::SetResourceNameDisplay(const QString &name)
{
#if defined(RELEASE)
  ui->resourceName->setText(name);
#else
  if(m_Resource != ResourceId())
    ui->resourceName->setText(name + QFormatStr(" (%1)").arg(ToQStr(m_Resource)));
  else
    ui->resourceName->setText(name);
#endif
}

void ResourceInspector::OnCaptureClosed()
{
  m_Resource = ResourceId();

  ui->renameResource->setEnabled(false);
  ui->resetName->hide();

  SetResourceNameDisplay(tr("No Resource Selected"));

  ui->viewContents->hide();

  m_ResourceModel->reset();
  m_ResourceModel->resetLastUse();

  m_ChunksModel->setObjects({});
  ui->initChunks->clearInternalExpansions();
  ui->relatedResources->clear();
  ui->resourceUsage->clear();
}

void ResourceInspector::OnEventChanged(uint32_t eventId)
{
  Inspect(m_Resource);

  if(m_ResourceCacheID != m_Ctx.ResourceNameCacheID())
  {
    m_ResourceCacheID = m_Ctx.ResourceNameCacheID();
    m_ResourceModel->reset();
    SetResourceNameDisplay(m_Ctx.GetResourceName(m_Resource));
  }
}

void ResourceInspector::on_renameResource_clicked()
{
  if(!ui->resourceNameEdit->isVisible())
  {
    ui->resourceNameEdit->setText(m_Ctx.GetResourceNameUnsuffixed(m_Resource).trimmed());
    ui->resourceName->hide();
    ui->resourceNameEdit->show();
    ui->resourceNameEdit->setFocus();
  }
  else
  {
    QString name = ui->resourceNameEdit->text();

    // apply the edit
    m_Ctx.SetResourceCustomName(m_Resource, name);

    SetResourceNameDisplay(m_Ctx.GetResourceName(m_Resource));

    ui->resourceNameEdit->hide();
    ui->resourceName->show();

    ui->resetName->show();
  }
}

void ResourceInspector::on_resourceNameEdit_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    // throw away the edit and show the name again
    ui->resourceNameEdit->hide();
    ui->resourceName->show();
  }
  else if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    // apply the edit
    on_renameResource_clicked();
  }
}

void ResourceInspector::on_resetName_clicked()
{
  m_Ctx.SetResourceCustomName(m_Resource, QString());

  SetResourceNameDisplay(m_Ctx.GetResourceName(m_Resource));

  ui->resetName->hide();

  // force a refresh to pick up the new name
  ResourceId id = m_Resource;
  m_Resource = ResourceId();
  Inspect(id);
}

void ResourceInspector::on_sortType_currentIndexChanged(int index)
{
  m_FilterModel->setSortType((ResourceSorterModel::SortType)index);
}

void ResourceInspector::on_cancelResourceListFilter_clicked()
{
  ui->resourceListFilter->setText(QString());
}

void ResourceInspector::on_resourceListFilter_textChanged(const QString &text)
{
  m_FilterModel->setFilterFixedString(text);
}

void ResourceInspector::resource_doubleClicked(const QModelIndex &index)
{
  ResourceId id = index.model()->data(index, ResourceIdRole).value<ResourceId>();
  Inspect(id);

  HighlightUsage();
}

void ResourceInspector::on_viewContents_clicked()
{
  TextureDescription *tex = m_Ctx.GetTexture(m_Resource);
  BufferDescription *buf = m_Ctx.GetBuffer(m_Resource);
  DescriptorStoreDescription *desc = m_Ctx.GetDescriptorStore(m_Resource);

  if(tex)
  {
    if(tex->type == TextureType::Buffer)
    {
      IBufferViewer *viewer = m_Ctx.ViewTextureAsBuffer(
          tex->resourceId, Subresource(), BufferFormatter::GetTextureFormatString(*tex));

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
    else
    {
      if(!m_Ctx.HasTextureViewer())
        m_Ctx.ShowTextureViewer();
      ITextureViewer *viewer = m_Ctx.GetTextureViewer();
      viewer->ViewTexture(tex->resourceId, CompType::Typeless, true);
    }
  }
  else if(buf)
  {
    IBufferViewer *viewer = m_Ctx.ViewBuffer(0, buf->length, buf->resourceId);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
  else if(desc)
  {
    IDescriptorViewer *viewer = m_Ctx.ViewDescriptorStore(desc->resourceId);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
  else if(!m_Entries.isEmpty())
  {
    ShaderEntryPoint entry = m_Entries[0];

    if(m_Entries.count() > 1)
    {
      // TODO need to let the user choose the entry point
    }

    // TODO allow choosing parent pipeline?
    ResourceId pipeid;
    ResourceId id = m_Resource;
    ICaptureContext *ctx = &m_Ctx;
    m_Ctx.Replay().AsyncInvoke([this, ctx, pipeid, id, entry](IReplayController *r) {
      const ShaderReflection *refl = r->GetShader(pipeid, id, entry);

      if(!refl)
        return;

      GUIInvoke::call(this, [ctx, refl] {
        IShaderViewer *viewer = ctx->ViewShader(refl, ResourceId());

        ctx->AddDockWindow(viewer->Widget(), DockReference::MainToolArea, NULL);
      });
    });
  }
}

void ResourceInspector::on_resourceUsage_doubleClicked(const QModelIndex &index)
{
  uint32_t eid = index.model()->data(index, ResourceIdRole).value<uint32_t>();
  m_Ctx.SetEventID({}, eid, eid);
}

void ResourceInspector::enterEvent(QEvent *event)
{
  HighlightUsage();
}

void ResourceInspector::showEvent(QShowEvent *event)
{
  HighlightUsage();
}

void ResourceInspector::HighlightUsage()
{
  if(m_Resource != ResourceId() && m_Ctx.HasTimelineBar())
    m_Ctx.GetTimelineBar()->HighlightResourceUsage(m_Resource);
}
