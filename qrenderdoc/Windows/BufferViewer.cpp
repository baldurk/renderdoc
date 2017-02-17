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
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include "Code/Resources.h"
#include "Windows/ShaderViewer.h"
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
    m_ColumnCount = columnLookup.count() + reservedColumnCount();
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
    return m_ColumnCount;
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(section < m_ColumnCount && orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      if(section == 0 && meshView)
      {
        return "VTX";
      }
      else if(section == 1 && meshView)
      {
        return "IDX";
      }
      else
      {
        const FormatElement &el = elementForColumn(section);

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
        if(index.column() < 2 && meshView)
          opt.text = "999999";
        else
          opt.text = data(index).toString();
        opt.styleObject = NULL;

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        return style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), opt.widget);
      }

      uint32_t row = index.row();
      int col = index.column();

      if((role == Qt::BackgroundRole || role == Qt::ForegroundRole) && col >= reservedColumnCount())
      {
        int elIdx = columnLookup[col - reservedColumnCount()];
        int compIdx = componentForIndex(col);
        if(elIdx == positionEl)
        {
          if(role == Qt::ForegroundRole)
            return QBrush(Qt::black);

          if(compIdx != 3 || !meshInput)
          {
            // C# SkyBlue
            return QBrush(QColor::fromRgb(135, 206, 235));
          }
          else
          {
            // C# LightCyan
            return QBrush(QColor::fromRgb(224, 255, 255));
          }
        }
        else if(secondaryEnabled && elIdx == secondaryEl)
        {
          if(role == Qt::ForegroundRole)
            return QBrush(Qt::black);

          if((secondaryElAlpha && compIdx == 3) || (!secondaryElAlpha && compIdx != 3))
          {
            // C# LightGreen
            return QBrush(QColor::fromRgb(144, 238, 144));
          }
          else
          {
            return QBrush(QColor::fromRgb(200, 238, 200));
          }
        }
      }

      if(role == Qt::DisplayRole)
      {
        if(col >= 0 && col < m_ColumnCount && row < numRows)
        {
          if(col == 0 && meshView)
            return row;

          uint32_t idx = row;

          if(indices.data)
          {
            byte *idxData = indices.data + row * sizeof(uint32_t);
            if(idxData + 1 > indices.end)
              return QVariant();

            idx = *(uint32_t *)idxData;
          }

          if(col == 1 && meshView)
            return idx;

          const FormatElement &el = elementForColumn(col);

          uint32_t instIdx = 0;
          if(el.instancerate > 0)
            instIdx = curInstance / el.instancerate;

          if(el.buffer < buffers.size())
          {
            const byte *data = buffers[el.buffer].data;
            const byte *end = buffers[el.buffer].end;

            if(!el.perinstance)
              data += buffers[el.buffer].stride * idx;
            else
              data += buffers[el.buffer].stride * instIdx;

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

  uint32_t curInstance = 0;
  uint32_t numRows = 0;
  bool meshView = true;
  bool meshInput = false;
  BufferData indices;
  QList<FormatElement> columns;
  QList<BufferData> buffers;

  void setPosColumn(int pos)
  {
    QVector<int> roles = {Qt::BackgroundRole, Qt::ForegroundRole};

    if(positionEl != pos)
    {
      if(positionEl >= 0)
        emit dataChanged(index(0, firstColumnForElement(positionEl)),
                         index(rowCount() - 1, lastColumnForElement(positionEl)), roles);

      if(pos >= 0)
        emit dataChanged(index(0, firstColumnForElement(pos)),
                         index(rowCount() - 1, lastColumnForElement(pos)), roles);
    }

    positionEl = pos;
  }

  int posColumn() { return positionEl; }
  QString posName()
  {
    if(positionEl >= 0 && positionEl < columns.count())
      return columns[positionEl].name;

    return "";
  }

  void setSecondaryColumn(int sec, bool secEnabled, bool secAlpha)
  {
    QVector<int> roles = {Qt::BackgroundRole, Qt::ForegroundRole};

    if(secondaryEl != sec || secondaryElAlpha != secAlpha || secondaryEnabled != secEnabled)
    {
      if(secondaryEl >= 0 && secondaryEl != sec)
        emit dataChanged(index(0, firstColumnForElement(secondaryEl)),
                         index(rowCount() - 1, lastColumnForElement(secondaryEl)), roles);

      if(sec >= 0)
        emit dataChanged(index(0, firstColumnForElement(sec)),
                         index(rowCount() - 1, lastColumnForElement(sec)), roles);
    }

    secondaryEl = sec;
    secondaryElAlpha = secAlpha;
    secondaryEnabled = secEnabled;
  }

  int secondaryColumn() { return secondaryEl; }
  bool secondaryAlpha() { return secondaryElAlpha; }
  QString secondaryName()
  {
    if(secondaryEl >= 0 && secondaryEl < columns.count())
      return columns[secondaryEl].name;

    return "";
  }
  int elementIndexForColumn(int col) const
  {
    if(col < reservedColumnCount())
      return -1;

    return columnLookup[col - reservedColumnCount()];
  }

  const FormatElement &elementForColumn(int col) const
  {
    return columns[columnLookup[col - reservedColumnCount()]];
  }

private:
  // maps from column number (0-based from data, so excluding VTX/IDX columns)
  // to the column element in the columns list, and lists its component.
  //
  // So a float4, float3, int set of columns would be:
  // { 0, 0, 0, 0, 1, 1, 1, 2 };
  // { 0, 1, 2, 3, 0, 1, 2, 0 };
  QVector<int> columnLookup;
  QVector<int> componentLookup;
  int m_ColumnCount = 0;

  int positionEl = -1;
  int secondaryEl = -1;
  bool secondaryElAlpha = false;
  bool secondaryEnabled = false;

  int reservedColumnCount() const { return (meshView ? 2 : 0); }
  int componentForIndex(int col) const { return componentLookup[col - reservedColumnCount()]; }
  int firstColumnForElement(int el) const
  {
    for(int i = 0; i < columnLookup.count(); i++)
    {
      if(columnLookup[i] == el)
        return reservedColumnCount() + i;
    }

    return 0;
  }

  int lastColumnForElement(int el) const
  {
    for(int i = columnLookup.count() - 1; i >= 0; i--)
    {
      if(columnLookup[i] == el)
        return reservedColumnCount() + i;
    }

    return columnCount() - 1;
  }

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

BufferViewer::BufferViewer(CaptureContext &ctx, bool meshview, QWidget *parent)
    : QFrame(parent), ui(new Ui::BufferViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_ModelVSIn = new BufferItemModel(ui->vsinData, this);
  m_ModelVSOut = new BufferItemModel(ui->vsoutData, this);
  m_ModelGSOut = new BufferItemModel(ui->gsoutData, this);

  m_Flycam = new FlycamWrapper();
  m_Arcball = new ArcballWrapper();
  m_CurrentCamera = m_Arcball;

  m_Output = NULL;

  memset(&m_Config, 0, sizeof(m_Config));
  m_Config.type = eMeshDataStage_VSIn;
  m_Config.wireframeDraw = true;

  ui->outputTabs->setCurrentIndex(0);
  m_CurStage = eMeshDataStage_VSIn;

  ui->vsinData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->vsoutData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->gsoutData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  m_ModelVSIn->meshView = m_ModelVSOut->meshView = m_ModelGSOut->meshView = m_MeshView = meshview;

  m_ModelVSIn->meshInput = true;

  if(meshview)
    SetupMeshView();
  else
    SetupRawView();

  QMenu *exportMenu = new QMenu(this);

  QAction *csv = new QAction(tr("Export to &CSV"), this);
  csv->setIcon(Icons::save());
  exportMenu->addAction(csv);
  QAction *bytes = new QAction(tr("Export to &Bytes"), this);
  bytes->setIcon(Icons::save());
  exportMenu->addAction(bytes);

  QAction *debug = new QAction(tr("&Debug this Vertex"), this);
  debug->setIcon(Icons::wrench());

  ui->exportDrop->setMenu(exportMenu);

  QObject::connect(csv, &QAction::triggered, [this] { exportData(BufferExport(BufferExport::CSV)); });
  QObject::connect(bytes, &QAction::triggered,
                   [this] { exportData(BufferExport(BufferExport::RawBytes)); });
  QObject::connect(debug, &QAction::triggered, this, &BufferViewer::debugVertex);

  QObject::connect(ui->exportDrop, &QToolButton::clicked,
                   [this] { exportData(BufferExport(BufferExport::CSV)); });

  ui->vsinData->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->vsoutData->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->gsoutData->setContextMenuPolicy(Qt::CustomContextMenu);

  QMenu *vsinMenu = new QMenu(this);

  QObject::connect(ui->vsinData, &RDTableView::customContextMenuRequested,
                   [this, vsinMenu, debug, csv, bytes](const QPoint &pos) {
                     m_CurView = ui->vsinData;

                     vsinMenu->clear();

                     if(m_MeshView)
                     {
                       vsinMenu->addAction(debug);
                       vsinMenu->addSeparator();
                     }

                     vsinMenu->addAction(csv);
                     vsinMenu->addAction(bytes);

                     vsinMenu->popup(ui->vsinData->viewport()->mapToGlobal(pos));
                   });

  QMenu *vsoutMenu = new QMenu(this);

  QObject::connect(ui->vsoutData, &RDTableView::customContextMenuRequested,
                   [this, vsoutMenu, debug, csv, bytes](const QPoint &pos) {
                     m_CurView = ui->vsoutData;

                     vsoutMenu->clear();

                     if(m_MeshView)
                     {
                       vsoutMenu->addAction(debug);
                       vsoutMenu->addSeparator();
                     }

                     vsoutMenu->addAction(csv);
                     vsoutMenu->addAction(bytes);

                     vsoutMenu->popup(ui->vsoutData->viewport()->mapToGlobal(pos));
                   });

  QMenu *gsoutMenu = new QMenu(this);

  QObject::connect(ui->gsoutData, &RDTableView::customContextMenuRequested,
                   [this, gsoutMenu, csv, bytes](const QPoint &pos) {
                     m_CurView = ui->gsoutData;

                     gsoutMenu->clear();

                     gsoutMenu->addAction(csv);
                     gsoutMenu->addAction(bytes);

                     gsoutMenu->popup(ui->gsoutData->viewport()->mapToGlobal(pos));
                   });

  ui->dockarea->setAllowFloatingWindow(false);
  ui->dockarea->setRubberBandLineWidth(50);

  ui->controlType->addItems({tr("Arcball"), tr("WASD")});
  ui->controlType->adjustSize();

  ui->drawRange->addItems({tr("Only this draw"), tr("Show previous instances"),
                           tr("Show all instances"), tr("Show whole pass")});
  ui->drawRange->adjustSize();
  ui->drawRange->setCurrentIndex(0);

  ui->solidShading->addItems({tr("None"), tr("Solid Colour"), tr("Flat Shaded"), tr("Secondary")});
  ui->solidShading->adjustSize();
  ui->solidShading->setCurrentIndex(0);

  // wireframe only available on solid shaded options
  ui->wireframeRender->setEnabled(false);

  ui->fovGuess->setValue(90.0);

  on_controlType_currentIndexChanged(0);

  QObject::connect(ui->vsinData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->vsoutData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->gsoutData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);

  QObject::connect(ui->vsinData, &RDTableView::clicked, [this]() { m_CurView = ui->vsinData; });
  QObject::connect(ui->vsoutData, &RDTableView::clicked, [this]() { m_CurView = ui->vsoutData; });
  QObject::connect(ui->gsoutData, &RDTableView::clicked, [this]() { m_CurView = ui->gsoutData; });

  QObject::connect(ui->vsinData->verticalScrollBar(), &QScrollBar::valueChanged, this,
                   &BufferViewer::data_scrolled);
  QObject::connect(ui->vsoutData->verticalScrollBar(), &QScrollBar::valueChanged, this,
                   &BufferViewer::data_scrolled);
  QObject::connect(ui->gsoutData->verticalScrollBar(), &QScrollBar::valueChanged, this,
                   &BufferViewer::data_scrolled);

  QObject::connect(ui->fovGuess, OverloadedSlot<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &BufferViewer::camGuess_changed);
  QObject::connect(ui->aspectGuess, OverloadedSlot<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &BufferViewer::camGuess_changed);
  QObject::connect(ui->nearGuess, OverloadedSlot<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &BufferViewer::camGuess_changed);
  QObject::connect(ui->farGuess, OverloadedSlot<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &BufferViewer::camGuess_changed);
  QObject::connect(ui->matrixType, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                   [this](int) { camGuess_changed(0.0); });

  Reset();

  m_Ctx.AddLogViewer(this);
}

void BufferViewer::SetupRawView()
{
  ui->formatSpecifier->setVisible(true);
  ui->outputTabs->setVisible(false);
  ui->vsoutData->setVisible(false);
  ui->gsoutData->setVisible(false);

  // hide buttons we don't want in the toolbar
  ui->syncViews->setVisible(false);
  ui->offsetLine->setVisible(false);
  ui->instanceLabel->setVisible(false);
  ui->instance->setVisible(false);
  ui->rowOffsetLabel->setVisible(false);
  ui->rowOffset->setVisible(false);

  ui->vsinData->setWindowTitle(tr("Buffer Contents"));
  ui->dockarea->addToolWindow(ui->vsinData, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(ui->vsinData, ToolWindowManager::HideCloseButton);

  ui->formatSpecifier->setWindowTitle(tr("Buffer Format"));
  ui->dockarea->addToolWindow(ui->formatSpecifier, ToolWindowManager::AreaReference(
                                                       ToolWindowManager::BottomOf,
                                                       ui->dockarea->areaOf(ui->vsinData), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->formatSpecifier, ToolWindowManager::HideCloseButton);

  QObject::connect(ui->formatSpecifier, &BufferFormatSpecifier::processFormat, this,
                   &BufferViewer::processFormat);

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(0, 0, 0, 0);

  vertical->addWidget(ui->meshToolbar);
  vertical->addWidget(ui->dockarea);
}

void BufferViewer::SetupMeshView()
{
  setWindowTitle(tr("Mesh Output"));

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

  QMenu *headerMenu = new QMenu(this);

  QAction *resetColSel = new QAction(tr("Reset Selected Columns"), this);
  QAction *selPos = new QAction(tr("Select as Position"), this);
  QAction *selSec = new QAction(tr("Select as Secondary"), this);
  QAction *selSecAlpha = new QAction(tr("Select Alpha as Secondary"), this);

  headerMenu->addAction(resetColSel);
  headerMenu->addSeparator();
  headerMenu->addAction(selPos);
  headerMenu->addAction(selSec);
  headerMenu->addAction(selSecAlpha);

  QObject::connect(resetColSel, &QAction::triggered, [this]() {
    guessPositionColumn((BufferItemModel *)m_CurView->model());
    guessSecondaryColumn((BufferItemModel *)m_CurView->model());
    updatePreviewColumns();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(selPos, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setPosColumn(m_ContextColumn);
    updatePreviewColumns();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(selSec, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setSecondaryColumn(m_ContextColumn, m_Config.solidShadeMode == eShade_Secondary, false);
    updatePreviewColumns();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(selSecAlpha, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setSecondaryColumn(m_ContextColumn, m_Config.solidShadeMode == eShade_Secondary, true);
    updatePreviewColumns();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });

  ui->vsinData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->vsoutData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->gsoutData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

  QObject::connect(
      ui->vsinData->horizontalHeader(), &QHeaderView::customContextMenuRequested,
      [this, headerMenu, selSecAlpha](const QPoint &pos) {
        int col = ui->vsinData->horizontalHeader()->logicalIndexAt(pos);

        if(col < 2)
          return;

        m_CurView = ui->vsinData;
        m_ContextColumn = m_ModelVSIn->elementIndexForColumn(col);

        selSecAlpha->setEnabled(m_ModelVSIn->elementForColumn(col).format.compCount == 4);

        headerMenu->popup(ui->vsinData->horizontalHeader()->mapToGlobal(pos));
      });
  QObject::connect(
      ui->vsoutData->horizontalHeader(), &QHeaderView::customContextMenuRequested,
      [this, headerMenu, selSecAlpha](const QPoint &pos) {
        int col = ui->vsoutData->horizontalHeader()->logicalIndexAt(pos);

        if(col < 2)
          return;

        m_CurView = ui->vsoutData;
        m_ContextColumn = m_ModelVSOut->elementIndexForColumn(col);

        selSecAlpha->setEnabled(m_ModelVSOut->elementForColumn(col).format.compCount == 4);

        headerMenu->popup(ui->vsoutData->horizontalHeader()->mapToGlobal(pos));
      });
  QObject::connect(
      ui->gsoutData->horizontalHeader(), &QHeaderView::customContextMenuRequested,
      [this, headerMenu, selSecAlpha](const QPoint &pos) {
        int col = ui->gsoutData->horizontalHeader()->logicalIndexAt(pos);

        if(col < 2)
          return;

        m_CurView = ui->gsoutData;
        m_ContextColumn = m_ModelGSOut->elementIndexForColumn(col);

        selSecAlpha->setEnabled(m_ModelGSOut->elementForColumn(col).format.compCount == 4);

        headerMenu->popup(ui->gsoutData->horizontalHeader()->mapToGlobal(pos));
      });

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(0, 0, 0, 0);

  vertical->addWidget(ui->meshToolbar);
  vertical->addWidget(ui->dockarea);

  QTimer *renderTimer = new QTimer(this);
  QObject::connect(renderTimer, &QTimer::timeout, this, &BufferViewer::render_timer);
  renderTimer->setSingleShot(false);
  renderTimer->setInterval(10);
  renderTimer->start();
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

  if(m_MeshView)
    m_Ctx.windowClosed(this);

  m_Ctx.RemoveLogViewer(this);
  delete ui;
}

void BufferViewer::OnLogfileLoaded()
{
  Reset();

  if(!m_MeshView)
    return;

  WId renderID = ui->render->winId();

  m_Ctx.Renderer().BlockInvoke([renderID, this](IReplayRenderer *r) {
    m_Output = r->CreateOutput(m_Ctx.m_CurWinSystem, m_Ctx.FillWindowingData(renderID),
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
  int vsinHoriz = ui->vsinData->horizontalScrollBar()->value();
  int vsoutHoriz = ui->vsoutData->horizontalScrollBar()->value();
  int gsoutHoriz = ui->gsoutData->horizontalScrollBar()->value();

  QString highlightNames[6] = {
      m_ModelVSIn->posName(),        m_ModelVSIn->secondaryName(), m_ModelVSOut->posName(),
      m_ModelVSOut->secondaryName(), m_ModelGSOut->posName(),      m_ModelGSOut->secondaryName(),
  };

  if(m_MeshView)
  {
    ClearModels();

    CalcColumnWidth();

    ClearModels();
  }

  EnableCameraGuessControls();

  m_ModelVSIn->curInstance = m_Config.curInstance;
  m_ModelVSOut->curInstance = m_Config.curInstance;
  m_ModelGSOut->curInstance = m_Config.curInstance;

  m_ModelVSIn->beginReset();
  m_ModelVSOut->beginReset();
  m_ModelGSOut->beginReset();

  const FetchDrawcall *draw = m_Ctx.CurDrawcall();

  ui->instance->setEnabled(draw && draw->numInstances > 1);
  if(!ui->instance->isEnabled())
    ui->instance->setValue(0);

  if(draw)
    ui->instance->setMaximum(qMax(0, int(draw->numInstances) - 1));

  if(m_MeshView)
  {
    configureMeshColumns();

    if(m_ModelVSIn->posColumn() == -1 || highlightNames[0] != m_ModelVSIn->posName())
      guessPositionColumn(m_ModelVSIn);
    if(m_ModelVSIn->secondaryColumn() == -1 || highlightNames[1] != m_ModelVSIn->secondaryName())
      guessSecondaryColumn(m_ModelVSIn);
    if(m_ModelVSOut->posColumn() == -1 || highlightNames[2] != m_ModelVSOut->posName())
      guessSecondaryColumn(m_ModelVSOut);
    if(m_ModelVSOut->secondaryColumn() == -1 || highlightNames[3] != m_ModelVSOut->secondaryName())
      guessPositionColumn(m_ModelVSOut);
    if(m_ModelGSOut->posColumn() == -1 || highlightNames[4] != m_ModelGSOut->posName())
      guessPositionColumn(m_ModelGSOut);
    if(m_ModelGSOut->secondaryColumn() == -1 || highlightNames[5] != m_ModelGSOut->secondaryName())
      guessSecondaryColumn(m_ModelGSOut);
  }

  m_Ctx.Renderer().AsyncInvoke([this, vsinHoriz, vsoutHoriz, gsoutHoriz](IReplayRenderer *r) {

    if(m_MeshView)
    {
      RT_FetchMeshData(r);
    }
    else
    {
      BufferData buf = {};
      rdctype::array<byte> data;
      if(m_IsBuffer)
      {
        uint64_t len = m_ByteSize;
        if(len == UINT64_MAX)
          len = 0;

        r->GetBufferData(m_BufferID, m_ByteOffset, len, &data);
      }
      else
      {
        r->GetTextureData(m_BufferID, m_TexArrayIdx, m_TexMip, &data);
      }

      buf.data = new byte[data.count];
      memcpy(buf.data, data.elems, data.count);
      buf.end = buf.data + data.count;

      // calculate tight stride
      buf.stride = 0;
      for(const FormatElement &el : m_ModelVSIn->columns)
        buf.stride += el.byteSize();

      buf.stride = qMax((size_t)1, buf.stride);

      m_ModelVSIn->numRows = uint32_t((data.count + buf.stride - 1) / buf.stride);

      m_ModelVSIn->buffers.push_back(buf);
    }

    updatePreviewColumns();

    RT_UpdateAndDisplay(r);

    GUIInvoke::call([this, vsinHoriz, vsoutHoriz, gsoutHoriz] {
      m_ModelVSIn->endReset();
      m_ModelVSOut->endReset();
      m_ModelGSOut->endReset();

      ApplyColumnWidths(m_ModelVSIn->columnCount(), ui->vsinData);
      ApplyColumnWidths(m_ModelVSOut->columnCount(), ui->vsoutData);
      ApplyColumnWidths(m_ModelGSOut->columnCount(), ui->gsoutData);

      int numRows = qMax(qMax(m_ModelVSIn->numRows, m_ModelVSOut->numRows), m_ModelGSOut->numRows);

      ui->rowOffset->setMaximum(qMax(0, numRows - 1));

      ScrollToRow(m_ModelVSIn, ui->rowOffset->value());
      ScrollToRow(m_ModelVSOut, ui->rowOffset->value());
      ScrollToRow(m_ModelGSOut, ui->rowOffset->value());

      ui->vsinData->horizontalScrollBar()->setValue(vsinHoriz);
      ui->vsoutData->horizontalScrollBar()->setValue(vsoutHoriz);
      ui->gsoutData->horizontalScrollBar()->setValue(gsoutHoriz);
    });
  });
}

void BufferViewer::RT_FetchMeshData(IReplayRenderer *r)
{
  const FetchDrawcall *draw = m_Ctx.CurDrawcall();

  ResourceId ib;
  uint64_t ioffset = 0;
  m_Ctx.CurPipelineState.GetIBuffer(ib, ioffset);

  QVector<BoundVBuffer> vbs = m_Ctx.CurPipelineState.GetVBuffers();

  rdctype::array<byte> idata;
  if(ib != ResourceId() && draw && (draw->flags & eDraw_UseIBuffer))
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

  uint32_t maxIndex = 0;
  if(draw)
    maxIndex = qMax(1U, draw->numIndices) - 1;

  if(draw && idata.count > 0)
  {
    maxIndex = 0;
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
      rdctype::array<byte> bufdata;
      r->GetBufferData(vb.Buffer, vb.ByteOffset + offset * vb.ByteStride,
                       (maxIdx + 1) * vb.ByteStride, &bufdata);

      buf.data = new byte[bufdata.count];
      memcpy(buf.data, bufdata.elems, bufdata.count);
      buf.end = buf.data + bufdata.count;
      buf.stride = vb.ByteStride;
    }
    m_ModelVSIn->buffers.push_back(buf);
  }

  r->GetPostVSData(m_Config.curInstance, eMeshDataStage_VSOut, &m_PostVS);

  m_ModelVSOut->numRows = m_PostVS.numVerts;

  if(draw && m_PostVS.idxbuf != ResourceId() && (draw->flags & eDraw_UseIBuffer))
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

  if(draw && idata.count > 0)
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
    rdctype::array<byte> bufdata;
    r->GetBufferData(m_PostVS.buf, m_PostVS.offset, 0, &bufdata);

    postvs.data = new byte[bufdata.count];
    memcpy(postvs.data, bufdata.elems, bufdata.count);
    postvs.end = postvs.data + bufdata.count;
    postvs.stride = m_PostVS.stride;
    m_ModelVSOut->buffers.push_back(postvs);
  }

  r->GetPostVSData(m_Config.curInstance, eMeshDataStage_GSOut, &m_PostGS);

  m_ModelGSOut->numRows = m_PostGS.numVerts;

  indices = NULL;
  m_ModelGSOut->indices = BufferData();

  if(m_PostGS.buf != ResourceId())
  {
    BufferData postgs = {};
    rdctype::array<byte> bufdata;
    r->GetBufferData(m_PostGS.buf, m_PostGS.offset, 0, &bufdata);

    postgs.data = new byte[bufdata.count];
    memcpy(postgs.data, bufdata.elems, bufdata.count);
    postgs.end = postgs.data + bufdata.count;
    postgs.stride = m_PostGS.stride;
    m_ModelGSOut->buffers.push_back(postgs);
  }
}

void BufferViewer::guessPositionColumn(BufferItemModel *model)
{
  int posEl = -1;

  if(!model->columns.empty())
  {
    // prioritise system value over general "POSITION" string matching
    for(int i = 0; i < model->columns.count(); i++)
    {
      const FormatElement &el = model->columns[i];

      if(el.systemValue == eAttr_Position)
      {
        posEl = i;
        break;
      }
    }

    // look for an exact match
    for(int i = 0; posEl == -1 && i < model->columns.count(); i++)
    {
      const FormatElement &el = model->columns[i];

      if(el.name.compare("POSITION", Qt::CaseInsensitive) == 0 ||
         el.name.compare("POSITION0", Qt::CaseInsensitive) == 0 ||
         el.name.compare("POS", Qt::CaseInsensitive) == 0 ||
         el.name.compare("POS0", Qt::CaseInsensitive) == 0)
      {
        posEl = i;
        break;
      }
    }

    // try anything containing position
    for(int i = 0; posEl == -1 && i < model->columns.count(); i++)
    {
      const FormatElement &el = model->columns[i];

      if(el.name.contains("POSITION", Qt::CaseInsensitive))
      {
        posEl = i;
        break;
      }
    }

    // OK last resort, just look for 'pos'
    for(int i = 0; posEl == -1 && i < model->columns.count(); i++)
    {
      const FormatElement &el = model->columns[i];

      if(el.name.contains("POS", Qt::CaseInsensitive))
      {
        posEl = i;
        break;
      }
    }

    // if we still have absolutely nothing, just use the first available element
    if(posEl == -1)
    {
      posEl = 0;
    }
  }

  model->setPosColumn(posEl);
}

void BufferViewer::guessSecondaryColumn(BufferItemModel *model)
{
  int secondEl = -1;

  if(!model->columns.empty())
  {
    // prioritise TEXCOORD over general COLOR
    for(int i = 0; i < model->columns.count(); i++)
    {
      const FormatElement &el = model->columns[i];

      if(el.name.compare("TEXCOORD", Qt::CaseInsensitive) == 0 ||
         el.name.compare("TEXCOORD0", Qt::CaseInsensitive) == 0 ||
         el.name.compare("TEX", Qt::CaseInsensitive) == 0 ||
         el.name.compare("TEX0", Qt::CaseInsensitive) == 0 ||
         el.name.compare("UV", Qt::CaseInsensitive) == 0 ||
         el.name.compare("UV0", Qt::CaseInsensitive) == 0)
      {
        secondEl = i;
        break;
      }
    }

    for(int i = 0; secondEl == -1 && i < model->columns.count(); i++)
    {
      const FormatElement &el = model->columns[i];

      if(el.name.compare("COLOR", Qt::CaseInsensitive) == 0 ||
         el.name.compare("COLOR0", Qt::CaseInsensitive) == 0 ||
         el.name.compare("COL", Qt::CaseInsensitive) == 0 ||
         el.name.compare("COL0", Qt::CaseInsensitive) == 0)
      {
        secondEl = i;
        break;
      }
    }
  }

  model->setSecondaryColumn(secondEl, m_Config.solidShadeMode == eShade_Secondary, false);
}

void BufferViewer::updatePreviewColumns()
{
  QVector<BoundVBuffer> vbs = m_Ctx.CurPipelineState.GetVBuffers();
  const FetchDrawcall *draw = m_Ctx.CurDrawcall();

  if(draw)
  {
    m_VSInPosition = MeshFormat();
    m_VSInSecondary = MeshFormat();

    if(!m_ModelVSIn->columns.empty())
    {
      int elIdx = m_ModelVSIn->posColumn();
      if(elIdx < 0 || elIdx >= m_ModelVSIn->columns.count())
        elIdx = 0;

      m_VSInPosition.numVerts = draw->numIndices;
      m_VSInPosition.topo = draw->topology;
      m_VSInPosition.idxByteWidth = draw->indexByteWidth;
      m_VSInPosition.baseVertex = draw->baseVertex;
      m_Ctx.CurPipelineState.GetIBuffer(m_VSInPosition.idxbuf, m_VSInPosition.idxoffs);

      {
        const FormatElement &el = m_ModelVSIn->columns[elIdx];

        m_VSInPosition.buf = vbs[el.buffer].Buffer;
        m_VSInPosition.stride = vbs[el.buffer].ByteStride;
        m_VSInPosition.offset =
            vbs[el.buffer].ByteOffset + el.offset + draw->vertexOffset * m_VSInPosition.stride;

        m_VSInPosition.compCount = el.format.compCount;
        m_VSInPosition.compByteWidth = el.format.compByteWidth;
        m_VSInPosition.compType = el.format.compType;
        m_VSInPosition.bgraOrder = el.format.bgraOrder;
        m_VSInPosition.specialFormat = el.format.special ? el.format.specialFormat : eSpecial_Unknown;
      }

      elIdx = m_ModelVSIn->secondaryColumn();

      if(elIdx >= 0 && elIdx < m_ModelVSIn->columns.count())
      {
        const FormatElement &el = m_ModelVSIn->columns[elIdx];

        m_VSInSecondary.buf = vbs[el.buffer].Buffer;
        m_VSInSecondary.stride = vbs[el.buffer].ByteStride;
        m_VSInSecondary.offset =
            vbs[el.buffer].ByteOffset + el.offset + draw->vertexOffset * m_VSInPosition.stride;

        m_VSInSecondary.compCount = el.format.compCount;
        m_VSInSecondary.compByteWidth = el.format.compByteWidth;
        m_VSInSecondary.compType = el.format.compType;
        m_VSInSecondary.bgraOrder = el.format.bgraOrder;
        m_VSInSecondary.specialFormat =
            el.format.special ? el.format.specialFormat : eSpecial_Unknown;
        m_VSInSecondary.showAlpha = m_ModelVSIn->secondaryAlpha();
      }
    }

    m_PostVSPosition = MeshFormat();
    m_PostVSSecondary = MeshFormat();

    if(!m_ModelVSOut->columns.empty())
    {
      int elIdx = m_ModelVSOut->posColumn();
      if(elIdx < 0 || elIdx >= m_ModelVSOut->columns.count())
        elIdx = 0;

      m_PostVSPosition = m_PostVS;
      m_PostVSPosition.offset += m_ModelVSOut->columns[elIdx].offset;

      elIdx = m_ModelVSOut->secondaryColumn();

      if(elIdx >= 0 && elIdx < m_ModelVSOut->columns.count())
      {
        m_PostVSSecondary = m_PostVS;
        m_PostVSSecondary.offset += m_ModelVSOut->columns[elIdx].offset;
        m_PostVSSecondary.showAlpha = m_ModelVSOut->secondaryAlpha();
      }
    }

    m_PostGSPosition = MeshFormat();
    m_PostGSSecondary = MeshFormat();

    if(!m_ModelGSOut->columns.empty())
    {
      int elIdx = m_ModelGSOut->posColumn();
      if(elIdx < 0 || elIdx >= m_ModelGSOut->columns.count())
        elIdx = 0;

      m_PostGSPosition = m_PostGS;
      m_PostGSPosition.offset += m_ModelGSOut->columns[elIdx].offset;

      elIdx = m_ModelGSOut->secondaryColumn();

      if(elIdx >= 0 && elIdx < m_ModelGSOut->columns.count())
      {
        m_PostGSSecondary = m_PostGS;
        m_PostGSSecondary.offset += m_ModelGSOut->columns[elIdx].offset;
        m_PostGSSecondary.showAlpha = m_ModelGSOut->secondaryAlpha();
      }
    }

    m_PostGSPosition.idxByteWidth = 0;

    if(!(draw->flags & eDraw_UseIBuffer))
      m_PostVSPosition.idxByteWidth = m_VSInPosition.idxByteWidth = 0;

    m_PostGSPosition.unproject = true;
    m_PostVSPosition.unproject = !m_Ctx.CurPipelineState.IsTessellationEnabled();
  }
  else
  {
    m_VSInPosition = MeshFormat();
    m_VSInSecondary = MeshFormat();

    m_PostVSPosition = MeshFormat();
    m_PostVSSecondary = MeshFormat();

    m_PostGSPosition = MeshFormat();
    m_PostGSSecondary = MeshFormat();
  }

  UpdateMeshConfig();
}

void BufferViewer::configureMeshColumns()
{
  const FetchDrawcall *draw = m_Ctx.CurDrawcall();

  QVector<VertexInputAttribute> vinputs = m_Ctx.CurPipelineState.GetVertexInputs();

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

  QVector<BoundVBuffer> vbs = m_Ctx.CurPipelineState.GetVBuffers();

  Viewport vp = m_Ctx.CurPipelineState.GetViewport(0);

  m_Config.fov = ui->fovGuess->value();
  m_Config.aspect = vp.width / vp.height;
  m_Config.highlightVert = 0;

  if(ui->aspectGuess->value() > 0.0)
    m_Config.aspect = ui->aspectGuess->value();

  if(ui->nearGuess->value() > 0.0)
    m_PostVS.nearPlane = m_PostGS.nearPlane = ui->nearGuess->value();

  if(ui->farGuess->value() > 0.0)
    m_PostVS.farPlane = m_PostGS.farPlane = ui->farGuess->value();

  const ShaderReflection *vs = m_Ctx.CurPipelineState.GetShaderReflection(eShaderStage_Vertex);

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

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      FormatElement pos = m_ModelVSOut->columns[posidx];
      m_ModelVSOut->columns.insert(0, m_ModelVSOut->columns.takeAt(posidx));
    }

    i = 0;
    uint32_t offset = 0;
    for(const FormatElement &sig : m_ModelVSOut->columns)
    {
      uint numComps = sig.format.compCount;
      uint elemSize = sig.format.compType == eCompType_Double ? 8U : 4U;

      if(m_Ctx.CurPipelineState.HasAlignedPostVSData())
      {
        if(numComps == 2)
          offset = AlignUp(offset, 2U * elemSize);
        else if(numComps > 2)
          offset = AlignUp(offset, 4U * elemSize);
      }

      m_ModelVSOut->columns[i++].offset = offset;

      offset += numComps * elemSize;
    }
  }

  m_ModelGSOut->columns.clear();

  if(draw)
  {
    const ShaderReflection *last = m_Ctx.CurPipelineState.GetShaderReflection(eShaderStage_Geometry);
    if(last == NULL)
      last = m_Ctx.CurPipelineState.GetShaderReflection(eShaderStage_Domain);

    if(last)
    {
      m_ModelGSOut->columns.reserve(last->OutputSig.count);

      int i = 0, posidx = -1;
      for(const SigParameter &sig : last->OutputSig)
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

        m_ModelGSOut->columns.push_back(f);

        i++;
      }

      // shift position attribute up to first, keeping order otherwise
      // the same
      if(posidx > 0)
      {
        FormatElement pos = m_ModelGSOut->columns[posidx];
        m_ModelGSOut->columns.insert(0, m_ModelGSOut->columns.takeAt(posidx));
      }

      i = 0;
      uint32_t offset = 0;
      for(const FormatElement &sig : m_ModelGSOut->columns)
      {
        uint numComps = sig.format.compCount;
        uint elemSize = sig.format.compType == eCompType_Double ? 8U : 4U;

        if(m_Ctx.CurPipelineState.HasAlignedPostVSData())
        {
          if(numComps == 2)
            offset = AlignUp(offset, 2U * elemSize);
          else if(numComps > 2)
            offset = AlignUp(offset, 4U * elemSize);
        }

        m_ModelGSOut->columns[i++].offset = offset;

        offset += numComps * elemSize;
      }
    }
  }
}

void BufferViewer::ApplyColumnWidths(int numColumns, RDTableView *view)
{
  int start = 0;

  if(m_MeshView)
  {
    view->setColumnWidth(0, m_IdxColWidth);
    view->setColumnWidth(1, m_IdxColWidth);
    start = 2;
  }

  for(int i = start; i < numColumns; i++)
    view->setColumnWidth(i, m_DataColWidth);
}

void BufferViewer::UpdateMeshConfig()
{
  m_Config.type = m_CurStage;
  switch(m_CurStage)
  {
    case eMeshDataStage_VSIn:
      m_Config.position = m_VSInPosition;
      m_Config.second = m_VSInSecondary;
      break;
    case eMeshDataStage_VSOut:
      m_Config.position = m_PostVSPosition;
      m_Config.second = m_PostVSSecondary;
      break;
    case eMeshDataStage_GSOut:
      m_Config.position = m_PostGSPosition;
      m_Config.second = m_PostGSSecondary;
      break;
    default: break;
  }
}

void BufferViewer::render_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
    return;

  if(m_CurrentCamera)
    m_CurrentCamera->MouseMove(e);

  if(e->buttons() & Qt::RightButton)
    render_clicked(e);

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_clicked(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
    return;

  QPoint curpos = e->pos();

  if((e->buttons() & Qt::RightButton) && m_Output)
  {
    m_Ctx.Renderer().AsyncInvoke("PickVertex", [this, curpos](IReplayRenderer *r) {
      uint32_t instanceSelected = 0;
      uint32_t vertSelected = m_Output->PickVertex(m_Ctx.CurEvent(), (uint32_t)curpos.x(),
                                                   (uint32_t)curpos.y(), &instanceSelected);

      if(vertSelected != ~0U)
      {
        GUIInvoke::call([this, vertSelected, instanceSelected] {
          int row = (int)vertSelected;

          if(instanceSelected != m_Config.curInstance)
            ui->instance->setValue(instanceSelected);

          BufferItemModel *model = currentBufferModel();

          if(model && row >= 0 && row < model->rowCount())
            ScrollToRow(model, row);

          SyncViews(currentTable(), true, true);
        });
      }
    });
  }

  if(m_CurrentCamera)
    m_CurrentCamera->MouseClick(e);

  ui->render->setFocus();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::ScrollToRow(BufferItemModel *model, int row)
{
  model->view->scrollTo(model->index(row, 0), QAbstractItemView::PositionAtTop);
  model->view->clearSelection();
  model->view->selectRow(row);
}

void BufferViewer::ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                              const QString &format)
{
  if(!m_Ctx.LogLoaded())
    return;

  m_IsBuffer = true;
  m_ByteOffset = byteOffset;
  m_ByteSize = byteSize;
  m_BufferID = id;

  FetchBuffer *buf = m_Ctx.GetBuffer(id);
  if(buf)
    setWindowTitle(ToQStr(buf->name) + " - Contents");

  processFormat(format);
}

void BufferViewer::ViewTexture(uint32_t arrayIdx, uint32_t mip, ResourceId id, const QString &format)
{
  if(!m_Ctx.LogLoaded())
    return;

  m_IsBuffer = false;
  m_TexArrayIdx = arrayIdx;
  m_TexMip = mip;
  m_BufferID = id;

  FetchTexture *tex = m_Ctx.GetTexture(id);
  if(tex)
    setWindowTitle(ToQStr(tex->name) + " - Contents");

  processFormat(format);
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

RDTableView *BufferViewer::currentTable()
{
  if(m_CurStage == eMeshDataStage_VSIn)
    return ui->vsinData;
  else if(m_CurStage == eMeshDataStage_VSOut)
    return ui->vsoutData;
  else if(m_CurStage == eMeshDataStage_GSOut)
    return ui->gsoutData;

  return NULL;
}

BufferItemModel *BufferViewer::currentBufferModel()
{
  if(m_CurStage == eMeshDataStage_VSIn)
    return m_ModelVSIn;
  else if(m_CurStage == eMeshDataStage_VSOut)
    return m_ModelVSOut;
  else if(m_CurStage == eMeshDataStage_GSOut)
    return m_ModelGSOut;

  return NULL;
}

bool BufferViewer::isCurrentRasterOut()
{
  if(m_CurStage == eMeshDataStage_VSIn)
  {
    return false;
  }
  else if(m_CurStage == eMeshDataStage_VSOut)
  {
    if(m_Ctx.LogLoaded() && m_Ctx.CurPipelineState.IsTessellationEnabled())
      return false;

    return true;
  }
  else if(m_CurStage == eMeshDataStage_GSOut)
  {
    return true;
  }

  return false;
}

void BufferViewer::Reset()
{
  m_Output = NULL;

  ClearModels();

  CaptureContext *ctx = &m_Ctx;

  // while a log is loaded, pass NULL into the widget
  if(!m_Ctx.LogLoaded())
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

  FormatElement("ColumnSizeTest", 0, 0, false, 1, false, 1, floatFmt, false);
  FormatElement("ColumnSizeTest", 0, 0, false, 1, false, 1, intFmt, true);
  FormatElement("ColumnSizeTest", 0, 0, false, 1, false, 1, intFmt, false);

  m_ModelVSIn->columns.clear();
  m_ModelVSIn->columns.push_back(
      FormatElement("ColumnSizeTest", 0, 0, false, 1, false, 1, floatFmt, false));
  m_ModelVSIn->columns.push_back(
      FormatElement("ColumnSizeTest", 0, 4, false, 1, false, 1, floatFmt, false));
  m_ModelVSIn->columns.push_back(
      FormatElement("ColumnSizeTest", 0, 8, false, 1, false, 1, floatFmt, false));
  m_ModelVSIn->columns.push_back(
      FormatElement("ColumnSizeTest", 0, 12, false, 1, false, 1, intFmt, true));
  m_ModelVSIn->columns.push_back(
      FormatElement("ColumnSizeTest", 0, 16, false, 1, false, 1, intFmt, false));

  m_ModelVSIn->numRows = 2;

  m_ModelVSIn->indices.stride = sizeof(uint32_t);
  m_ModelVSIn->indices.data = new byte[sizeof(uint32_t) * 2];
  m_ModelVSIn->indices.end = m_ModelVSIn->indices.data + sizeof(uint32_t) * 2;

  uint32_t *indices = (uint32_t *)m_ModelVSIn->indices.data;
  indices[0] = 0;
  indices[1] = 1000000;

  m_ModelVSIn->buffers.clear();

  struct TestData
  {
    float f[3];
    uint32_t ui[3];
  };

  BufferData bufdata;
  bufdata.stride = sizeof(TestData);
  bufdata.data = new byte[sizeof(TestData)];
  bufdata.end = bufdata.data + sizeof(TestData);
  m_ModelVSIn->buffers.push_back(bufdata);

  TestData *test = (TestData *)bufdata.data;

  test->f[0] = 1.0f;
  test->f[1] = 1.2345e-20f;
  test->f[2] = 123456.7890123456789f;

  test->ui[1] = 0x12345678;
  test->ui[2] = 0xffffffff;

  m_ModelVSIn->endReset();

  // measure this data so we can use this as column widths
  ui->vsinData->resizeColumnsToContents();

  // index column
  int col = 0;
  if(m_MeshView)
  {
    m_IdxColWidth = ui->vsinData->columnWidth(1);
    col = 2;
  }

  m_DataColWidth = 10;
  for(int c = 0; c < 5; c++)
  {
    int colWidth = ui->vsinData->columnWidth(col + c);
    m_DataColWidth = qMax(m_DataColWidth, colWidth);
  }
}

void BufferViewer::data_selected(const QItemSelection &selected, const QItemSelection &deselected)
{
  m_CurView = qobject_cast<RDTableView *>(QObject::sender());

  if(selected.count() > 0)
  {
    UpdateHighlightVerts();

    SyncViews(qobject_cast<RDTableView *>(QObject::sender()), true, false);

    INVOKE_MEMFN(RT_UpdateAndDisplay);
  }
}

void BufferViewer::data_scrolled(int scrollvalue)
{
  SyncViews(qobject_cast<RDTableView *>(QObject::sender()), false, true);
}

void BufferViewer::camGuess_changed(double value)
{
  m_Config.ortho = (ui->matrixType->currentIndex() == 1);

  m_Config.fov = ui->fovGuess->value();

  m_Config.aspect = 1.0f;

  // take a guess for the aspect ratio, for if the user hasn't overridden it
  Viewport vp = m_Ctx.CurPipelineState.GetViewport(0);
  m_Config.aspect = vp.width / vp.height;

  if(ui->aspectGuess->value() > 0.0)
    m_Config.aspect = ui->aspectGuess->value();

  // use estimates from post vs data (calculated from vertex position data) if the user
  // hasn't overridden the values
  m_Config.position.nearPlane = 0.1f;

  if(m_CurStage == eMeshDataStage_VSOut)
    m_Config.position.nearPlane = m_PostVS.nearPlane;
  else if(m_CurStage == eMeshDataStage_GSOut)
    m_Config.position.nearPlane = m_PostGS.nearPlane;

  if(ui->nearGuess->value() > 0.0)
    m_Config.position.nearPlane = ui->nearGuess->value();

  m_Config.position.farPlane = 100.0f;

  if(m_CurStage == eMeshDataStage_VSOut)
    m_Config.position.farPlane = m_PostVS.farPlane;
  else if(m_CurStage == eMeshDataStage_GSOut)
    m_Config.position.farPlane = m_PostGS.farPlane;

  if(ui->nearGuess->value() > 0.0)
    m_Config.position.farPlane = ui->nearGuess->value();

  if(ui->farGuess->value() > 0.0)
    m_Config.position.nearPlane = ui->farGuess->value();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::processFormat(const QString &format)
{
  QString errors;

  Reset();

  CalcColumnWidth();

  ClearModels();

  m_ModelVSIn->columns = FormatElement::ParseFormatString(format, 0, true, errors);

  ui->formatSpecifier->setErrors(errors);

  OnEventChanged(m_Ctx.CurEvent());
}

void BufferViewer::exportData(const BufferExport &params)
{
  if(!m_Ctx.LogLoaded())
    return;

  if(!m_Ctx.CurDrawcall())
    return;

  if(!m_CurView)
    return;

  QString filter;
  if(params.format == BufferExport::CSV)
    filter = "CSV Files (*.csv)";
  else if(params.format == BufferExport::RawBytes)
    filter = "Binary Files (*.bin)";

  QString filename = RDDialog::getSaveFileName(this, tr("Export buffer to bytes"), QString(),
                                               QString("%1;;All files (*.*)").arg(filter));

  if(filename.isEmpty())
    return;

  QFile *f = new QFile(filename);

  if(!f->open(QIODevice::WriteOnly | QFile::Truncate))
  {
    delete f;
    RDDialog::critical(this, tr("Error exporting file"),
                       tr("Couldn't open file '%1' for writing").arg(filename));
    return;
  }

  BufferItemModel *model = (BufferItemModel *)m_CurView->model();

  LambdaThread *exportThread = new LambdaThread([this, params, model, f]() {
    if(params.format == BufferExport::RawBytes)
    {
      if(!m_MeshView)
      {
        // this is the simplest possible case, we just dump the contents of the first buffer, as
        // it's tightly packed
        f->write((const char *)model->buffers[0].data,
                 int(model->buffers[0].end - model->buffers[0].data));
      }
      else
      {
        // cache column data for the inner loop
        struct CachedElData
        {
          const FormatElement *el = NULL;

          const char *data = NULL;
          const char *end = NULL;

          size_t stride;
          int byteSize;
          uint32_t instIdx = 0;

          QByteArray nulls;
        };
        QVector<CachedElData> cache;
        cache.reserve(model->columns.count());

        for(int col = 0; col < model->columns.count(); col++)
        {
          const FormatElement &el = model->columns[col];

          CachedElData d;

          d.el = &el;

          d.byteSize = el.byteSize();
          d.nulls = QByteArray(d.byteSize, '\0');

          if(el.instancerate > 0)
            d.instIdx = model->curInstance / el.instancerate;

          if(el.buffer < model->buffers.size())
          {
            d.data = (const char *)model->buffers[el.buffer].data;
            d.end = (const char *)model->buffers[el.buffer].end;

            d.stride = model->buffers[el.buffer].stride;

            d.data += el.offset;

            if(el.perinstance)
              d.data += d.stride * d.instIdx;
          }

          cache.push_back(d);
        }

        // go row by row, finding the start of the row and dumping out the elements using their
        // offset and sizes
        for(int i = 0; i < model->rowCount(); i++)
        {
          uint32_t idx = model->data(model->index(i, 1), Qt::DisplayRole).toUInt();

          for(int col = 0; col < cache.count(); col++)
          {
            const CachedElData &d = cache[col];
            const FormatElement *el = d.el;

            if(d.data)
            {
              const char *bytes = d.data;

              if(!el->perinstance)
                bytes += d.stride * idx;

              if(bytes + d.byteSize <= d.end)
                f->write(bytes, d.byteSize);
            }

            // if we didn't continue above, something was wrong, so write nulls
            f->write(d.nulls);
          }
        }
      }
    }
    else if(params.format == BufferExport::CSV)
    {
      // this works identically no matter whether we're mesh view or what, we just iterate the
      // elements and call the model's data()

      QTextStream s(f);

      for(int i = 0; i < model->columnCount(); i++)
      {
        s << model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();

        if(i + 1 < model->columnCount())
          s << ", ";
      }

      s << "\n";

      for(int row = 0; row < model->rowCount(); row++)
      {
        for(int col = 0; col < model->columnCount(); col++)
        {
          s << model->data(model->index(row, col), Qt::DisplayRole).toString();

          if(col + 1 < model->columnCount())
            s << ", ";
        }

        s << "\n";
      }
    }

    f->close();

    delete f;
  });
  exportThread->start();

  ShowProgressDialog(this, tr("Exporting data"),
                     [exportThread]() { return !exportThread->isRunning(); });

  exportThread->deleteLater();
}

void BufferViewer::debugVertex()
{
  if(!m_Ctx.LogLoaded())
    return;

  if(!m_Ctx.CurDrawcall())
    return;

  if(!m_CurView)
    return;

  QModelIndex idx = m_CurView->selectionModel()->currentIndex();

  if(!idx.isValid())
  {
    GUIInvoke::call([this]() {
      RDDialog::critical(this, tr("Error debugging"),
                         tr("Error debugging vertex - make sure a valid vertex is selected"));
    });
    return;
  }

  uint32_t vertid =
      m_CurView->model()->data(m_CurView->model()->index(idx.row(), 0), Qt::DisplayRole).toUInt();
  uint32_t index =
      m_CurView->model()->data(m_CurView->model()->index(idx.row(), 1), Qt::DisplayRole).toUInt();

  m_Ctx.Renderer().AsyncInvoke([this, vertid, index](IReplayRenderer *r) {
    ShaderDebugTrace *trace = new ShaderDebugTrace;

    bool success =
        r->DebugVertex(vertid, m_Config.curInstance, index, m_Ctx.CurDrawcall()->instanceOffset,
                       m_Ctx.CurDrawcall()->vertexOffset, trace);

    if(!success || trace->states.count == 0)
    {
      delete trace;

      // if we couldn't debug the pixel on this event, open up a pixel history
      GUIInvoke::call([this]() {
        RDDialog::critical(this, tr("Error debugging"),
                           tr("Error debugging vertex - make sure a valid vertex is selected"));
      });
      return;
    }

    GUIInvoke::call([this, vertid, trace]() {
      QString debugContext = tr("Vertex %1").arg(vertid);

      if(m_Ctx.CurDrawcall()->numInstances > 1)
        debugContext += tr(", Instance %1").arg(m_Config.curInstance);

      const ShaderReflection *shaderDetails =
          m_Ctx.CurPipelineState.GetShaderReflection(eShaderStage_Pixel);
      const ShaderBindpointMapping &bindMapping =
          m_Ctx.CurPipelineState.GetBindpointMapping(eShaderStage_Pixel);

      // viewer takes ownership of the trace
      ShaderViewer *s = ShaderViewer::debugShader(m_Ctx, &bindMapping, shaderDetails,
                                                  eShaderStage_Pixel, trace, debugContext, this);

      m_Ctx.setupDockWindow(s);

      ToolWindowManager *manager = ToolWindowManager::managerOf(this);

      ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
      manager->addToolWindow(s, ref);
    });
  });
}

void BufferViewer::SyncViews(RDTableView *primary, bool selection, bool scroll)
{
  if(!ui->syncViews->isChecked())
    return;

  RDTableView *views[] = {ui->vsinData, ui->vsoutData, ui->gsoutData};

  if(primary == NULL)
  {
    for(RDTableView *table : views)
    {
      if(table->hasFocus())
      {
        primary = table;
        break;
      }
    }
  }

  if(primary == NULL)
    primary = views[0];

  for(RDTableView *table : views)
  {
    if(table == primary)
      continue;

    if(selection)
    {
      QModelIndexList selected = primary->selectionModel()->selectedRows();
      if(!selected.empty())
        table->selectRow(selected[0].row());
    }

    if(scroll)
      table->verticalScrollBar()->setValue(primary->verticalScrollBar()->value());
  }
}

void BufferViewer::UpdateHighlightVerts()
{
  m_Config.highlightVert = ~0U;

  if(!ui->highlightVerts->isChecked())
    return;

  RDTableView *table = currentTable();

  if(!table)
    return;

  QModelIndexList selected = table->selectionModel()->selectedRows();

  if(selected.empty())
    return;

  m_Config.highlightVert = selected[0].row();
}

void BufferViewer::EnableCameraGuessControls()
{
  ui->aspectGuess->setEnabled(isCurrentRasterOut());
  ui->nearGuess->setEnabled(isCurrentRasterOut());
  ui->farGuess->setEnabled(isCurrentRasterOut());
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

  ui->drawRange->setEnabled(index > 0);

  on_resetCamera_clicked();
  ui->autofitCamera->setEnabled(!isCurrentRasterOut());

  EnableCameraGuessControls();

  UpdateMeshConfig();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_toggleControls_toggled(bool checked)
{
  ui->cameraControlsGroup->setVisible(checked);

  EnableCameraGuessControls();
}

void BufferViewer::on_syncViews_toggled(bool checked)
{
  SyncViews(NULL, true, true);
}

void BufferViewer::on_highlightVerts_toggled(bool checked)
{
  UpdateHighlightVerts();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_wireframeRender_toggled(bool checked)
{
  m_Config.wireframeDraw = checked;

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_solidShading_currentIndexChanged(int index)
{
  ui->wireframeRender->setEnabled(index > 0);

  if(!ui->wireframeRender->isEnabled())
  {
    ui->wireframeRender->setChecked(true);
    m_Config.wireframeDraw = true;
  }

  m_Config.solidShadeMode = (SolidShadeMode)index;

  m_ModelVSIn->setSecondaryColumn(m_ModelVSIn->secondaryColumn(),
                                  m_Config.solidShadeMode == eShade_Secondary,
                                  m_ModelVSIn->secondaryAlpha());
  m_ModelVSOut->setSecondaryColumn(m_ModelVSOut->secondaryColumn(),
                                   m_Config.solidShadeMode == eShade_Secondary,
                                   m_ModelVSOut->secondaryAlpha());
  m_ModelGSOut->setSecondaryColumn(m_ModelGSOut->secondaryColumn(),
                                   m_Config.solidShadeMode == eShade_Secondary,
                                   m_ModelGSOut->secondaryAlpha());

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_drawRange_currentIndexChanged(int index)
{
  /*
  "Only this draw",
  "Show previous instances",
  "Show all instances",
  "Show whole pass"
   */

  m_Config.showPrevInstances = (index >= 1);
  m_Config.showAllInstances = (index >= 2);
  m_Config.showWholePass = (index >= 3);

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_controlType_currentIndexChanged(int index)
{
  m_Arcball->Reset(FloatVector(), 10.0f);
  m_Flycam->Reset(FloatVector());

  if(index == 0)
  {
    m_CurrentCamera = m_Arcball;
  }
  else
  {
    m_CurrentCamera = m_Flycam;
    if(isCurrentRasterOut())
      m_Flycam->Reset(FloatVector(0.0f, 0.0f, 0.0f, 0.0f));
    else
      m_Flycam->Reset(FloatVector(0.0f, 0.0f, -10.0f, 0.0f));
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_resetCamera_clicked()
{
  if(isCurrentRasterOut())
    ui->controlType->setCurrentIndex(1);
  else
    ui->controlType->setCurrentIndex(0);

  // make sure callback is called even if we're re-selecting same
  // camera type
  on_controlType_currentIndexChanged(ui->controlType->currentIndex());
}

void BufferViewer::on_camSpeed_valueChanged(double value)
{
  m_Arcball->SpeedMultiplier = m_Flycam->SpeedMultiplier = value;
}

void BufferViewer::on_instance_valueChanged(int value)
{
  m_Config.curInstance = value;
  OnEventChanged(m_Ctx.CurEvent());
}

void BufferViewer::on_rowOffset_valueChanged(int value)
{
  ScrollToRow(m_ModelVSIn, value);
  ScrollToRow(m_ModelVSOut, value);
  ScrollToRow(m_ModelGSOut, value);
}

void BufferViewer::on_autofitCamera_clicked()
{
}
