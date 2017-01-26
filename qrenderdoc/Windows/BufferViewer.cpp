/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "BufferViewer.h"
#include <QFontDatabase>
#include <QMouseEvent>
#include <QTimer>
#include "ui_BufferViewer.h"

class CameraWrapper
{
public:
  virtual ~CameraWrapper() {}
  virtual bool Update(QRect winSize) = 0;
  virtual Camera *camera() = 0;

  virtual void MouseWheel(QWheelEvent *e) = 0;

  virtual void MouseClick(QMouseEvent *e) { m_DragStartPos = e->pos(); }
  virtual void MouseMove(QMouseEvent *e)
  {
    if(e->buttons() & Qt::LeftButton)
    {
      if(m_DragStartPos.x() < 0)
      {
        m_DragStartPos = e->pos();
      }

      m_DragStartPos = e->pos();
    }
    else
    {
      m_DragStartPos = QPoint(-1, -1);
    }
  }

  virtual void KeyUp(QKeyEvent *e)
  {
    if(e->key() == Qt::Key_A || e->key() == Qt::Key_D)
      setMove(Direction::Horiz, 0);
    if(e->key() == Qt::Key_Q || e->key() == Qt::Key_E)
      setMove(Direction::Vert, 0);
    if(e->key() == Qt::Key_W || e->key() == Qt::Key_S)
      setMove(Direction::Fwd, 0);

    if(e->modifiers() && Qt::ShiftModifier)
      m_CurrentSpeed = 3.0f;
    else
      m_CurrentSpeed = 1.0f;
  }

  virtual void KeyDown(QKeyEvent *e)
  {
    if(e->key() == Qt::Key_W)
      setMove(Direction::Fwd, 1);
    if(e->key() == Qt::Key_S)
      setMove(Direction::Fwd, -1);
    if(e->key() == Qt::Key_Q)
      setMove(Direction::Vert, 1);
    if(e->key() == Qt::Key_E)
      setMove(Direction::Vert, -1);
    if(e->key() == Qt::Key_D)
      setMove(Direction::Horiz, 1);
    if(e->key() == Qt::Key_A)
      setMove(Direction::Horiz, -1);

    if(e->modifiers() && Qt::ShiftModifier)
      m_CurrentSpeed = 3.0f;
    else
      m_CurrentSpeed = 1.0f;
  }

  float SpeedMultiplier = 0.05f;

protected:
  enum class Direction
  {
    Fwd,
    Horiz,
    Vert,
    Num
  };

  int move(Direction dir) { return m_CurrentMove[(int)dir]; }
  float currentSpeed() { return m_CurrentSpeed * SpeedMultiplier; }
  QPoint dragStartPos() { return m_DragStartPos; }
private:
  float m_CurrentSpeed = 1.0f;
  int m_CurrentMove[(int)Direction::Num] = {0, 0, 0};

  void setMove(Direction dir, int val) { m_CurrentMove[(int)dir] = val; }
  QPoint m_DragStartPos = QPoint(-1, -1);
};

class ArcballWrapper : public CameraWrapper
{
public:
  ArcballWrapper() { m_Cam = Camera_InitArcball(); }
  virtual ~ArcballWrapper() { Camera_Shutdown(m_Cam); }
  Camera *camera() override { return m_Cam; }
  void Reset(FloatVector pos, float dist)
  {
    Camera_ResetArcball(m_Cam);

    setLookAtPos(pos);
    SetDistance(dist);
  }

  void SetDistance(float dist)
  {
    m_Distance = qAbs(dist);
    Camera_SetArcballDistance(m_Cam, m_Distance);
  }

  bool Update(QRect size) override
  {
    m_WinSize = size;
    return false;
  }

  void MouseWheel(QWheelEvent *e) override
  {
    float mod = (1.0f - e->delta() / 2500.0f);

    SetDistance(qMax(1e-6f, m_Distance * mod));
  }

  void MouseMove(QMouseEvent *e) override
  {
    if(dragStartPos().x() > 0)
    {
      if(e->buttons() == Qt::MiddleButton ||
         (e->buttons() == Qt::LeftButton && e->modifiers() & Qt::AltModifier))
      {
        float xdelta = (float)(e->pos().x() - dragStartPos().x()) / 300.0f;
        float ydelta = (float)(e->pos().y() - dragStartPos().y()) / 300.0f;

        xdelta *= qMax(1.0f, m_Distance);
        ydelta *= qMax(1.0f, m_Distance);

        FloatVector pos, fwd, right, up;
        Camera_GetBasis(m_Cam, &pos, &fwd, &right, &up);

        m_LookAt.x -= right.x * xdelta;
        m_LookAt.y -= right.y * xdelta;
        m_LookAt.z -= right.z * xdelta;

        m_LookAt.x += up.x * ydelta;
        m_LookAt.y += up.y * ydelta;
        m_LookAt.z += up.z * ydelta;

        Camera_SetPosition(m_Cam, m_LookAt.x, m_LookAt.y, m_LookAt.z);
      }
      else if(e->buttons() == Qt::LeftButton)
      {
        RotateArcball(dragStartPos(), e->pos());
      }
    }

    CameraWrapper::MouseMove(e);
  }

  FloatVector lookAtPos() { return m_LookAt; }
  void setLookAtPos(const FloatVector &v)
  {
    m_LookAt = v;
    Camera_SetPosition(m_Cam, v.x, v.y, v.z);
  }

private:
  Camera *m_Cam;

  QRect m_WinSize;

  float m_Distance = 10.0f;
  FloatVector m_LookAt;

  void RotateArcball(QPoint from, QPoint to)
  {
    float ax = ((float)from.x() / (float)m_WinSize.width()) * 2.0f - 1.0f;
    float ay = ((float)from.y() / (float)m_WinSize.height()) * 2.0f - 1.0f;
    float bx = ((float)to.x() / (float)m_WinSize.width()) * 2.0f - 1.0f;
    float by = ((float)to.y() / (float)m_WinSize.height()) * 2.0f - 1.0f;

    // this isn't a 'true arcball' but it handles extreme aspect ratios
    // better. We basically 'centre' around the from point always being
    // 0,0 (straight out of the screen) as if you're always dragging
    // the arcball from the middle, and just use the relative movement
    int minDimension = qMin(m_WinSize.width(), m_WinSize.height());

    ax = ay = 0;
    bx = ((float)(to.x() - from.x()) / (float)minDimension) * 2.0f;
    by = ((float)(to.y() - from.y()) / (float)minDimension) * 2.0f;

    ay = -ay;
    by = -by;

    Camera_RotateArcball(m_Cam, ax, ay, bx, by);
  }
};

class FlycamWrapper : public CameraWrapper
{
public:
  FlycamWrapper() { m_Cam = Camera_InitFPSLook(); }
  virtual ~FlycamWrapper() { Camera_Shutdown(m_Cam); }
  Camera *camera() override { return m_Cam; }
  void Reset(FloatVector pos)
  {
    m_Position = pos;
    m_Rotation = FloatVector();

    Camera_SetPosition(m_Cam, m_Position.x, m_Position.y, m_Position.z);
    Camera_SetFPSRotation(m_Cam, m_Rotation.x, m_Rotation.y, m_Rotation.z);
  }

  bool Update(QRect size) override
  {
    FloatVector pos, fwd, right, up;
    Camera_GetBasis(m_Cam, &pos, &fwd, &right, &up);

    float speed = currentSpeed();

    int horizMove = move(CameraWrapper::Direction::Horiz);
    if(horizMove)
    {
      m_Position.x += right.x * speed * (float)horizMove;
      m_Position.y += right.y * speed * (float)horizMove;
      m_Position.z += right.z * speed * (float)horizMove;
    }

    int vertMove = move(CameraWrapper::Direction::Vert);
    if(vertMove)
    {
      // this makes less intuitive sense, instead go 'absolute' up
      // m_Position.x += up.x * speed * (float)vertMove;
      // m_Position.y += up.y * speed * (float)vertMove;
      // m_Position.z += up.z * speed * (float)vertMove;

      m_Position.y += speed * (float)vertMove;
    }

    int fwdMove = move(CameraWrapper::Direction::Fwd);
    if(fwdMove)
    {
      m_Position.x += fwd.x * speed * (float)fwdMove;
      m_Position.y += fwd.y * speed * (float)fwdMove;
      m_Position.z += fwd.z * speed * (float)fwdMove;
    }

    if(horizMove || vertMove || fwdMove)
    {
      Camera_SetPosition(m_Cam, m_Position.x, m_Position.y, m_Position.z);
      return true;
    }

    return false;
  }

  void MouseWheel(QWheelEvent *e) override {}
  void MouseMove(QMouseEvent *e) override
  {
    if(dragStartPos().x() > 0 && e->buttons() == Qt::LeftButton)
    {
      m_Rotation.y -= (float)(e->pos().x() - dragStartPos().x()) / 300.0f;
      m_Rotation.x -= (float)(e->pos().y() - dragStartPos().y()) / 300.0f;

      Camera_SetFPSRotation(m_Cam, m_Rotation.x, m_Rotation.y, m_Rotation.z);
    }

    CameraWrapper::MouseMove(e);
  }

private:
  Camera *m_Cam;

  FloatVector m_Position, m_Rotation;
};

struct BufferData
{
  BufferData()
  {
    data = end = NULL;
    stride = 0;
  }

  byte *data;
  byte *end;
  size_t stride;
};

class BufferItemModel : public QAbstractItemModel
{
public:
  BufferItemModel(RDTableView *v, QObject *parent) : QAbstractItemModel(parent)
  {
    view = v;
    view->setModel(this);
  }
  void beginReset() { emit beginResetModel(); }
  void endReset()
  {
    cacheColumns();
    emit endResetModel();
  }
  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount())
      return QModelIndex();

    return createIndex(row, column);
  }

  QModelIndex parent(const QModelIndex &index) const override { return QModelIndex(); }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return numRows; }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return columnLookup.count() + 2;
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(section < columnCount() && orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      if(section == 0)
      {
        return "VTX";
      }
      else if(section == 1)
      {
        return "IDX";
      }
      else
      {
        const FormatElement &el = columnForIndex(section);

        if(el.format.compCount == 1)
          return el.name;

        QChar comps[] = {'x', 'y', 'z', 'w'};

        return QString("%1.%2").arg(el.name).arg(comps[componentForIndex(section)]);
      }
    }

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      if(role == Qt::SizeHintRole)
      {
        QStyleOptionViewItem opt = view->viewOptions();
        opt.features |= QStyleOptionViewItem::HasDisplay;

        // pad these columns to allow for sufficiently wide data
        if(index.column() < 2)
          opt.text = "999999";
        else
          opt.text = data(index).toString();
        opt.styleObject = NULL;

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        return style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), opt.widget);
      }

      if(role == Qt::DisplayRole)
      {
        uint32_t row = index.row();
        int col = index.column();

        if(col >= 0 && col < columnCount() && row < numRows)
        {
          if(col == 0)
            return row;

          if(indices.data)
          {
            byte *idx = indices.data + row * sizeof(uint32_t);
            if(idx + 1 > indices.end)
              return QVariant();

            row = *(uint32_t *)idx;
          }

          if(col == 1)
            return row;

          const FormatElement &el = columnForIndex(col);

          if(el.buffer < buffers.size())
          {
            const byte *data = buffers[el.buffer].data;
            const byte *end = buffers[el.buffer].end;

            data += buffers[el.buffer].stride * row;

            data += el.offset;

            // only slightly wasteful, we need to fetch all variants together
            // since some formats are packed and can't be read individually
            QVariantList list = el.GetVariants(data, end);

            int comp = componentForIndex(col);

            if(comp < list.count())
            {
              QVariant &v = list[comp];

              QString ret;

              QMetaType::Type vt = (QMetaType::Type)v.type();

              if(vt == QMetaType::Double)
              {
                double d = v.toDouble();
                // pad with space on left if sign is missing, to better align
                if(d < 0.0)
                  ret = Formatter::Format(d);
                else if(d > 0.0)
                  ret = " " + Formatter::Format(d);
                else if(qIsNaN(d))
                  ret = " NaN";
                else
                  // force negative and positive 0 together
                  ret = " " + Formatter::Format(0.0);
              }
              else if(vt == QMetaType::Float)
              {
                float f = v.toFloat();
                // pad with space on left if sign is missing, to better align
                if(f < 0.0)
                  ret = Formatter::Format(f);
                else if(f > 0.0)
                  ret = " " + Formatter::Format(f);
                else if(qIsNaN(f))
                  ret = " NaN";
                else
                  // force negative and positive 0 together
                  ret = " " + Formatter::Format(0.0);
              }
              else if(vt == QMetaType::UInt || vt == QMetaType::UShort || vt == QMetaType::UChar)
              {
                ret = Formatter::Format(v.toUInt(), el.hex);
              }
              else if(vt == QMetaType::Int || vt == QMetaType::Short || vt == QMetaType::SChar)
              {
                int i = v.toInt();
                if(i > 0)
                  ret = " " + Formatter::Format(i);
                else
                  ret = Formatter::Format(i);
              }
              else
                ret = v.toString();

              return ret;
            }
          }
        }
      }
    }

    return QVariant();
  }

  RDTableView *view = NULL;

  uint32_t numRows = 0;
  BufferData indices;
  QList<FormatElement> columns;
  QList<BufferData> buffers;

private:
  // maps from column number (0-based from data, so excluding VTX/IDX columns)
  // to the column element in the columns list, and lists its component.
  //
  // So a float4, float3, int set of columns would be:
  // { 0, 0, 0, 0, 1, 1, 1, 2 };
  // { 0, 1, 2, 3, 0, 1, 2, 0 };
  QVector<int> columnLookup;
  QVector<int> componentLookup;

  const FormatElement &columnForIndex(int col) const { return columns[columnLookup[col - 2]]; }
  int componentForIndex(int col) const { return componentLookup[col - 2]; }
  void cacheColumns()
  {
    columnLookup.clear();
    columnLookup.reserve(columns.count() * 4);
    componentLookup.clear();
    componentLookup.reserve(columns.count() * 4);

    for(int i = 0; i < columns.count(); i++)
    {
      FormatElement &fmt = columns[i];

      uint32_t compCount;

      switch(fmt.format.specialFormat)
      {
        case eSpecial_BC6:
        case eSpecial_ETC2:
        case eSpecial_R11G11B10:
        case eSpecial_R5G6B5:
        case eSpecial_R9G9B9E5: compCount = 3; break;
        case eSpecial_BC1:
        case eSpecial_BC7:
        case eSpecial_BC3:
        case eSpecial_BC2:
        case eSpecial_R10G10B10A2:
        case eSpecial_R5G5B5A1:
        case eSpecial_R4G4B4A4:
        case eSpecial_ASTC: compCount = 4; break;
        case eSpecial_BC5:
        case eSpecial_R4G4:
        case eSpecial_D16S8:
        case eSpecial_D24S8:
        case eSpecial_D32S8: compCount = 2; break;
        case eSpecial_BC4:
        case eSpecial_S8: compCount = 1; break;
        case eSpecial_YUV:
        case eSpecial_EAC:
        default: compCount = fmt.format.compCount;
      }

      for(uint32_t c = 0; c < compCount; c++)
      {
        columnLookup.push_back(i);
        componentLookup.push_back((int)c);
      }
    }
  }
};

BufferViewer::BufferViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::BufferViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->vsinData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->vsoutData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->gsoutData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  ui->formatSpecifier->setVisible(false);
  ui->cameraControlsGroup->setVisible(false);

  ui->outputTabs->setWindowTitle(tr("Preview"));
  ui->dockarea->addToolWindow(ui->outputTabs, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(ui->outputTabs, ToolWindowManager::HideCloseButton);

  ui->vsinData->setWindowTitle(tr("VS Input"));
  ui->dockarea->addToolWindow(
      ui->vsinData, ToolWindowManager::AreaReference(ToolWindowManager::TopOf,
                                                     ui->dockarea->areaOf(ui->outputTabs), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->vsinData, ToolWindowManager::HideCloseButton);

  ui->vsoutData->setWindowTitle(tr("VS Output"));
  ui->dockarea->addToolWindow(
      ui->vsoutData, ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                                      ui->dockarea->areaOf(ui->vsinData), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->vsoutData, ToolWindowManager::HideCloseButton);

  ui->gsoutData->setWindowTitle(tr("GS/DS Output"));
  ui->dockarea->addToolWindow(
      ui->gsoutData, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                      ui->dockarea->areaOf(ui->vsoutData), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->gsoutData, ToolWindowManager::HideCloseButton);

  ToolWindowManager::raiseToolWindow(ui->vsoutData);

  ui->dockarea->setAllowFloatingWindow(false);
  ui->dockarea->setRubberBandLineWidth(50);

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(0, 0, 0, 0);

  vertical->addWidget(ui->meshToolbar);
  vertical->addWidget(ui->dockarea);

  ui->controlType->addItems({tr("Arcball"), tr("WASD")});
  ui->controlType->adjustSize();

  ui->drawRange->addItems({tr("Only this draw"), tr("Show previous instances"),
                           tr("Show all instances"), tr("Show whole pass")});
  ui->drawRange->adjustSize();

  ui->solidShading->addItems({tr("None"), tr("Solid Colour"), tr("Flat Shaded"), tr("Secondary")});
  ui->solidShading->adjustSize();

  ui->fovGuess->setValue(90.0);

  m_ModelVSIn = new BufferItemModel(ui->vsinData, this);
  m_ModelVSOut = new BufferItemModel(ui->vsoutData, this);
  m_ModelGSOut = new BufferItemModel(ui->gsoutData, this);

  m_Flycam = new FlycamWrapper();
  m_Arcball = new ArcballWrapper();
  m_CurrentCamera = m_Arcball;

  m_Arcball->Reset(FloatVector(), 10.0f);
  m_Flycam->Reset(FloatVector());

  m_Config.type = eMeshDataStage_VSIn;
  m_Config.ortho = false;
  m_Config.showPrevInstances = false;
  m_Config.showAllInstances = false;
  m_Config.showWholePass = false;
  m_Config.curInstance = 0;
  m_Config.showBBox = false;
  m_Config.solidShadeMode = eShade_None;
  m_Config.wireframeDraw = true;
  memset(&m_Config.position, 0, sizeof(m_Config.position));
  memset(&m_Config.second, 0, sizeof(m_Config.second));

  ui->outputTabs->setCurrentIndex(0);
  m_CurStage = eMeshDataStage_VSIn;

  QObject::connect(ui->vsinData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->vsoutData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->gsoutData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);

  QTimer *renderTimer = new QTimer(this);
  QObject::connect(renderTimer, &QTimer::timeout, this, &BufferViewer::render_timer);
  renderTimer->setSingleShot(false);
  renderTimer->setInterval(1000);
  renderTimer->start();

  Reset();

  m_Ctx->AddLogViewer(this);
}

BufferViewer::~BufferViewer()
{
  delete[] m_ModelVSIn->indices.data;

  for(auto vb : m_ModelVSIn->buffers)
    delete[] vb.data;

  delete[] m_ModelVSOut->indices.data;

  for(auto vb : m_ModelVSOut->buffers)
    delete[] vb.data;

  delete m_Arcball;
  delete m_Flycam;

  m_Ctx->windowClosed(this);
  m_Ctx->RemoveLogViewer(this);
  delete ui;
}

void BufferViewer::OnLogfileLoaded()
{
  Reset();

  WId renderID = ui->render->winId();

  m_Ctx->Renderer()->BlockInvoke([renderID, this](IReplayRenderer *r) {
    m_Output = r->CreateOutput(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(renderID),
                               eOutputType_MeshDisplay);

    ui->render->setOutput(m_Output);

    OutputConfig c = {eOutputType_MeshDisplay};
    m_Output->SetOutputConfig(c);

    RT_UpdateAndDisplay(r);
  });
}

void BufferViewer::OnLogfileClosed()
{
  Reset();
}

void BufferViewer::OnEventChanged(uint32_t eventID)
{
  ClearModels();

  memset(&m_VSIn, 0, sizeof(m_VSIn));
  memset(&m_PostVS, 0, sizeof(m_PostVS));
  memset(&m_PostGS, 0, sizeof(m_PostGS));

  CalcColumnWidth();

  ClearModels();

  m_ModelVSIn->beginReset();
  m_ModelVSOut->beginReset();
  m_ModelGSOut->beginReset();

  const FetchDrawcall *draw = m_Ctx->CurDrawcall();

  QVector<VertexInputAttribute> vinputs = m_Ctx->CurPipelineState.GetVertexInputs();

  m_ModelVSIn->columns.reserve(vinputs.count());

  for(const VertexInputAttribute &a : vinputs)
  {
    if(!a.Used)
      continue;

    FormatElement f(a.Name, a.VertexBuffer, a.RelativeByteOffset, a.PerInstance, a.InstanceRate,
                    false,    // row major matrix
                    1,        // matrix dimension
                    a.Format, false);

    m_ModelVSIn->columns.push_back(f);
  }

  if(draw == NULL)
    m_ModelVSIn->numRows = 0;
  else
    m_ModelVSIn->numRows = draw->numIndices;

  QVector<BoundVBuffer> vbs = m_Ctx->CurPipelineState.GetVBuffers();

  ResourceId ib;
  uint64_t ioffset = 0;
  m_Ctx->CurPipelineState.GetIBuffer(ib, ioffset);

  Viewport vp = m_Ctx->CurPipelineState.GetViewport(0);

  m_Config.fov = ui->fovGuess->value();
  m_Config.aspect = vp.width / vp.height;
  m_Config.highlightVert = 0;

  if(ui->aspectGuess->value() > 0.0)
    m_Config.aspect = ui->aspectGuess->value();

  if(ui->nearGuess->value() > 0.0)
    m_PostVS.nearPlane = m_PostGS.nearPlane = ui->nearGuess->value();

  if(ui->farGuess->value() > 0.0)
    m_PostVS.farPlane = m_PostGS.farPlane = ui->farGuess->value();

  if(draw == NULL)
  {
    m_VSIn.numVerts = 0;
    m_VSIn.topo = eTopology_TriangleList;
    m_VSIn.idxbuf = ResourceId();
    m_VSIn.idxoffs = 0;
    m_VSIn.idxByteWidth = 4;
    m_VSIn.baseVertex = 0;
  }
  else
  {
    m_VSIn.numVerts = draw->numIndices;
    m_VSIn.topo = draw->topology;
    m_VSIn.idxbuf = ib;
    m_VSIn.idxoffs = ioffset;
    m_VSIn.idxByteWidth = draw->indexByteWidth;
    m_VSIn.baseVertex = draw->baseVertex;
  }

  if(!vinputs.empty())
  {
    m_VSIn.buf = vbs[vinputs[0].VertexBuffer].Buffer;
    m_VSIn.offset = vbs[vinputs[0].VertexBuffer].ByteOffset;
    m_VSIn.stride = vbs[vinputs[0].VertexBuffer].ByteStride;

    m_VSIn.compCount = vinputs[0].Format.compCount;
    m_VSIn.compByteWidth = vinputs[0].Format.compByteWidth;
    m_VSIn.compType = vinputs[0].Format.compType;
  }

  ShaderReflection *vs = m_Ctx->CurPipelineState.GetShaderReflection(eShaderStage_Vertex);

  m_ModelVSOut->columns.clear();

  if(draw && vs)
  {
    m_ModelVSOut->columns.reserve(vs->OutputSig.count);

    int i = 0, posidx = -1;
    for(const SigParameter &sig : vs->OutputSig)
    {
      FormatElement f;

      f.buffer = 0;
      f.name = sig.varName.count > 0 ? ToQStr(sig.varName) : ToQStr(sig.semanticIdxName);
      f.format.compByteWidth = sizeof(float);
      f.format.compCount = sig.compCount;
      f.format.compType = sig.compType;
      f.format.special = false;
      f.format.rawType = 0;
      f.perinstance = false;
      f.instancerate = 1;
      f.rowmajor = false;
      f.matrixdim = 1;
      f.systemValue = sig.systemValue;

      if(f.systemValue == eAttr_Position)
        posidx = i;

      m_ModelVSOut->columns.push_back(f);

      i++;
    }

    i = 0;
    uint32_t offset = 0;
    for(const SigParameter &sig : vs->OutputSig)
    {
      uint numComps = sig.compCount;
      uint elemSize = sig.compType == eCompType_Double ? 8U : 4U;

      if(m_Ctx->CurPipelineState.HasAlignedPostVSData())
      {
        if(numComps == 2)
          offset = AlignUp(offset, 2U * elemSize);
        else if(numComps > 2)
          offset = AlignUp(offset, 4U * elemSize);
      }

      m_ModelVSOut->columns[i++].offset = offset;

      offset += numComps * elemSize;
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      FormatElement pos = m_ModelVSOut->columns[posidx];
      m_ModelVSOut->columns.insert(0, m_ModelVSOut->columns.takeAt(posidx));
    }
  }

  m_Ctx->Renderer()->AsyncInvoke([this, draw, vbs, ib, ioffset](IReplayRenderer *r) {

    rdctype::array<byte> idata;
    if(ib != ResourceId() && draw)
      r->GetBufferData(ib, ioffset + draw->indexOffset * draw->indexByteWidth,
                       draw->numIndices * draw->indexByteWidth, &idata);

    uint32_t *indices = NULL;
    m_ModelVSIn->indices = BufferData();
    if(draw && draw->indexByteWidth != 0 && idata.count != 0)
    {
      indices = new uint32_t[draw->numIndices];
      m_ModelVSIn->indices.data = (byte *)indices;
      m_ModelVSIn->indices.end = (byte *)(indices + draw->numIndices);
    }

    uint32_t maxIndex = qMax(1U, draw->numIndices) - 1;

    if(idata.count > 0)
    {
      if(draw->indexByteWidth == 1)
      {
        for(size_t i = 0; i < (size_t)idata.count && (uint32_t)i < draw->numIndices; i++)
        {
          indices[i] = (uint32_t)idata.elems[i];
          maxIndex = qMax(maxIndex, indices[i]);
        }
      }
      else if(draw->indexByteWidth == 2)
      {
        uint16_t *src = (uint16_t *)idata.elems;
        for(size_t i = 0;
            i < (size_t)idata.count / sizeof(uint16_t) && (uint32_t)i < draw->numIndices; i++)
        {
          indices[i] = (uint32_t)src[i];
          maxIndex = qMax(maxIndex, indices[i]);
        }
      }
      else if(draw->indexByteWidth == 4)
      {
        memcpy(indices, idata.elems, qMin((size_t)idata.count, draw->numIndices * sizeof(uint32_t)));

        for(uint32_t i = 0; i < draw->numIndices; i++)
          maxIndex = qMax(maxIndex, indices[i]);
      }
    }

    int vbIdx = 0;
    for(BoundVBuffer vb : vbs)
    {
      bool used = false;
      bool pi = false;
      bool pv = false;

      for(const FormatElement &col : m_ModelVSIn->columns)
      {
        if(col.buffer == vbIdx)
        {
          used = true;

          if(col.perinstance)
            pi = true;
          else
            pv = true;
        }
      }

      vbIdx++;

      uint32_t maxIdx = 0;
      uint32_t offset = 0;

      if(used && draw)
      {
        if(pi)
        {
          maxIdx = qMax(1U, draw->numInstances) - 1;
          offset = draw->instanceOffset;
        }
        if(pv)
        {
          maxIdx = qMax(maxIndex, maxIdx);
          offset = draw->vertexOffset;

          if(draw->baseVertex > 0)
            maxIdx += (uint32_t)draw->baseVertex;
        }

        if(pi && pv)
          qCritical() << "Buffer used for both instance and vertex rendering!";
      }

      BufferData buf = {};
      if(used)
      {
        rdctype::array<byte> data;
        r->GetBufferData(vb.Buffer, vb.ByteOffset + offset * vb.ByteStride,
                         (maxIdx + 1) * vb.ByteStride, &data);

        buf.data = new byte[data.count];
        memcpy(buf.data, data.elems, data.count);
        buf.end = buf.data + data.count;
        buf.stride = vb.ByteStride;
      }
      m_ModelVSIn->buffers.push_back(buf);
    }

    r->GetPostVSData(0, eMeshDataStage_VSOut, &m_PostVS);

    m_ModelVSOut->numRows = m_PostVS.numVerts;

    if(m_PostVS.idxbuf != ResourceId())
      r->GetBufferData(m_PostVS.idxbuf, ioffset + draw->indexOffset * draw->indexByteWidth,
                       draw->numIndices * draw->indexByteWidth, &idata);

    indices = NULL;
    m_ModelVSOut->indices = BufferData();
    if(draw && draw->indexByteWidth != 0 && idata.count != 0)
    {
      indices = new uint32_t[draw->numIndices];
      m_ModelVSOut->indices.data = (byte *)indices;
      m_ModelVSOut->indices.end = (byte *)(indices + draw->numIndices);
    }

    if(idata.count > 0)
    {
      if(draw->indexByteWidth == 1)
      {
        for(size_t i = 0; i < (size_t)idata.count && (uint32_t)i < draw->numIndices; i++)
          indices[i] = (uint32_t)idata.elems[i];
      }
      else if(draw->indexByteWidth == 2)
      {
        uint16_t *src = (uint16_t *)idata.elems;
        for(size_t i = 0;
            i < (size_t)idata.count / sizeof(uint16_t) && (uint32_t)i < draw->numIndices; i++)
          indices[i] = (uint32_t)src[i];
      }
      else if(draw->indexByteWidth == 4)
      {
        memcpy(indices, idata.elems, qMin((size_t)idata.count, draw->numIndices * sizeof(uint32_t)));
      }
    }

    if(m_PostVS.buf != ResourceId())
    {
      BufferData postvs = {};
      rdctype::array<byte> data;
      r->GetBufferData(m_PostVS.buf, 0, 0, &data);

      postvs.data = new byte[data.count];
      memcpy(postvs.data, data.elems, data.count);
      postvs.end = postvs.data + data.count;
      postvs.stride = m_PostVS.stride;
      m_ModelVSOut->buffers.push_back(postvs);
    }

    UpdateMeshConfig();

    RT_UpdateAndDisplay(r);

    GUIInvoke::call([this] {
      m_ModelVSIn->endReset();
      m_ModelVSOut->endReset();

      ApplyColumnWidths(m_ModelVSIn->columnCount(), ui->vsinData);
      ApplyColumnWidths(m_ModelVSOut->columnCount(), ui->vsoutData);
      ApplyColumnWidths(m_ModelGSOut->columnCount(), ui->gsoutData);
    });
  });
}

void BufferViewer::ApplyColumnWidths(int numColumns, RDTableView *view)
{
  view->setColumnWidth(0, m_IdxColWidth);
  view->setColumnWidth(1, m_IdxColWidth);

  for(int i = 2; i < numColumns; i++)
    view->setColumnWidth(i, m_DataColWidth);
}

void BufferViewer::UpdateMeshConfig()
{
  switch(m_CurStage)
  {
    case eMeshDataStage_VSIn: m_Config.position = m_VSIn; break;
    case eMeshDataStage_VSOut: m_Config.position = m_PostVS; break;
    case eMeshDataStage_GSOut: m_Config.position = m_PostGS; break;
    default: break;
  }
}

void BufferViewer::on_outputTabs_currentChanged(int index)
{
  ui->renderContainer->parentWidget()->layout()->removeWidget(ui->renderContainer);
  ui->outputTabs->widget(index)->layout()->addWidget(ui->renderContainer);

  if(index == 0)
    m_CurStage = eMeshDataStage_VSIn;
  else if(index == 1)
    m_CurStage = eMeshDataStage_VSOut;
  else if(index == 2)
    m_CurStage = eMeshDataStage_GSOut;

  UpdateMeshConfig();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx->LogLoaded())
    return;

  if(m_CurrentCamera)
    m_CurrentCamera->MouseMove(e);

  if(e->buttons() & Qt::RightButton)
    render_clicked(e);

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_clicked(QMouseEvent *e)
{
  if(!m_Ctx->LogLoaded())
    return;

  QPoint curpos = e->pos();

  if((e->buttons() & Qt::RightButton) && m_Output)
  {
    m_Ctx->Renderer()->AsyncInvoke([this, curpos](IReplayRenderer *r) {
      uint32_t instanceSelected = 0;
      uint32_t vertSelected = m_Output->PickVertex(m_Ctx->CurEvent(), (uint32_t)curpos.x(),
                                                   (uint32_t)curpos.y(), &instanceSelected);

      if(vertSelected != ~0U)
      {
        GUIInvoke::call([this, vertSelected] {
          int row = (int)vertSelected;

          BufferItemModel *model = NULL;
          if(ui->outputTabs->currentIndex() == 0)
            model = m_ModelVSIn;
          else if(ui->outputTabs->currentIndex() == 1)
            model = m_ModelVSOut;

          if(model && row >= 0 && row < model->rowCount())
          {
            model->view->scrollTo(model->index(row, 0));
            model->view->clearSelection();
            model->view->selectRow(row);
          }
        });
      }
    });
  }

  if(m_CurrentCamera)
    m_CurrentCamera->MouseClick(e);

  ui->render->setFocus();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_mouseWheel(QWheelEvent *e)
{
  if(m_CurrentCamera)
    m_CurrentCamera->MouseWheel(e);

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_keyPress(QKeyEvent *e)
{
  m_CurrentCamera->KeyDown(e);
}

void BufferViewer::render_keyRelease(QKeyEvent *e)
{
  m_CurrentCamera->KeyUp(e);
}

void BufferViewer::render_timer()
{
  if(m_CurrentCamera && m_CurrentCamera->Update(ui->render->rect()))
    INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::RT_UpdateAndDisplay(IReplayRenderer *)
{
  if(m_Output)
  {
    m_Config.cam = m_CurrentCamera->camera();
    m_Output->SetMeshDisplay(m_Config);
    m_Output->Display();
  }
}

void BufferViewer::Reset()
{
  m_Output = NULL;

  ClearModels();

  CaptureContext *ctx = m_Ctx;

  // while a log is loaded, pass NULL into the widget
  if(!ctx->LogLoaded())
    ctx = NULL;

  {
    CustomPaintWidget *render = new CustomPaintWidget(ctx, this);
    render->setObjectName(ui->render->objectName());
    render->setSizePolicy(ui->render->sizePolicy());
    delete ui->render;
    ui->render = render;
    ui->renderContainerGridLayout->addWidget(ui->render, 1, 1, 1, 1);
  }

  QObject::connect(ui->render, &CustomPaintWidget::mouseMove, this, &BufferViewer::render_mouseMove);
  QObject::connect(ui->render, &CustomPaintWidget::clicked, this, &BufferViewer::render_clicked);
  QObject::connect(ui->render, &CustomPaintWidget::keyPress, this, &BufferViewer::render_keyPress);
  QObject::connect(ui->render, &CustomPaintWidget::keyRelease, this,
                   &BufferViewer::render_keyRelease);
  QObject::connect(ui->render, &CustomPaintWidget::mouseWheel, this,
                   &BufferViewer::render_mouseWheel);

  ui->render->setColours(QColor::fromRgbF(0.57f, 0.57f, 0.57f, 1.0f),
                         QColor::fromRgbF(0.81f, 0.81f, 0.81f, 1.0f));
}

void BufferViewer::ClearModels()
{
  BufferItemModel *models[] = {m_ModelVSIn, m_ModelVSOut, m_ModelGSOut};

  for(BufferItemModel *m : models)
  {
    if(!m)
      continue;

    m->beginReset();

    delete[] m->indices.data;

    m->indices = BufferData();

    for(auto vb : m->buffers)
      delete[] vb.data;

    m->buffers.clear();
    m->columns.clear();
    m->numRows = 0;

    m->endReset();
  }
}

void BufferViewer::CalcColumnWidth()
{
  m_ModelVSIn->beginReset();

  ResourceFormat floatFmt;
  floatFmt.compByteWidth = 4;
  floatFmt.compType = eCompType_Float;
  floatFmt.compCount = 1;

  ResourceFormat intFmt;
  floatFmt.compByteWidth = 4;
  floatFmt.compType = eCompType_UInt;
  floatFmt.compCount = 1;

  FormatElement floatEl("ColumnSizeTest", 0, 0, false, 1, false, 1, floatFmt, false);
  FormatElement xintEl("ColumnSizeTest", 1, 0, false, 1, false, 1, intFmt, true);
  FormatElement uintEl("ColumnSizeTest", 2, 0, false, 1, false, 1, intFmt, false);

  m_ModelVSIn->columns.clear();
  m_ModelVSIn->columns.push_back(floatEl);
  m_ModelVSIn->columns.push_back(xintEl);
  m_ModelVSIn->columns.push_back(uintEl);

  m_ModelVSIn->numRows = 4;

  m_ModelVSIn->indices.stride = sizeof(uint32_t);
  m_ModelVSIn->indices.data = new byte[sizeof(uint32_t) * 4];
  m_ModelVSIn->indices.end = m_ModelVSIn->indices.data + sizeof(uint32_t) * 4;

  uint32_t *indices = (uint32_t *)m_ModelVSIn->indices.data;
  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;
  indices[3] = 1000000;

  m_ModelVSIn->buffers.clear();

  BufferData bufdata;
  bufdata.stride = sizeof(float);
  bufdata.data = new byte[sizeof(float) * 3];
  bufdata.end = m_ModelVSIn->indices.data + sizeof(float) * 3;
  m_ModelVSIn->buffers.push_back(bufdata);

  float *floats = (float *)bufdata.data;

  floats[0] = 1.0f;
  floats[1] = 1.2345e-20f;
  floats[2] = 123456.7890123456789f;

  bufdata.stride = sizeof(uint32_t);
  bufdata.data = new byte[sizeof(uint32_t) * 3];
  bufdata.end = m_ModelVSIn->indices.data + sizeof(uint32_t) * 3;
  m_ModelVSIn->buffers.push_back(bufdata);

  uint32_t *xints = (uint32_t *)bufdata.data;

  xints[0] = 0;
  xints[1] = 0x12345678;
  xints[2] = 0xffffffff;

  bufdata.stride = sizeof(uint32_t);
  bufdata.data = new byte[sizeof(uint32_t) * 3];
  bufdata.end = m_ModelVSIn->indices.data + sizeof(uint32_t) * 3;
  m_ModelVSIn->buffers.push_back(bufdata);

  uint32_t *uints = (uint32_t *)bufdata.data;

  uints[0] = 0;
  uints[1] = 0x12345678;
  uints[2] = 0xffffffff;

  m_ModelVSIn->endReset();

  // measure this data so we can use this as column widths
  ui->vsinData->resizeColumnsToContents();

  // index column
  m_IdxColWidth = ui->vsinData->columnWidth(1);

  int floatColWidth = ui->vsinData->columnWidth(2);
  int xintColWidth = ui->vsinData->columnWidth(2);
  int uintColWidth = ui->vsinData->columnWidth(3);
  m_DataColWidth = qMax(floatColWidth, qMax(xintColWidth, uintColWidth));
}

void BufferViewer::data_selected(const QItemSelection &selected, const QItemSelection &deselected)
{
  if(selected.count() > 0)
  {
    m_Config.highlightVert = selected[0].indexes()[0].row();

    INVOKE_MEMFN(RT_UpdateAndDisplay);
  }
}

void BufferViewer::on_toggleControls_toggled(bool checked)
{
  ui->cameraControlsGroup->setVisible(checked);
}
