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
  void endReset() { emit endResetModel(); }
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
    return columns.count() + 2;
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
        return "VTX";
      else if(section == 1)
        return "IDX";
      else
        return columns[section - 2].name;
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

          col -= 2;

          const FormatElement &el = columns[col];

          if(el.buffer < buffers.size())
          {
            const byte *data = buffers[el.buffer].data;
            const byte *end = buffers[el.buffer].end;

            data += buffers[el.buffer].stride * row;

            data += el.offset;

            QVariantList list = el.GetVariants(data, end);

            QString ret;

            for(QVariant &v : list)
            {
              QString comp;

              QMetaType::Type vt = (QMetaType::Type)v.type();

              if(vt == QMetaType::Double)
              {
                double d = v.toDouble();
                // pad with space on left if sign is missing, to better align
                if(d < 0.0)
                  comp = Formatter::Format(d);
                else if(d > 0.0)
                  comp = " " + Formatter::Format(d);
                else if(qIsNaN(d))
                  comp = " NaN";
                else
                  // force negative and positive 0 together
                  comp = " " + Formatter::Format(0.0);
              }
              else if(vt == QMetaType::Float)
              {
                float f = v.toFloat();
                // pad with space on left if sign is missing, to better align
                if(f < 0.0)
                  comp = Formatter::Format(f);
                else if(f > 0.0)
                  comp = " " + Formatter::Format(f);
                else if(qIsNaN(f))
                  comp = " NaN";
                else
                  // force negative and positive 0 together
                  comp = " " + Formatter::Format(0.0);
              }
              else if(vt == QMetaType::UInt || vt == QMetaType::UShort || vt == QMetaType::UChar)
              {
                comp = Formatter::Format(v.toUInt(), el.hex);
              }
              else if(vt == QMetaType::Int || vt == QMetaType::Short || vt == QMetaType::SChar)
              {
                int i = v.toInt();
                if(i > 0)
                  comp = " " + Formatter::Format(i);
                else
                  comp = Formatter::Format(i);
              }
              else
                comp = v.toString();

              ret += QString("%1 ").arg(comp, -componentWidth);
            }

            return ret;
          }
        }
      }
    }

    return QVariant();
  }

  RDTableView *view = NULL;

  int componentWidth = 0;
  uint32_t numRows = 0;
  BufferData indices;
  QList<FormatElement> columns;
  QList<BufferData> buffers;
};

BufferViewer::BufferViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::BufferViewer), m_Ctx(ctx)
{
  ui->setupUi(this);
  m_Ctx->AddLogViewer(this);

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

  m_ModelVSIn = new BufferItemModel(ui->vsinData, this);
  m_ModelVSOut = new BufferItemModel(ui->vsoutData, this);
  m_ModelGSOut = new BufferItemModel(ui->gsoutData, this);

  m_Flycam = new FlycamWrapper();
  m_Arcball = new ArcballWrapper();
  m_CurrentCamera = m_Arcball;

  m_Arcball->Reset(FloatVector(), 10.0f);
  m_Flycam->Reset(FloatVector());

  m_ConfigVSIn.type = eMeshDataStage_VSIn;
  m_ConfigVSIn.ortho = false;
  m_ConfigVSIn.showPrevInstances = false;
  m_ConfigVSIn.showAllInstances = false;
  m_ConfigVSIn.showWholePass = false;
  m_ConfigVSIn.curInstance = 0;
  m_ConfigVSIn.showBBox = false;
  m_ConfigVSIn.solidShadeMode = eShade_None;
  m_ConfigVSIn.wireframeDraw = true;
  memset(&m_ConfigVSIn.position, 0, sizeof(m_ConfigVSIn.position));
  memset(&m_ConfigVSIn.second, 0, sizeof(m_ConfigVSIn.second));

  m_ConfigVSIn.position.showAlpha = false;

  m_ConfigVSOut = m_ConfigVSIn;

  m_curConfig = &m_ConfigVSIn;
  ui->outputTabs->setCurrentIndex(0);

  QObject::connect(ui->vsinData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->vsoutData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);

  QTimer *renderTimer = new QTimer(this);
  QObject::connect(renderTimer, &QTimer::timeout, this, &BufferViewer::render_timer);
  renderTimer->setSingleShot(false);
  renderTimer->setInterval(1000);
  renderTimer->start();

  Reset();
}

BufferViewer::~BufferViewer()
{
  delete[] m_ModelVSIn->indices.data;

  for(auto vb : m_ModelVSIn->buffers)
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

    GUIInvoke::call([this]() { OnEventChanged(m_Ctx->CurEvent()); });
  });
}

void BufferViewer::OnLogfileClosed()
{
  Reset();
}

void BufferViewer::OnEventChanged(uint32_t eventID)
{
  m_ModelVSIn->beginReset();
  m_ModelVSOut->beginReset();
  m_ModelGSOut->beginReset();

  // this doesn't account for sign characters
  m_ModelVSIn->componentWidth = 0;

  // maximum width for a 32-bit integer
  m_ModelVSIn->componentWidth =
      qMax(m_ModelVSIn->componentWidth, Formatter::Format(0xffffffff).length());

  // maximum width for a few floats that are either very small or large or long,
  // to make a good estimate of how much to pad columns to
  m_ModelVSIn->componentWidth = qMax(m_ModelVSIn->componentWidth, Formatter::Format(1.0).length());
  m_ModelVSIn->componentWidth =
      qMax(m_ModelVSIn->componentWidth, Formatter::Format(1.2345e-200).length());
  m_ModelVSIn->componentWidth =
      qMax(m_ModelVSIn->componentWidth, Formatter::Format(123456.7890123456789).length());

  m_ModelVSOut->componentWidth = m_ModelVSIn->componentWidth;
  m_ModelGSOut->componentWidth = m_ModelVSIn->componentWidth;

  QVector<VertexInputAttribute> vinputs = m_Ctx->CurPipelineState.GetVertexInputs();

  m_ModelVSIn->columns.clear();
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

  delete[] m_ModelVSIn->indices.data;

  m_ModelVSIn->indices = BufferData();

  for(auto vb : m_ModelVSIn->buffers)
    delete[] vb.data;

  m_ModelVSIn->buffers.clear();

  const FetchDrawcall *draw = m_Ctx->CurDrawcall();

  if(draw == NULL)
    m_ModelVSIn->numRows = 0;
  else
    m_ModelVSIn->numRows = draw->numIndices;

  QVector<BoundVBuffer> vbs = m_Ctx->CurPipelineState.GetVBuffers();

  ResourceId ib;
  uint64_t ioffset = 0;
  m_Ctx->CurPipelineState.GetIBuffer(ib, ioffset);

  m_ConfigVSIn.fov = 90.0f;
  m_ConfigVSIn.aspect =
      m_Ctx->CurPipelineState.GetViewport(0).width / m_Ctx->CurPipelineState.GetViewport(0).height;
  m_ConfigVSIn.highlightVert = 0;

  if(draw == NULL)
  {
    m_ConfigVSIn.position.numVerts = 0;
    m_ConfigVSIn.position.topo = eTopology_TriangleList;
    m_ConfigVSIn.position.idxbuf = ResourceId();
    m_ConfigVSIn.position.idxoffs = 0;
    m_ConfigVSIn.position.idxByteWidth = 4;
    m_ConfigVSIn.position.baseVertex = 0;
  }
  else
  {
    m_ConfigVSIn.position.numVerts = draw->numIndices;
    m_ConfigVSIn.position.topo = draw->topology;
    m_ConfigVSIn.position.idxbuf = ib;
    m_ConfigVSIn.position.idxoffs = ioffset;
    m_ConfigVSIn.position.idxByteWidth = draw->indexByteWidth;
    m_ConfigVSIn.position.baseVertex = draw->baseVertex;
  }

  if(!vinputs.empty())
  {
    m_ConfigVSIn.position.buf = vbs[vinputs[0].VertexBuffer].Buffer;
    m_ConfigVSIn.position.offset = vbs[vinputs[0].VertexBuffer].ByteOffset;
    m_ConfigVSIn.position.stride = vbs[vinputs[0].VertexBuffer].ByteStride;

    m_ConfigVSIn.position.compCount = vinputs[0].Format.compCount;
    m_ConfigVSIn.position.compByteWidth = vinputs[0].Format.compByteWidth;
    m_ConfigVSIn.position.compType = vinputs[0].Format.compType;
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

  delete[] m_ModelVSOut->indices.data;

  for(auto vb : m_ModelVSOut->buffers)
    delete[] vb.data;

  m_ModelVSOut->buffers.clear();

  m_ConfigVSOut.fov = m_ConfigVSIn.fov;
  m_ConfigVSOut.aspect = m_ConfigVSIn.aspect;
  m_ConfigVSOut.highlightVert = m_ConfigVSIn.highlightVert;

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

    r->GetPostVSData(0, eMeshDataStage_VSOut, &m_ConfigVSOut.position);

    m_ModelVSOut->numRows = m_ConfigVSOut.position.numVerts;

    if(m_ConfigVSOut.position.idxbuf != ResourceId())
      r->GetBufferData(m_ConfigVSOut.position.idxbuf,
                       ioffset + draw->indexOffset * draw->indexByteWidth,
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

    if(m_ConfigVSOut.position.buf != ResourceId())
    {
      BufferData postvs = {};
      rdctype::array<byte> data;
      r->GetBufferData(m_ConfigVSOut.position.buf, 0, 0, &data);

      postvs.data = new byte[data.count];
      memcpy(postvs.data, data.elems, data.count);
      postvs.end = postvs.data + data.count;
      postvs.stride = m_ConfigVSOut.position.stride;
      m_ModelVSOut->buffers.push_back(postvs);
    }

    RT_UpdateAndDisplay(r);

    GUIInvoke::call([this] {
      m_ModelVSIn->endReset();
      m_ModelVSOut->endReset();
      // ui->vsinData->resizeColumnsToContents();
      // ui->vsoutData->resizeColumnsToContents();
    });
  });
}

void BufferViewer::on_outputTabs_currentChanged(int index)
{
  ui->renderContainer->parentWidget()->layout()->removeWidget(ui->renderContainer);
  ui->outputTabs->widget(index)->layout()->addWidget(ui->renderContainer);

  if(index == 0)
    m_curConfig = &m_ConfigVSIn;
  else if(index == 1)
    m_curConfig = &m_ConfigVSOut;

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
    m_curConfig->cam = m_CurrentCamera->camera();
    m_Output->SetMeshDisplay(*m_curConfig);
    m_Output->Display();
  }
}

void BufferViewer::Reset()
{
  m_Output = NULL;

  if(m_ModelVSIn)
  {
    for(auto vb : m_ModelVSIn->buffers)
      delete[] vb.data;

    m_ModelVSIn->buffers.clear();

    m_ModelVSIn->columns.clear();

    m_ModelVSIn->numRows = 0;
  }

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

void BufferViewer::data_selected(const QItemSelection &selected, const QItemSelection &deselected)
{
  if(QObject::sender() == (QObject *)ui->vsinData->selectionModel() && selected.count() > 0)
  {
    m_ConfigVSIn.highlightVert = selected[0].indexes()[0].row();

    INVOKE_MEMFN(RT_UpdateAndDisplay);
  }
  else if(QObject::sender() == (QObject *)ui->vsoutData->selectionModel() && selected.count() > 0)
  {
    m_ConfigVSOut.highlightVert = selected[0].indexes()[0].row();

    INVOKE_MEMFN(RT_UpdateAndDisplay);
  }
}

void BufferViewer::on_toggleControls_toggled(bool checked)
{
  ui->cameraControlsGroup->setVisible(checked);
}
