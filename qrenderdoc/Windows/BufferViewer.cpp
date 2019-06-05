/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include <float.h>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QItemSelection>
#include <QMenu>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QScrollBar>
#include <QTimer>
#include <QtMath>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "ui_BufferViewer.h"

class CameraWrapper
{
public:
  virtual ~CameraWrapper() {}
  virtual bool Update(QRect winSize) = 0;
  virtual ICamera *camera() = 0;

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

    if(e->modifiers() & Qt::ShiftModifier)
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

    if(e->modifiers() & Qt::ShiftModifier)
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
  ArcballWrapper() { m_Cam = RENDERDOC_InitCamera(CameraType::Arcball); }
  virtual ~ArcballWrapper() { m_Cam->Shutdown(); }
  ICamera *camera() override { return m_Cam; }
  void Reset(FloatVector pos, float dist)
  {
    m_Cam->ResetArcball();

    setLookAtPos(pos);
    SetDistance(dist);
  }

  void SetDistance(float dist)
  {
    m_Distance = qAbs(dist);
    m_Cam->SetArcballDistance(m_Distance);
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

        FloatVector right = m_Cam->GetRight();
        FloatVector up = m_Cam->GetUp();

        m_LookAt.x -= right.x * xdelta;
        m_LookAt.y -= right.y * xdelta;
        m_LookAt.z -= right.z * xdelta;

        m_LookAt.x += up.x * ydelta;
        m_LookAt.y += up.y * ydelta;
        m_LookAt.z += up.z * ydelta;

        m_Cam->SetPosition(m_LookAt.x, m_LookAt.y, m_LookAt.z);
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
    m_Cam->SetPosition(v.x, v.y, v.z);
  }

private:
  ICamera *m_Cam;

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

    m_Cam->RotateArcball(ax, ay, bx, by);
  }
};

class FlycamWrapper : public CameraWrapper
{
public:
  FlycamWrapper() { m_Cam = RENDERDOC_InitCamera(CameraType::FPSLook); }
  virtual ~FlycamWrapper() { m_Cam->Shutdown(); }
  ICamera *camera() override { return m_Cam; }
  void Reset(FloatVector pos)
  {
    m_Position = pos;
    m_Rotation = FloatVector();

    m_Cam->SetPosition(m_Position.x, m_Position.y, m_Position.z);
    m_Cam->SetFPSRotation(m_Rotation.x, m_Rotation.y, m_Rotation.z);
  }

  bool Update(QRect size) override
  {
    FloatVector fwd = m_Cam->GetForward();
    FloatVector right = m_Cam->GetRight();

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
      m_Cam->SetPosition(m_Position.x, m_Position.y, m_Position.z);
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

      m_Cam->SetFPSRotation(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    }

    CameraWrapper::MouseMove(e);
  }

private:
  ICamera *m_Cam;

  FloatVector m_Position, m_Rotation;
};

struct BufferData
{
  BufferData()
  {
    refcount.store(1);
    stride = 0;
  }

  void ref() { refcount.ref(); }
  void deref()
  {
    bool alive = refcount.deref();

    if(!alive)
      delete this;
  }

  size_t stride;
  bytebuf storage;
  QAtomicInteger<uint32_t> refcount;

  const byte *data() const { return storage.begin(); };
  const byte *end() const { return storage.end(); }
  bool hasData() const { return !storage.empty(); }
  size_t size() const { return storage.size(); }
};

struct BufferConfiguration
{
  uint32_t curInstance = 0, curView = 0;
  uint32_t numRows = 0, unclampedNumRows = 0;

  // we can have two index buffers for VSOut data:
  // the original index buffer is used for the displayed value (in displayIndices), and the actual
  // potentially remapped or permuated index buffer used for fetching data (in indices).
  BufferData *displayIndices = NULL;
  int32_t displayBaseVertex = 0;
  BufferData *indices = NULL;
  int32_t baseVertex = 0;

  QList<FormatElement> columns;
  QVector<PixelValue> generics;
  QVector<bool> genericsEnabled;
  QList<BufferData *> buffers;
  uint32_t primRestart = 0;

  BufferConfiguration() = default;
  BufferConfiguration(const BufferConfiguration &o) = delete;
  ~BufferConfiguration() { reset(); }
  BufferConfiguration &operator=(const BufferConfiguration &o)
  {
    reset();

    curInstance = o.curInstance;
    numRows = o.numRows;
    unclampedNumRows = o.unclampedNumRows;

    displayIndices = o.displayIndices;
    if(displayIndices)
      displayIndices->ref();
    displayBaseVertex = o.displayBaseVertex;

    indices = o.indices;
    if(indices)
      indices->ref();

    baseVertex = o.baseVertex;

    columns = o.columns;
    generics = o.generics;
    genericsEnabled = o.genericsEnabled;
    primRestart = o.primRestart;

    buffers = o.buffers;
    for(BufferData *b : buffers)
      b->ref();

    return *this;
  }

  void reset()
  {
    if(indices)
      indices->deref();
    indices = NULL;

    if(displayIndices)
      displayIndices->deref();
    displayIndices = NULL;

    for(BufferData *b : buffers)
      b->deref();

    buffers.clear();
    columns.clear();
    generics.clear();
    genericsEnabled.clear();
    numRows = 0;
    unclampedNumRows = 0;
  }

  QString columnName(int col) const
  {
    if(col >= 0 && col < columns.count())
      return columns[col].name;

    return QString();
  }

  int guessPositionColumn() const
  {
    int posEl = -1;

    if(!columns.empty())
    {
      // prioritise system value over general "POSITION" string matching
      for(int i = 0; i < columns.count(); i++)
      {
        const FormatElement &el = columns[i];

        if(el.systemValue == ShaderBuiltin::Position)
        {
          posEl = i;
          break;
        }
      }

      // look for an exact match
      for(int i = 0; posEl == -1 && i < columns.count(); i++)
      {
        const FormatElement &el = columns[i];

        if(el.name.compare(lit("POSITION"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("POSITION0"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("POS"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("POS0"), Qt::CaseInsensitive) == 0)
        {
          posEl = i;
          break;
        }
      }

      // try anything containing position
      for(int i = 0; posEl == -1 && i < columns.count(); i++)
      {
        const FormatElement &el = columns[i];

        if(el.name.contains(lit("POSITION"), Qt::CaseInsensitive))
        {
          posEl = i;
          break;
        }
      }

      // OK last resort, just look for 'pos'
      for(int i = 0; posEl == -1 && i < columns.count(); i++)
      {
        const FormatElement &el = columns[i];

        if(el.name.contains(lit("POS"), Qt::CaseInsensitive))
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

    return posEl;
  }

  int guessSecondaryColumn() const
  {
    int secondEl = -1;

    if(!columns.empty())
    {
      // prioritise TEXCOORD over general COLOR
      for(int i = 0; i < columns.count(); i++)
      {
        const FormatElement &el = columns[i];

        if(el.name.compare(lit("TEXCOORD"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("TEXCOORD0"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("TEX"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("TEX0"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("UV"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("UV0"), Qt::CaseInsensitive) == 0)
        {
          secondEl = i;
          break;
        }
      }

      for(int i = 0; secondEl == -1 && i < columns.count(); i++)
      {
        const FormatElement &el = columns[i];

        if(el.name.compare(lit("COLOR"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("COLOR0"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("COL"), Qt::CaseInsensitive) == 0 ||
           el.name.compare(lit("COL0"), Qt::CaseInsensitive) == 0)
        {
          secondEl = i;
          break;
        }
      }
    }

    return secondEl;
  }
};

uint32_t CalcIndex(BufferData *data, uint32_t vertID, int32_t baseVertex, uint32_t primRestart)
{
  const byte *idxData = data->data() + vertID * sizeof(uint32_t);
  if(idxData + sizeof(uint32_t) > data->end())
    return ~0U;

  uint32_t idx = *(const uint32_t *)idxData;

  // check for primitive restart *before* adding base vertex
  if(primRestart && idx == primRestart)
    return idx;

  // apply base vertex but clamp to 0 if subtracting
  if(baseVertex < 0)
  {
    uint32_t subtract = (uint32_t)(-baseVertex);

    if(idx < subtract)
      idx = 0;
    else
      idx -= subtract;
  }
  else if(baseVertex > 0)
  {
    idx += (uint32_t)baseVertex;
  }

  return idx;
}

static int columnGroupRole = Qt::UserRole + 10000;

class BufferItemModel : public QAbstractItemModel
{
public:
  BufferItemModel(RDTableView *v, bool vertexInput, bool mesh, QObject *parent)
      : QAbstractItemModel(parent)
  {
    vertexInputData = vertexInput;
    meshView = mesh;
    view = v;
    view->setModel(this);
  }
  void beginReset()
  {
    emit beginResetModel();
    config.reset();
  }
  void endReset(const BufferConfiguration &conf)
  {
    config = conf;
    cacheColumns();
    totalColumnCount = columnLookup.count() + reservedColumnCount();
    emit endResetModel();
  }
  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount())
      return QModelIndex();

    return createIndex(row, column);
  }

  QModelIndex parent(const QModelIndex &index) const override { return QModelIndex(); }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return config.numRows; }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return totalColumnCount;
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(section < totalColumnCount && orientation == Qt::Horizontal)
    {
      if(role == Qt::DisplayRole || role == columnGroupRole)
      {
        if(section == 0)
        {
          return meshView ? lit("VTX") : lit("Element");
        }
        else if(section == 1 && meshView)
        {
          return lit("IDX");
        }
        else
        {
          const FormatElement &el = elementForColumn(section);

          if(el.format.compCount == 1 || role == columnGroupRole)
            return el.name;

          QChar comps[] = {QLatin1Char('x'), QLatin1Char('y'), QLatin1Char('z'), QLatin1Char('w')};

          return QFormatStr("%1.%2").arg(el.name).arg(comps[componentForIndex(section)]);
        }
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
        if(index.column() < reservedColumnCount())
          opt.text = lit("4294967295");
        else
          opt.text = data(index).toString();

        opt.text.replace(QLatin1Char('\n'), QChar::LineSeparator);

        opt.styleObject = NULL;

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        return style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), opt.widget);
      }

      uint32_t row = index.row();
      int col = index.column();

      if(role == columnGroupRole)
      {
        if(col < reservedColumnCount())
          return -1 - col;
        else
          return columnLookup[col - reservedColumnCount()];
      }

      if((role == Qt::BackgroundRole || role == Qt::ForegroundRole) && col >= reservedColumnCount())
      {
        if(meshView)
        {
          int elIdx = columnLookup[col - reservedColumnCount()];
          int compIdx = componentForIndex(col);

          float lightnessOn = qBound(0.25, view->palette().color(QPalette::Base).lightnessF(), 0.75);
          float lightnessOff = lightnessOn > 0.5f ? lightnessOn + 0.2f : lightnessOn - 0.2f;

          static float a = 0.55f;
          static float b = 0.8f;

          if(elIdx == positionEl)
          {
            QColor backCol;
            if(compIdx != 3 || !vertexInputData)
            {
              backCol = QColor::fromHslF(0.55f, 0.75f, lightnessOn);
            }
            else
            {
              backCol = QColor::fromHslF(0.55f, 0.75f, lightnessOff);
            }

            if(role == Qt::ForegroundRole)
              return QBrush(contrastingColor(backCol, view->palette().color(QPalette::Text)));

            return backCol;
          }
          else if(secondaryEnabled && elIdx == secondaryEl)
          {
            QColor backCol;
            if((secondaryElAlpha && compIdx == 3) || (!secondaryElAlpha && compIdx != 3))
            {
              backCol = QColor::fromHslF(0.33f, 0.75f, lightnessOn);
            }
            else
            {
              backCol = QColor::fromHslF(0.33f, 0.75f, lightnessOff);
            }

            if(role == Qt::ForegroundRole)
              return QBrush(contrastingColor(backCol, view->palette().color(QPalette::Text)));

            return backCol;
          }
        }
        else
        {
          const FormatElement &el = elementForColumn(col);

          if(el.rgb && el.buffer < config.buffers.size())
          {
            const byte *data = config.buffers[el.buffer]->data();
            const byte *end = config.buffers[el.buffer]->end();

            data += config.buffers[el.buffer]->stride * row;
            data += el.offset;

            // only slightly wasteful, we need to fetch all variants together
            // since some formats are packed and can't be read individually
            QVariantList list = el.GetVariants(data, end);

            if(!list.isEmpty())
            {
              QMetaType::Type vt = GetVariantMetatype(list[0]);

              QColor rgb;

              if(vt == QMetaType::Double)
              {
                double r = qBound(0.0, list[0].toDouble(), 1.0);
                double g = list.size() > 1 ? qBound(0.0, list[1].toDouble(), 1.0) : 0.0;
                double b = list.size() > 2 ? qBound(0.0, list[2].toDouble(), 1.0) : 0.0;

                rgb = QColor::fromRgbF(r, g, b);
              }
              else if(vt == QMetaType::Float)
              {
                float r = qBound(0.0f, list[0].toFloat(), 1.0f);
                float g = list.size() > 1 ? qBound(0.0f, list[1].toFloat(), 1.0f) : 0.0;
                float b = list.size() > 2 ? qBound(0.0f, list[2].toFloat(), 1.0f) : 0.0;

                rgb = QColor::fromRgbF(r, g, b);
              }
              else if(vt == QMetaType::UInt || vt == QMetaType::UShort || vt == QMetaType::UChar)
              {
                uint r = qBound(0U, list[0].toUInt(), 255U);
                uint g = list.size() > 1 ? qBound(0U, list[1].toUInt(), 255U) : 0.0;
                uint b = list.size() > 2 ? qBound(0U, list[2].toUInt(), 255U) : 0.0;

                rgb = QColor::fromRgb(r, g, b);
              }
              else if(vt == QMetaType::Int || vt == QMetaType::Short || vt == QMetaType::SChar)
              {
                int r = qBound(0, list[0].toInt(), 255);
                int g = list.size() > 1 ? qBound(0, list[1].toInt(), 255) : 0.0;
                int b = list.size() > 2 ? qBound(0, list[2].toInt(), 255) : 0.0;

                rgb = QColor::fromRgb(r, g, b);
              }

              if(role == Qt::BackgroundRole)
                return QBrush(rgb);
              else if(role == Qt::ForegroundRole)
                return QBrush(contrastingColor(rgb, QColor::fromRgb(0, 0, 0)));
            }
          }
        }
      }

      if(role == Qt::DisplayRole)
      {
        if(config.unclampedNumRows > 0 && row >= config.numRows - 2)
        {
          if(col < 2 && row == config.numRows - 1)
            return QString::number(config.unclampedNumRows - config.numRows);

          return lit("...");
        }

        if(col >= 0 && col < totalColumnCount && row < config.numRows)
        {
          if(col == 0)
            return row;

          uint32_t idx = row;

          if(config.indices && config.indices->hasData())
          {
            idx = CalcIndex(config.indices, row, config.baseVertex, config.primRestart);

            if(config.primRestart && idx == config.primRestart)
              return col == 1 ? lit("--") : lit(" Restart");

            if(idx == ~0U)
              return outOfBounds();
          }

          if(col == 1 && meshView)
          {
            // if we have separate displayIndices, fetch that for display instead
            if(config.displayIndices && config.displayIndices->hasData())
              idx = CalcIndex(config.displayIndices, row, config.displayBaseVertex,
                              config.primRestart);

            if(idx == ~0U)
              return outOfBounds();

            return idx;
          }

          const FormatElement &el = elementForColumn(col);

          if(useGenerics(col))
            return interpretGeneric(col, el);

          uint32_t instIdx = 0;
          if(el.instancerate > 0)
            instIdx = config.curInstance / el.instancerate;

          if(el.buffer < config.buffers.size())
          {
            const byte *data = config.buffers[el.buffer]->data();
            const byte *end = config.buffers[el.buffer]->end();

            if(!el.perinstance)
              data += config.buffers[el.buffer]->stride * idx;
            else
              data += config.buffers[el.buffer]->stride * instIdx;

            data += el.offset;

            // only slightly wasteful, we need to fetch all variants together
            // since some formats are packed and can't be read individually
            QVariantList list = el.GetVariants(data, end);

            int comp = componentForIndex(col);

            if(comp < list.count())
            {
              QString ret;

              uint32_t rowdim = el.matrixdim;
              uint32_t coldim = el.format.compCount;

              for(uint32_t r = 0; r < rowdim; r++)
              {
                if(r > 0)
                  ret += lit("\n");

                if(el.rowmajor)
                  ret += interpretVariant(list[comp + r * coldim], el);
                else
                  ret += interpretVariant(list[r + comp * rowdim], el);
              }

              return ret;
            }
          }

          return outOfBounds();
        }
      }
    }

    return QVariant();
  }

  void setPosColumn(int pos)
  {
    QVector<int> roles = {Qt::BackgroundRole, Qt::ForegroundRole};

    if(pos == -1)
      pos = config.guessPositionColumn();

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
  QString posName() { return config.columnName(positionEl); }
  void setSecondaryColumn(int sec, bool secEnabled, bool secAlpha)
  {
    QVector<int> roles = {Qt::BackgroundRole, Qt::ForegroundRole};

    if(sec == -1)
      sec = config.guessSecondaryColumn();

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
  QString secondaryName() { return config.columnName(secondaryEl); }
  int elementIndexForColumn(int col) const
  {
    if(col < reservedColumnCount())
      return -1;

    return columnLookup[col - reservedColumnCount()];
  }

  const FormatElement &elementForColumn(int col) const
  {
    return config.columns[columnLookup[col - reservedColumnCount()]];
  }

  bool useGenerics(int col) const
  {
    col = columnLookup[col - reservedColumnCount()];
    return col < config.genericsEnabled.size() && config.genericsEnabled[col];
  }

  const BufferConfiguration &getConfig() { return config; }
private:
  // constant data over the item model's lifetime
  // The view that this model is for
  RDTableView *view = NULL;
  // Is this the vertex input stage
  bool vertexInputData = false;
  // are we configured for mesh viewing, or for raw buffer data
  bool meshView = true;

  // the mutable configuration of what we're displaying.
  BufferConfiguration config;

  // Internal cached data, generated by cacheColumns() from endReset().
  // Only accessible to main UI thread

  // maps from column number (0-based from data, so excluding VTX/IDX columns)
  // to the column element in the columns list, and lists its component.
  //
  // So a float4, float3, int set of columns would be:
  // { 0, 0, 0, 0, 1, 1, 1, 2 };
  // { 0, 1, 2, 3, 0, 1, 2, 0 };
  QVector<int> columnLookup;
  QVector<int> componentLookup;
  // the total number of columns including any reserved ones like VTX / IDX
  int totalColumnCount = 0;

  // which format element is selected as position data
  int positionEl = -1;
  // which format element is selected as secondary data
  int secondaryEl = -1;
  // is secondary data enabled
  bool secondaryEnabled = false;
  // are we using the alpha channel for secondary data
  bool secondaryElAlpha = false;

  int reservedColumnCount() const { return (meshView ? 2 : 1); }
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
    columnLookup.reserve(config.columns.count() * 4);
    componentLookup.clear();
    componentLookup.reserve(config.columns.count() * 4);

    for(int i = 0; i < config.columns.count(); i++)
    {
      FormatElement &fmt = config.columns[i];

      uint32_t compCount;

      switch(fmt.format.type)
      {
        case ResourceFormatType::BC6:
        case ResourceFormatType::ETC2:
        case ResourceFormatType::R11G11B10:
        case ResourceFormatType::R5G6B5:
        case ResourceFormatType::R9G9B9E5: compCount = 3; break;
        case ResourceFormatType::BC1:
        case ResourceFormatType::BC7:
        case ResourceFormatType::BC3:
        case ResourceFormatType::BC2:
        case ResourceFormatType::R10G10B10A2:
        case ResourceFormatType::R5G5B5A1:
        case ResourceFormatType::R4G4B4A4:
        case ResourceFormatType::ASTC: compCount = 4; break;
        case ResourceFormatType::BC5:
        case ResourceFormatType::R4G4:
        case ResourceFormatType::D16S8:
        case ResourceFormatType::D24S8:
        case ResourceFormatType::D32S8: compCount = 2; break;
        case ResourceFormatType::BC4:
        case ResourceFormatType::S8: compCount = 1; break;
        case ResourceFormatType::YUV8:
        case ResourceFormatType::YUV10:
        case ResourceFormatType::YUV12:
        case ResourceFormatType::YUV16:
        case ResourceFormatType::EAC:
        default: compCount = fmt.format.compCount;
      }

      for(uint32_t c = 0; c < compCount; c++)
      {
        columnLookup.push_back(i);
        componentLookup.push_back((int)c);
      }
    }
  }

  QString outOfBounds() const { return lit("---"); }
  QString interpretGeneric(int col, const FormatElement &el) const
  {
    int comp = componentForIndex(col);

    col = columnLookup[col - reservedColumnCount()];

    if(col < config.generics.size())
    {
      if(el.format.compType == CompType::Float)
      {
        return interpretVariant(QVariant(config.generics[col].floatValue[comp]), el);
      }
      else if(el.format.compType == CompType::SInt)
      {
        return interpretVariant(QVariant(config.generics[col].intValue[comp]), el);
      }
      else if(el.format.compType == CompType::UInt)
      {
        return interpretVariant(QVariant(config.generics[col].uintValue[comp]), el);
      }
    }

    return outOfBounds();
  }

  QString interpretVariant(const QVariant &v, const FormatElement &el) const
  {
    QString ret;

    QMetaType::Type vt = GetVariantMetatype(v);

    if(vt == QMetaType::Double)
    {
      double d = v.toDouble();
      // pad with space on left if sign is missing, to better align
      if(d < 0.0)
        ret = Formatter::Format(d);
      else if(d > 0.0)
        ret = lit(" ") + Formatter::Format(d);
      else if(qIsNaN(d))
        ret = lit(" NaN");
      else
        // force negative and positive 0 together
        ret = lit(" ") + Formatter::Format(0.0);
    }
    else if(vt == QMetaType::Float)
    {
      float f = v.toFloat();
      // pad with space on left if sign is missing, to better align
      if(f < 0.0)
        ret = Formatter::Format(f);
      else if(f > 0.0)
        ret = lit(" ") + Formatter::Format(f);
      else if(qIsNaN(f))
        ret = lit(" NaN");
      else
        // force negative and positive 0 together
        ret = lit(" ") + Formatter::Format(0.0);
    }
    else if(vt == QMetaType::UInt || vt == QMetaType::UShort || vt == QMetaType::UChar)
    {
      uint u = v.toUInt();
      if(el.hex && el.format.type == ResourceFormatType::Regular)
        ret = Formatter::HexFormat(u, el.format.compByteWidth);
      else
        ret = Formatter::Format(u, el.hex);
    }
    else if(vt == QMetaType::Int || vt == QMetaType::Short || vt == QMetaType::SChar)
    {
      int i = v.toInt();
      if(i >= 0)
        ret = lit(" ") + Formatter::Format(i);
      else
        ret = Formatter::Format(i);
    }
    else
    {
      ret = v.toString();
    }

    return ret;
  }
};

struct CachedElData
{
  const FormatElement *el = NULL;

  const byte *data = NULL;
  const byte *end = NULL;

  size_t stride;
  int byteSize;
  uint32_t instIdx = 0;

  QByteArray nulls;
};

struct PopulateBufferData
{
  int vsinHoriz;
  int vsoutHoriz;
  int gsoutHoriz;

  int vsinVert;
  int vsoutVert;
  int gsoutVert;

  QString highlightNames[6];

  BufferConfiguration vsinConfig, vsoutConfig, gsoutConfig;

  MeshFormat postVS, postGS;
};

struct CalcBoundingBoxData
{
  uint32_t eventId;

  BufferConfiguration input[3];

  BBoxData output;
};

void CacheDataForIteration(QVector<CachedElData> &cache, const QList<FormatElement> &columns,
                           const QList<BufferData *> buffers, uint32_t inst)
{
  cache.reserve(columns.count());

  for(int col = 0; col < columns.count(); col++)
  {
    const FormatElement &el = columns[col];

    CachedElData d;

    d.el = &el;

    d.byteSize = el.byteSize();
    d.nulls = QByteArray(d.byteSize, '\0');

    if(el.instancerate > 0)
      d.instIdx = inst / el.instancerate;

    if(el.buffer < buffers.size())
    {
      d.data = buffers[el.buffer]->data();
      d.end = buffers[el.buffer]->end();

      d.stride = buffers[el.buffer]->stride;

      d.data += el.offset;

      if(el.perinstance)
        d.data += d.stride * d.instIdx;
    }

    cache.push_back(d);
  }
}

static void ConfigureColumnsForShader(ICaptureContext &ctx, const ShaderReflection *shader,
                                      QList<FormatElement> &columns)
{
  if(!shader)
    return;

  columns.reserve(shader->outputSignature.count());

  int i = 0, posidx = -1;
  for(const SigParameter &sig : shader->outputSignature)
  {
    FormatElement f;

    f.buffer = 0;
    f.name = !sig.varName.isEmpty() ? sig.varName : sig.semanticIdxName;
    f.format.compByteWidth = sizeof(float);
    f.format.compCount = sig.compCount;
    f.format.compType = sig.compType;
    f.format.type = ResourceFormatType::Regular;
    f.perinstance = false;
    f.instancerate = 1;
    f.rowmajor = false;
    f.matrixdim = 1;
    f.systemValue = sig.systemValue;

    if(f.systemValue == ShaderBuiltin::Position)
      posidx = i;

    columns.push_back(f);

    i++;
  }

  // shift position attribute up to first, keeping order otherwise
  // the same
  if(posidx > 0)
  {
    FormatElement pos = columns[posidx];
    columns.insert(0, columns.takeAt(posidx));
  }

  i = 0;
  uint32_t offset = 0;
  for(FormatElement &sig : columns)
  {
    uint numComps = sig.format.compCount;
    uint elemSize = sig.format.compType == CompType::Double ? 8U : 4U;

    if(ctx.CurPipelineState().HasAlignedPostVSData(
           shader->stage == ShaderStage::Vertex ? MeshDataStage::VSOut : MeshDataStage::GSOut))
    {
      if(numComps == 2)
        offset = AlignUp(offset, 2U * elemSize);
      else if(numComps > 2)
        offset = AlignUp(offset, 4U * elemSize);
    }

    sig.offset = offset;

    offset += numComps * elemSize;
  }
}

static void ConfigureMeshColumns(ICaptureContext &ctx, PopulateBufferData *bufdata)
{
  const DrawcallDescription *draw = ctx.CurDrawcall();

  rdcarray<VertexInputAttribute> vinputs = ctx.CurPipelineState().GetVertexInputs();

  bufdata->vsinConfig.columns.reserve(vinputs.count());
  bufdata->vsinConfig.columns.clear();
  bufdata->vsinConfig.genericsEnabled.resize(vinputs.count());
  bufdata->vsinConfig.generics.resize(vinputs.count());

  for(const VertexInputAttribute &a : vinputs)
  {
    if(!a.used)
      continue;

    FormatElement f(a.name, a.vertexBuffer, a.byteOffset, a.perInstance, a.instanceRate,
                    false,    // row major matrix
                    1,        // matrix dimension
                    a.format, false, false);

    bufdata->vsinConfig.genericsEnabled[bufdata->vsinConfig.columns.size()] = false;

    if(a.genericEnabled)
    {
      bufdata->vsinConfig.genericsEnabled[bufdata->vsinConfig.columns.size()] = true;
      bufdata->vsinConfig.generics[bufdata->vsinConfig.columns.size()] = a.genericValue;
    }

    bufdata->vsinConfig.columns.push_back(f);
  }

  bufdata->vsinConfig.numRows = 0;
  bufdata->vsinConfig.unclampedNumRows = 0;

  if(draw)
  {
    bufdata->vsinConfig.numRows = draw->numIndices;
    bufdata->vsinConfig.unclampedNumRows = 0;

    // calculate an upper bound on the valid number of rows just in case it's an invalid value (e.g.
    // 0xdeadbeef) and we want to clamp.
    uint32_t numRowsUpperBound = 0;

    if(draw->flags & DrawFlags::Indexed)
    {
      // In an indexed draw we clamp to however many indices are available in the index buffer

      BoundVBuffer ib = ctx.CurPipelineState().GetIBuffer();

      uint32_t bytesAvailable = 0;

      BufferDescription *buf = ctx.GetBuffer(ib.resourceId);
      if(buf)
        bytesAvailable = buf->length - ib.byteOffset - draw->indexOffset * draw->indexByteWidth;

      // drawing more than this many indices will read off the end of the index buffer - which while
      // technically not invalid is certainly not intended, so serves as a good 'upper bound'
      numRowsUpperBound = bytesAvailable / qMax(1U, draw->indexByteWidth);
    }
    else
    {
      // for a non-indexed draw, we take the largest vertex buffer
      rdcarray<BoundVBuffer> VBs = ctx.CurPipelineState().GetVBuffers();

      for(const BoundVBuffer &vb : VBs)
      {
        if(vb.byteStride == 0)
          continue;

        BufferDescription *buf = ctx.GetBuffer(vb.resourceId);
        if(buf)
        {
          numRowsUpperBound =
              qMax(numRowsUpperBound, uint32_t(buf->length - vb.byteOffset) / vb.byteStride);
        }
      }

      // if there are no vertex buffers we can't clamp.
      if(numRowsUpperBound == 0)
        numRowsUpperBound = ~0U;
    }

    // if we have significantly clamped, then set the unclamped number of rows and clamp.
    if(numRowsUpperBound + 100 < bufdata->vsinConfig.numRows)
    {
      bufdata->vsinConfig.unclampedNumRows = bufdata->vsinConfig.numRows;
      bufdata->vsinConfig.numRows = numRowsUpperBound + 100;
    }
  }

  bufdata->vsoutConfig.columns.clear();
  bufdata->gsoutConfig.columns.clear();

  if(draw)
  {
    const ShaderReflection *vs = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Vertex);
    const ShaderReflection *last = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Geometry);
    if(last == NULL)
      last = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Domain);

    ConfigureColumnsForShader(ctx, vs, bufdata->vsoutConfig.columns);
    ConfigureColumnsForShader(ctx, last, bufdata->gsoutConfig.columns);
  }
}

static void RT_FetchMeshData(IReplayController *r, ICaptureContext &ctx, PopulateBufferData *data)
{
  const DrawcallDescription *draw = ctx.CurDrawcall();

  BoundVBuffer ib = ctx.CurPipelineState().GetIBuffer();

  rdcarray<BoundVBuffer> vbs = ctx.CurPipelineState().GetVBuffers();

  bytebuf idata;
  if(ib.resourceId != ResourceId() && draw && (draw->flags & DrawFlags::Indexed))
    idata = r->GetBufferData(ib.resourceId, ib.byteOffset + draw->indexOffset * draw->indexByteWidth,
                             draw->numIndices * draw->indexByteWidth);

  if(data->vsinConfig.indices)
    data->vsinConfig.indices->deref();

  data->vsinConfig.indices = new BufferData();

  if(draw && draw->indexByteWidth != 0 && !idata.isEmpty())
    data->vsinConfig.indices->storage.resize(sizeof(uint32_t) * draw->numIndices);
  else if(draw && (draw->flags & DrawFlags::Indexed))
    data->vsinConfig.indices->storage.resize(sizeof(uint32_t));

  uint32_t *indices = (uint32_t *)data->vsinConfig.indices->data();

  uint32_t maxIndex = 0;
  if(draw)
    maxIndex = qMax(1U, draw->numIndices) - 1;

  if(draw && !idata.isEmpty())
  {
    maxIndex = 0;
    if(draw->indexByteWidth == 1)
    {
      uint8_t primRestart = data->vsinConfig.primRestart & 0xff;

      for(size_t i = 0; i < idata.size() && (uint32_t)i < draw->numIndices; i++)
      {
        if(primRestart && idata[i] == primRestart)
          continue;

        indices[i] = (uint32_t)idata[i];
        maxIndex = qMax(maxIndex, indices[i]);
      }
    }
    else if(draw->indexByteWidth == 2)
    {
      uint16_t primRestart = data->vsinConfig.primRestart & 0xffff;

      uint16_t *src = (uint16_t *)idata.data();
      for(size_t i = 0; i < idata.size() / sizeof(uint16_t) && (uint32_t)i < draw->numIndices; i++)
      {
        if(primRestart && idata[i] == primRestart)
          continue;

        indices[i] = (uint32_t)src[i];
        maxIndex = qMax(maxIndex, indices[i]);
      }
    }
    else if(draw->indexByteWidth == 4)
    {
      uint32_t primRestart = data->vsinConfig.primRestart;

      memcpy(indices, idata.data(), qMin(idata.size(), draw->numIndices * sizeof(uint32_t)));

      for(uint32_t i = 0; i < draw->numIndices; i++)
      {
        if(primRestart && idata[i] == primRestart)
          continue;

        maxIndex = qMax(maxIndex, indices[i]);
      }
    }
  }

  int vbIdx = 0;
  for(BoundVBuffer vb : vbs)
  {
    bool used = false;
    bool pi = false;
    bool pv = false;

    uint32_t maxAttrOffset = 0;

    for(const FormatElement &col : data->vsinConfig.columns)
    {
      if(col.buffer == vbIdx)
      {
        used = true;

        maxAttrOffset = qMax(maxAttrOffset, col.offset);

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
          maxIdx = qMax(maxIdx, maxIdx + (uint32_t)draw->baseVertex);
      }

      if(pi && pv)
        qCritical() << "Buffer used for both instance and vertex rendering!";
    }

    BufferData *buf = new BufferData;
    if(used)
    {
      buf->storage = r->GetBufferData(vb.resourceId, vb.byteOffset + offset * vb.byteStride,
                                      qMax(maxIdx, maxIdx + 1) * vb.byteStride + maxAttrOffset);

      buf->stride = vb.byteStride;
    }
    // ref passes to model
    data->vsinConfig.buffers.push_back(buf);
  }

  data->vsoutConfig.numRows = data->postVS.numIndices;
  data->vsoutConfig.unclampedNumRows = 0;
  data->vsoutConfig.baseVertex = data->postVS.baseVertex;
  data->vsoutConfig.displayBaseVertex = data->vsinConfig.baseVertex;

  if(draw && data->postVS.indexResourceId != ResourceId() && (draw->flags & DrawFlags::Indexed))
    idata = r->GetBufferData(data->postVS.indexResourceId, data->postVS.indexByteOffset,
                             draw->numIndices * data->postVS.indexByteStride);

  indices = NULL;
  if(data->vsoutConfig.indices)
    data->vsoutConfig.indices->deref();
  if(data->vsoutConfig.displayIndices)
    data->vsoutConfig.displayIndices->deref();

  if(data->vsinConfig.indices)
  {
    // display the same index values
    data->vsoutConfig.displayIndices = data->vsinConfig.indices;
    data->vsoutConfig.displayIndices->ref();

    data->vsoutConfig.indices = new BufferData();
    if(draw && draw->indexByteWidth != 0 && !idata.isEmpty())
    {
      data->vsoutConfig.indices->storage.resize(sizeof(uint32_t) * draw->numIndices);
      indices = (uint32_t *)data->vsoutConfig.indices->data();

      if(draw->indexByteWidth == 1)
      {
        for(size_t i = 0; i < idata.size() && (uint32_t)i < draw->numIndices; i++)
          indices[i] = (uint32_t)idata[i];
      }
      else if(draw->indexByteWidth == 2)
      {
        uint16_t *src = (uint16_t *)idata.data();
        for(size_t i = 0; i < idata.size() / sizeof(uint16_t) && (uint32_t)i < draw->numIndices; i++)
          indices[i] = (uint32_t)src[i];
      }
      else if(draw->indexByteWidth == 4)
      {
        memcpy(indices, idata.data(), qMin(idata.size(), draw->numIndices * sizeof(uint32_t)));
      }
    }
  }

  if(data->postVS.vertexResourceId != ResourceId())
  {
    BufferData *postvs = new BufferData;
    postvs->storage =
        r->GetBufferData(data->postVS.vertexResourceId, data->postVS.vertexByteOffset, 0);

    postvs->stride = data->postVS.vertexByteStride;

    // ref passes to model
    data->vsoutConfig.buffers.push_back(postvs);
  }

  data->gsoutConfig.numRows = data->postGS.numIndices;
  data->gsoutConfig.unclampedNumRows = 0;
  data->gsoutConfig.baseVertex = data->postGS.baseVertex;
  data->gsoutConfig.displayBaseVertex = data->vsinConfig.baseVertex;

  indices = NULL;
  data->gsoutConfig.indices = NULL;

  if(data->postGS.vertexResourceId != ResourceId())
  {
    BufferData *postgs = new BufferData;
    postgs->storage =
        r->GetBufferData(data->postGS.vertexResourceId, data->postGS.vertexByteOffset, 0);

    postgs->stride = data->postGS.vertexByteStride;

    // ref passes to model
    data->gsoutConfig.buffers.push_back(postgs);
  }
}

BufferViewer::BufferViewer(ICaptureContext &ctx, bool meshview, QWidget *parent)
    : QFrame(parent), ui(new Ui::BufferViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_ModelVSIn = new BufferItemModel(ui->vsinData, true, meshview, this);
  m_ModelVSOut = new BufferItemModel(ui->vsoutData, false, meshview, this);
  m_ModelGSOut = new BufferItemModel(ui->gsoutData, false, meshview, this);

  m_MeshView = meshview;

  m_Flycam = new FlycamWrapper();
  m_Arcball = new ArcballWrapper();
  m_CurrentCamera = m_Arcball;

  m_Output = NULL;

  memset(&m_Config, 0, sizeof(m_Config));
  m_Config.type = MeshDataStage::VSIn;
  m_Config.wireframeDraw = true;

  ui->outputTabs->setCurrentIndex(0);
  m_CurStage = MeshDataStage::VSIn;

  ui->vsinData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->vsoutData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->gsoutData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  ui->minBoundsLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->maxBoundsLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  ui->rowOffset->setFont(Formatter::PreferredFont());
  ui->instance->setFont(Formatter::PreferredFont());
  ui->viewIndex->setFont(Formatter::PreferredFont());
  ui->camSpeed->setFont(Formatter::PreferredFont());
  ui->fovGuess->setFont(Formatter::PreferredFont());
  ui->aspectGuess->setFont(Formatter::PreferredFont());
  ui->nearGuess->setFont(Formatter::PreferredFont());
  ui->farGuess->setFont(Formatter::PreferredFont());

  if(meshview)
    SetupMeshView();
  else
    SetupRawView();

  m_ExportMenu = new QMenu(this);

  m_ExportCSV = new QAction(tr("Export to &CSV"), this);
  m_ExportCSV->setIcon(Icons::save());
  m_ExportBytes = new QAction(tr("Export to &Bytes"), this);
  m_ExportBytes->setIcon(Icons::save());

  m_ExportMenu->addAction(m_ExportCSV);
  m_ExportMenu->addAction(m_ExportBytes);

  m_DebugVert = new QAction(tr("&Debug this Vertex"), this);
  m_DebugVert->setIcon(Icons::wrench());

  ui->exportDrop->setMenu(m_ExportMenu);

  QObject::connect(m_ExportCSV, &QAction::triggered,
                   [this] { exportData(BufferExport(BufferExport::CSV)); });
  QObject::connect(m_ExportBytes, &QAction::triggered,
                   [this] { exportData(BufferExport(BufferExport::RawBytes)); });
  QObject::connect(m_DebugVert, &QAction::triggered, this, &BufferViewer::debugVertex);

  QObject::connect(ui->exportDrop, &QToolButton::clicked,
                   [this] { exportData(BufferExport(BufferExport::CSV)); });

  ui->vsinData->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->vsoutData->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->gsoutData->setContextMenuPolicy(Qt::CustomContextMenu);

  QMenu *menu = new QMenu(this);

  ui->vsinData->setCustomHeaderSizing(true);
  ui->vsoutData->setCustomHeaderSizing(true);
  ui->gsoutData->setCustomHeaderSizing(true);

  QObject::connect(ui->vsinData, &RDTableView::customContextMenuRequested,
                   [this, menu](const QPoint &pos) { stageRowMenu(MeshDataStage::VSIn, menu, pos); });

  menu = new QMenu(this);

  QObject::connect(
      ui->vsoutData, &RDTableView::customContextMenuRequested,
      [this, menu](const QPoint &pos) { stageRowMenu(MeshDataStage::VSOut, menu, pos); });

  menu = new QMenu(this);

  QObject::connect(
      ui->gsoutData, &RDTableView::customContextMenuRequested,
      [this, menu](const QPoint &pos) { stageRowMenu(MeshDataStage::GSOut, menu, pos); });

  ui->dockarea->setAllowFloatingWindow(false);

  ui->controlType->addItems({tr("Arcball"), tr("WASD")});
  ui->controlType->adjustSize();

  configureDrawRange();

  ui->solidShading->addItems({tr("None"), tr("Solid Colour"), tr("Flat Shaded"), tr("Secondary")});
  ui->solidShading->adjustSize();
  ui->solidShading->setCurrentIndex(0);

  ui->matrixType->addItems({tr("Perspective"), tr("Orthographic")});

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

  m_CurView = ui->vsinData;

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

  {
    QMenu *extensionsMenu = new QMenu(this);

    ui->extensions->setMenu(extensionsMenu);
    ui->extensions->setPopupMode(QToolButton::InstantPopup);

    QObject::connect(extensionsMenu, &QMenu::aboutToShow, [this, extensionsMenu]() {
      extensionsMenu->clear();
      m_Ctx.Extensions().MenuDisplaying(PanelMenu::MeshPreview, extensionsMenu, ui->extensions, {});
    });
  }

  Reset();

  m_Ctx.AddCaptureViewer(this);
}

void BufferViewer::SetupRawView()
{
  ui->formatSpecifier->setVisible(true);
  ui->outputTabs->setVisible(false);
  ui->vsoutData->setVisible(false);
  ui->gsoutData->setVisible(false);

  // hide buttons we don't want in the toolbar
  ui->syncViews->setVisible(false);
  ui->instanceLabel->setVisible(false);
  ui->instance->setVisible(false);
  ui->viewLabel->setVisible(false);
  ui->viewIndex->setVisible(false);

  ui->vsinData->setWindowTitle(tr("Buffer Contents"));
  ui->vsinData->setFrameShape(QFrame::NoFrame);
  ui->dockarea->addToolWindow(ui->vsinData, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(ui->vsinData, ToolWindowManager::HideCloseButton);

  ui->vsinData->setPinnedColumns(1);
  ui->vsinData->setColumnGroupRole(columnGroupRole);

  ui->formatSpecifier->setWindowTitle(tr("Buffer Format"));
  ui->dockarea->addToolWindow(ui->formatSpecifier, ToolWindowManager::AreaReference(
                                                       ToolWindowManager::BottomOf,
                                                       ui->dockarea->areaOf(ui->vsinData), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->formatSpecifier, ToolWindowManager::HideCloseButton);

  QObject::connect(ui->formatSpecifier, &BufferFormatSpecifier::processFormat, this,
                   &BufferViewer::processFormat);

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(3, 3, 3, 3);

  vertical->addWidget(ui->meshToolbar);
  vertical->addWidget(ui->dockarea);
}

void BufferViewer::SetupMeshView()
{
  setWindowTitle(tr("Mesh Viewer"));

  // hide buttons we don't want in the toolbar
  ui->byteRangeLine->setVisible(false);
  ui->byteRangeStartLabel->setVisible(false);
  ui->byteRangeStart->setVisible(false);
  ui->byteRangeLengthLabel->setVisible(false);
  ui->byteRangeLength->setVisible(false);

  ui->resourceDetails->setVisible(false);
  ui->formatSpecifier->setVisible(false);
  ui->cameraControlsGroup->setVisible(false);

  ui->minBoundsLabel->setText(lit("---"));
  ui->maxBoundsLabel->setText(lit("---"));

  ui->outputTabs->setWindowTitle(tr("Preview"));
  ui->dockarea->addToolWindow(ui->outputTabs, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(ui->outputTabs, ToolWindowManager::HideCloseButton);

  ui->vsinData->setWindowTitle(tr("VS Input"));
  ui->vsinData->setFrameShape(QFrame::NoFrame);
  ui->dockarea->addToolWindow(
      ui->vsinData, ToolWindowManager::AreaReference(ToolWindowManager::TopOf,
                                                     ui->dockarea->areaOf(ui->outputTabs), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->vsinData, ToolWindowManager::HideCloseButton);

  ui->vsoutData->setWindowTitle(tr("VS Output"));
  ui->vsoutData->setFrameShape(QFrame::NoFrame);
  ui->dockarea->addToolWindow(
      ui->vsoutData, ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                                      ui->dockarea->areaOf(ui->vsinData), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->vsoutData, ToolWindowManager::HideCloseButton);

  ui->gsoutData->setWindowTitle(tr("GS/DS Output"));
  ui->gsoutData->setFrameShape(QFrame::NoFrame);
  ui->dockarea->addToolWindow(
      ui->gsoutData, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                      ui->dockarea->areaOf(ui->vsoutData), 0.5f));
  ui->dockarea->setToolWindowProperties(ui->gsoutData, ToolWindowManager::HideCloseButton);

  ToolWindowManager::raiseToolWindow(ui->vsoutData);

  m_HeaderMenu = new QMenu(this);

  m_ResetColumnSel = new QAction(tr("Reset Selected Columns"), this);
  m_SelectPosColumn = new QAction(tr("Select as Position"), this);
  m_SelectSecondColumn = new QAction(tr("Select as Secondary"), this);
  m_SelectSecondAlphaColumn = new QAction(tr("Select Alpha as Secondary"), this);

  m_HeaderMenu->addAction(m_ResetColumnSel);
  m_HeaderMenu->addSeparator();
  m_HeaderMenu->addAction(m_SelectPosColumn);
  m_HeaderMenu->addAction(m_SelectSecondColumn);
  m_HeaderMenu->addAction(m_SelectSecondAlphaColumn);

  QObject::connect(m_ResetColumnSel, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setPosColumn(-1);
    model->setSecondaryColumn(-1, m_Config.solidShadeMode == SolidShade::Secondary, false);

    UI_CalculateMeshFormats();
    on_resetCamera_clicked();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(m_SelectPosColumn, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setPosColumn(m_ContextColumn);

    UI_CalculateMeshFormats();
    on_resetCamera_clicked();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(m_SelectSecondColumn, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setSecondaryColumn(m_ContextColumn, m_Config.solidShadeMode == SolidShade::Secondary,
                              false);

    UI_CalculateMeshFormats();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(m_SelectSecondAlphaColumn, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setSecondaryColumn(m_ContextColumn, m_Config.solidShadeMode == SolidShade::Secondary,
                              true);
    UI_CalculateMeshFormats();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });

  ui->vsinData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->vsoutData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->gsoutData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

  ui->vsinData->setPinnedColumns(2);
  ui->vsoutData->setPinnedColumns(2);
  ui->gsoutData->setPinnedColumns(2);

  ui->vsinData->setColumnGroupRole(columnGroupRole);
  ui->vsoutData->setColumnGroupRole(columnGroupRole);
  ui->gsoutData->setColumnGroupRole(columnGroupRole);

  QObject::connect(ui->vsinData->horizontalHeader(), &QHeaderView::customContextMenuRequested,
                   [this](const QPoint &pos) { meshHeaderMenu(MeshDataStage::VSIn, pos); });
  QObject::connect(ui->vsoutData->horizontalHeader(), &QHeaderView::customContextMenuRequested,
                   [this](const QPoint &pos) { meshHeaderMenu(MeshDataStage::VSOut, pos); });
  QObject::connect(ui->gsoutData->horizontalHeader(), &QHeaderView::customContextMenuRequested,
                   [this](const QPoint &pos) { meshHeaderMenu(MeshDataStage::GSOut, pos); });

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(3, 3, 3, 3);

  vertical->addWidget(ui->meshToolbar);
  vertical->addWidget(ui->dockarea);

  QTimer *renderTimer = new QTimer(this);
  QObject::connect(renderTimer, &QTimer::timeout, this, &BufferViewer::render_timer);
  renderTimer->setSingleShot(false);
  renderTimer->setInterval(10);
  renderTimer->start();
}

void BufferViewer::meshHeaderMenu(MeshDataStage stage, const QPoint &pos)
{
  int col = tableForStage(stage)->horizontalHeader()->logicalIndexAt(pos);

  if(col < 2)
    return;

  m_CurView = tableForStage(stage);
  m_ContextColumn = modelForStage(stage)->elementIndexForColumn(col);

  m_SelectSecondAlphaColumn->setEnabled(
      modelForStage(stage)->elementForColumn(col).format.compCount == 4);

  m_HeaderMenu->popup(tableForStage(stage)->horizontalHeader()->mapToGlobal(pos));
}

void BufferViewer::stageRowMenu(MeshDataStage stage, QMenu *menu, const QPoint &pos)
{
  m_CurView = tableForStage(stage);

  menu->clear();

  if(m_MeshView && stage != MeshDataStage::GSOut && m_Ctx.CurPipelineState().IsCaptureD3D11())
  {
    menu->addAction(m_DebugVert);
    menu->addSeparator();
  }

  menu->addAction(m_ExportCSV);
  menu->addAction(m_ExportBytes);

  menu->popup(m_CurView->viewport()->mapToGlobal(pos));

  ContextMenu contextMenu = ContextMenu::MeshPreview_VSInVertex;

  if(stage == MeshDataStage::VSOut)
    contextMenu = ContextMenu::MeshPreview_VSOutVertex;
  else if(stage == MeshDataStage::GSOut)
    contextMenu = ContextMenu::MeshPreview_GSOutVertex;

  QModelIndex idx = m_CurView->selectionModel()->currentIndex();

  ExtensionCallbackData callbackdata = {make_pyarg("stage", (uint32_t)stage)};

  if(idx.isValid())
  {
    uint32_t vertid =
        m_CurView->model()->data(m_CurView->model()->index(idx.row(), 0), Qt::DisplayRole).toUInt();
    uint32_t index =
        m_CurView->model()->data(m_CurView->model()->index(idx.row(), 1), Qt::DisplayRole).toUInt();

    callbackdata.push_back(make_pyarg("vertex", vertid));
    callbackdata.push_back(make_pyarg("index", index));
  }

  m_Ctx.Extensions().MenuDisplaying(contextMenu, menu, callbackdata);
}

BufferViewer::~BufferViewer()
{
  if(m_Output)
  {
    m_Ctx.Replay().BlockInvoke([this](IReplayController *r) { m_Output->Shutdown(); });
  }

  delete m_Arcball;
  delete m_Flycam;

  if(m_MeshView)
    m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void BufferViewer::OnCaptureLoaded()
{
  Reset();

  if(!m_MeshView)
    return;

  WindowingData winData = m_Ctx.CreateWindowingData(ui->render);

  m_Ctx.Replay().BlockInvoke([winData, this](IReplayController *r) {
    m_Output = r->CreateOutput(winData, ReplayOutputType::Mesh);

    ui->render->setOutput(m_Output);

    RT_UpdateAndDisplay(r);
  });
}

void BufferViewer::OnCaptureClosed()
{
  Reset();

  if(!m_MeshView)
    ToolWindowManager::closeToolWindow(this);
}

void BufferViewer::OnEventChanged(uint32_t eventId)
{
  PopulateBufferData *bufdata = new PopulateBufferData;

  bufdata->vsinHoriz = ui->vsinData->horizontalScrollBar()->value();
  bufdata->vsoutHoriz = ui->vsoutData->horizontalScrollBar()->value();
  bufdata->gsoutHoriz = ui->gsoutData->horizontalScrollBar()->value();

  bufdata->vsinVert = ui->vsinData->indexAt(QPoint(0, 0)).row();
  bufdata->vsoutVert = ui->vsoutData->indexAt(QPoint(0, 0)).row();
  bufdata->gsoutVert = ui->gsoutData->indexAt(QPoint(0, 0)).row();

  bufdata->highlightNames[0] = m_ModelVSIn->posName();
  bufdata->highlightNames[1] = m_ModelVSIn->secondaryName();
  bufdata->highlightNames[2] = m_ModelVSOut->posName();
  bufdata->highlightNames[3] = m_ModelVSOut->secondaryName();
  bufdata->highlightNames[4] = m_ModelGSOut->posName();
  bufdata->highlightNames[5] = m_ModelGSOut->secondaryName();

  updateWindowTitle();

  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  configureDrawRange();

  if(m_MeshView)
  {
    ClearModels();

    CalcColumnWidth();

    ClearModels();

    if(m_Ctx.CurPipelineState().IsStripRestartEnabled() && draw &&
       (draw->flags & DrawFlags::Indexed) && IsStrip(draw->topology))
    {
      bufdata->vsinConfig.primRestart = m_Ctx.CurPipelineState().GetStripRestartIndex();

      if(draw->indexByteWidth == 1)
        bufdata->vsinConfig.primRestart &= 0xff;
      else if(draw->indexByteWidth == 2)
        bufdata->vsinConfig.primRestart &= 0xffff;

      bufdata->vsoutConfig.primRestart = bufdata->vsinConfig.primRestart;
      // GS Out doesn't use primitive restart because it is post-expansion
    }

    ConfigureMeshColumns(m_Ctx, bufdata);

    Viewport vp = m_Ctx.CurPipelineState().GetViewport(0);

    float vpWidth = qAbs(vp.width);
    float vpHeight = qAbs(vp.height);

    m_Config.fov = ui->fovGuess->value();
    m_Config.aspect = (vpWidth > 0.0f && vpHeight > 0.0f) ? (vpWidth / vpHeight) : 1.0f;
    m_Config.highlightVert = 0;

    if(ui->aspectGuess->value() > 0.0)
      m_Config.aspect = ui->aspectGuess->value();
  }
  else
  {
    QString errors;
    bufdata->vsinConfig.columns = FormatElement::ParseFormatString(m_Format, 0, true, errors);

    ClearModels();
  }

  EnableCameraGuessControls();

  bufdata->vsinConfig.curInstance = bufdata->vsoutConfig.curInstance =
      bufdata->gsoutConfig.curInstance = m_Config.curInstance;
  bufdata->vsinConfig.curView = bufdata->vsoutConfig.curView = bufdata->gsoutConfig.curView =
      m_Config.curView;

  m_ModelVSIn->beginReset();
  m_ModelVSOut->beginReset();
  m_ModelGSOut->beginReset();

  bufdata->vsinConfig.baseVertex = draw ? draw->baseVertex : 0;

  ui->instance->setEnabled(draw && draw->numInstances > 1);
  if(!ui->instance->isEnabled())
    ui->instance->setValue(0);

  if(draw)
    ui->instance->setMaximum(qMax(0, int(draw->numInstances) - 1));

  uint32_t numViews = m_Ctx.CurPipelineState().MultiviewBroadcastCount();

  if(draw && numViews > 1)
  {
    ui->viewIndex->setEnabled(true);
    ui->viewIndex->setMaximum(qMax(0, int(numViews) - 1));
  }
  else
  {
    ui->viewIndex->setEnabled(false);
    ui->viewIndex->setValue(0);
  }

  m_Ctx.Replay().AsyncInvoke([this, bufdata](IReplayController *r) {

    BufferData *buf = NULL;

    if(m_MeshView)
    {
      bufdata->postVS = r->GetPostVSData(bufdata->vsinConfig.curInstance,
                                         bufdata->vsinConfig.curView, MeshDataStage::VSOut);
      bufdata->postGS = r->GetPostVSData(bufdata->vsinConfig.curInstance,
                                         bufdata->vsinConfig.curView, MeshDataStage::GSOut);

      RT_FetchMeshData(r, m_Ctx, bufdata);
    }
    else
    {
      buf = new BufferData;
      if(m_IsBuffer)
      {
        uint64_t len = m_ByteSize;
        if(len == UINT64_MAX)
          len = 0;

        buf->storage = r->GetBufferData(m_BufferID, m_ByteOffset, len);
      }
      else
      {
        buf->storage = r->GetTextureData(m_BufferID, m_TexArrayIdx, m_TexMip);
      }
    }

    GUIInvoke::call(this, [this, buf, bufdata]() {

      if(buf)
      {
        // calculate tight stride
        buf->stride = 0;
        for(const FormatElement &el : bufdata->vsinConfig.columns)
          buf->stride += el.byteSize();

        buf->stride = qMax((size_t)1, buf->stride);

        uint32_t bufCount = uint32_t(buf->size());

        bufdata->vsinConfig.numRows = uint32_t((bufCount + buf->stride - 1) / buf->stride);
        bufdata->vsinConfig.unclampedNumRows = 0;

        // ownership passes to model
        bufdata->vsinConfig.buffers.push_back(buf);
      }

      m_ModelVSIn->endReset(bufdata->vsinConfig);
      m_ModelVSOut->endReset(bufdata->vsoutConfig);
      m_ModelGSOut->endReset(bufdata->gsoutConfig);

      m_PostVS = bufdata->postVS;
      m_PostGS = bufdata->postGS;

      // if we didn't have a position column selected before, or the name has changed, re-guess
      if(m_ModelVSIn->posColumn() == -1 ||
         bufdata->highlightNames[0] != bufdata->vsinConfig.columnName(m_ModelVSIn->posColumn()))
        m_ModelVSIn->setPosColumn(-1);
      // similarly for secondary columns
      if(m_ModelVSIn->secondaryColumn() == -1 ||
         bufdata->highlightNames[1] != bufdata->vsinConfig.columnName(m_ModelVSIn->secondaryColumn()))
        m_ModelVSIn->setSecondaryColumn(-1, m_Config.solidShadeMode == SolidShade::Secondary, false);

      // and as above for VS Out / GS Out
      if(m_ModelVSOut->posColumn() == -1 ||
         bufdata->highlightNames[2] != bufdata->vsoutConfig.columnName(m_ModelVSOut->posColumn()))
        m_ModelVSOut->setPosColumn(-1);
      if(m_ModelVSOut->secondaryColumn() == -1 ||
         bufdata->highlightNames[3] !=
             bufdata->vsoutConfig.columnName(m_ModelVSOut->secondaryColumn()))
        m_ModelVSOut->setSecondaryColumn(-1, m_Config.solidShadeMode == SolidShade::Secondary, false);

      if(m_ModelGSOut->posColumn() == -1 ||
         bufdata->highlightNames[4] != bufdata->gsoutConfig.columnName(m_ModelGSOut->posColumn()))
        m_ModelGSOut->setPosColumn(-1);
      if(m_ModelGSOut->secondaryColumn() == -1 ||
         bufdata->highlightNames[5] !=
             bufdata->gsoutConfig.columnName(m_ModelGSOut->secondaryColumn()))
        m_ModelGSOut->setSecondaryColumn(-1, m_Config.solidShadeMode == SolidShade::Secondary, false);

      populateBBox(bufdata);

      UI_CalculateMeshFormats();
      UpdateCurrentMeshConfig();

      ApplyRowAndColumnDims(m_ModelVSIn->columnCount(), ui->vsinData);
      ApplyRowAndColumnDims(m_ModelVSOut->columnCount(), ui->vsoutData);
      ApplyRowAndColumnDims(m_ModelGSOut->columnCount(), ui->gsoutData);

      int numRows = qMax(qMax(bufdata->vsinConfig.numRows, bufdata->vsoutConfig.numRows),
                         bufdata->gsoutConfig.numRows);

      ui->rowOffset->setMaximum(qMax(0, numRows - 1));

      ScrollToRow(ui->vsinData, qMin(int(bufdata->vsinConfig.numRows - 1), bufdata->vsinVert));
      ScrollToRow(ui->vsoutData, qMin(int(bufdata->vsoutConfig.numRows - 1), bufdata->vsoutVert));
      ScrollToRow(ui->gsoutData, qMin(int(bufdata->gsoutConfig.numRows - 1), bufdata->gsoutVert));

      ui->vsinData->horizontalScrollBar()->setValue(bufdata->vsinHoriz);
      ui->vsoutData->horizontalScrollBar()->setValue(bufdata->vsoutHoriz);
      ui->gsoutData->horizontalScrollBar()->setValue(bufdata->gsoutHoriz);

      // we're done with it, the buffer configurations are individually copied/refcounted
      delete bufdata;

      INVOKE_MEMFN(RT_UpdateAndDisplay);
    });
  });
}

void BufferViewer::populateBBox(PopulateBufferData *bufdata)
{
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(draw && m_MeshView)
  {
    uint32_t eventId = draw->eventId;
    bool calcNeeded = false;

    {
      QMutexLocker autolock(&m_BBoxLock);
      calcNeeded = !m_BBoxes.contains(eventId);
    }

    if(!calcNeeded)
    {
      UI_ResetArcball();
      return;
    }

    {
      QMutexLocker autolock(&m_BBoxLock);
      m_BBoxes.insert(eventId, BBoxData());
    }

    CalcBoundingBoxData *bbox = new CalcBoundingBoxData;

    bbox->eventId = eventId;

    bbox->input[0] = bufdata->vsinConfig;
    bbox->input[1] = bufdata->vsoutConfig;
    bbox->input[2] = bufdata->vsoutConfig;

    // fire up a thread to calculate the bounding box
    LambdaThread *thread = new LambdaThread([this, bbox] {
      calcBoundingData(*bbox);

      GUIInvoke::call(this, [this, bbox]() { UI_UpdateBoundingBox(*bbox); });
    });
    thread->selfDelete(true);
    thread->start();

    // give the thread a few ms to finish, so we don't get a tiny flicker on small/fast meshes
    thread->wait(10);
  }
}

QVariant BufferViewer::persistData()
{
  QVariantMap state = ui->dockarea->saveState();

  return state;
}

void BufferViewer::setPersistData(const QVariant &persistData)
{
  QVariantMap state = persistData.toMap();

  ui->dockarea->restoreState(state);
}

void BufferViewer::calcBoundingData(CalcBoundingBoxData &bbox)
{
  for(size_t stage = 0; stage < ARRAY_COUNT(bbox.input); stage++)
  {
    const BufferConfiguration &s = bbox.input[stage];

    QList<FloatVector> &minOutputList = bbox.output.bounds[stage].Min;
    QList<FloatVector> &maxOutputList = bbox.output.bounds[stage].Max;

    minOutputList.reserve(s.columns.count());
    maxOutputList.reserve(s.columns.count());

    for(int i = 0; i < s.columns.count(); i++)
    {
      FloatVector maxvec(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);

      if(s.columns[i].format.compCount == 1)
        maxvec.y = maxvec.z = maxvec.w = 0.0;
      else if(s.columns[i].format.compCount == 2)
        maxvec.z = maxvec.w = 0.0;
      else if(s.columns[i].format.compCount == 3)
        maxvec.w = 0.0;

      minOutputList.push_back(maxvec);
      maxOutputList.push_back(FloatVector(-maxvec.x, -maxvec.y, -maxvec.z, -maxvec.w));
    }

    QVector<CachedElData> cache;

    CacheDataForIteration(cache, s.columns, s.buffers, bbox.input[0].curInstance);

    // possible optimisation here if this shows up as a hot spot - sort and unique the indices and
    // iterate in ascending order, to be more cache friendly

    for(uint32_t row = 0; row < s.numRows; row++)
    {
      uint32_t idx = row;

      if(s.indices && s.indices->hasData())
      {
        idx = CalcIndex(s.indices, row, s.baseVertex, s.primRestart);

        if(idx == ~0U || (s.primRestart && idx == s.primRestart))
          continue;
      }

      for(int col = 0; col < s.columns.count(); col++)
      {
        const CachedElData &d = cache[col];
        const FormatElement *el = d.el;

        float *minOut = (float *)&minOutputList[col];
        float *maxOut = (float *)&maxOutputList[col];

        if(d.data)
        {
          const byte *bytes = d.data;

          if(!el->perinstance)
            bytes += d.stride * idx;

          QVariantList list = el->GetVariants(bytes, d.end);

          for(int comp = 0; comp < 4 && comp < list.count(); comp++)
          {
            const QVariant &v = list[comp];

            QMetaType::Type vt = GetVariantMetatype(v);

            float fval = 0.0f;

            if(vt == QMetaType::Double)
              fval = (float)v.toDouble();
            else if(vt == QMetaType::Float)
              fval = v.toFloat();
            else if(vt == QMetaType::UInt || vt == QMetaType::UShort || vt == QMetaType::UChar)
              fval = (float)v.toUInt();
            else if(vt == QMetaType::Int || vt == QMetaType::Short || vt == QMetaType::SChar)
              fval = (float)v.toInt();
            else
              continue;

            if(qIsFinite(fval))
            {
              minOut[comp] = qMin(minOut[comp], fval);
              maxOut[comp] = qMax(maxOut[comp], fval);
            }
          }
        }
      }
    }
  }
}

void BufferViewer::UI_UpdateBoundingBox(const CalcBoundingBoxData &bbox)
{
  {
    QMutexLocker autolock(&m_BBoxLock);
    m_BBoxes[bbox.eventId] = bbox.output;
  }

  if(m_Ctx.CurEvent() == bbox.eventId)
    UpdateCurrentMeshConfig();

  UI_ResetArcball();

  delete &bbox;
}

void BufferViewer::UI_UpdateBoundingBoxLabels(int compCount)
{
  if(compCount == 0)
  {
    BufferItemModel *model = currentBufferModel();
    if(model)
    {
      int posEl = model->posColumn();
      if(posEl >= 0 && posEl < model->getConfig().columns.count())
      {
        compCount = model->getConfig().columns[posEl].format.compCount;
      }
    }
  }

  QString min, max;

  float *minData = &m_Config.minBounds.x;
  float *maxData = &m_Config.maxBounds.x;

  const QString comps = lit("xyzw");

  for(int i = 0; i < compCount && i < 4; i++)
  {
    if(i != 0)
    {
      min += lit("\n");
      max += lit("\n");
    }

    min += tr("Min %1: %2").arg(comps[i]).arg(Formatter::Format(minData[i]));
    max += tr("Max %1: %2").arg(comps[i]).arg(Formatter::Format(maxData[i]));
  }

  if(min.isEmpty())
    ui->minBoundsLabel->setText(lit("---"));
  else
    ui->minBoundsLabel->setText(min);

  if(max.isEmpty())
    ui->maxBoundsLabel->setText(lit("---"));
  else
    ui->maxBoundsLabel->setText(max);
}

void BufferViewer::UI_ResetArcball()
{
  BBoxData bbox;

  {
    QMutexLocker autolock(&m_BBoxLock);
    if(m_BBoxes.contains(m_Ctx.CurEvent()))
      bbox = m_BBoxes[m_Ctx.CurEvent()];
  }

  BufferItemModel *model = currentBufferModel();
  int stage = currentStageIndex();

  if(model)
  {
    int posEl = model->posColumn();
    if(posEl >= 0 && posEl < model->getConfig().columns.count() &&
       posEl < bbox.bounds[stage].Min.count())
    {
      FloatVector diag;
      diag.x = bbox.bounds[stage].Max[posEl].x - bbox.bounds[stage].Min[posEl].x;
      diag.y = bbox.bounds[stage].Max[posEl].y - bbox.bounds[stage].Min[posEl].y;
      diag.z = bbox.bounds[stage].Max[posEl].z - bbox.bounds[stage].Min[posEl].z;

      float len = qSqrt(diag.x * diag.x + diag.y * diag.y + diag.z * diag.z);

      if(diag.x >= 0.0f && diag.y >= 0.0f && diag.z >= 0.0f && len >= 1.0e-6f && len <= 1.0e+10f)
      {
        FloatVector mid;
        mid.x = bbox.bounds[stage].Min[posEl].x + diag.x * 0.5f;
        mid.y = bbox.bounds[stage].Min[posEl].y + diag.y * 0.5f;
        mid.z = bbox.bounds[stage].Min[posEl].z + diag.z * 0.5f;

        m_Arcball->Reset(mid, len * 0.7f);

        GUIInvoke::call(this, [this, len]() { ui->camSpeed->setValue(len / 200.0f); });
      }
    }
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::UI_CalculateMeshFormats()
{
  if(!m_MeshView)
    return;

  rdcarray<BoundVBuffer> vbs = m_Ctx.CurPipelineState().GetVBuffers();
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(draw)
  {
    m_VSInPosition = MeshFormat();
    m_VSInSecondary = MeshFormat();

    const BufferConfiguration &vsinConfig = m_ModelVSIn->getConfig();

    if(!vsinConfig.columns.empty())
    {
      int elIdx = m_ModelVSIn->posColumn();
      if(elIdx < 0 || elIdx >= vsinConfig.columns.count())
        elIdx = 0;

      if(vsinConfig.unclampedNumRows > 0)
        m_VSInPosition.numIndices = vsinConfig.numRows;
      else
        m_VSInPosition.numIndices = draw->numIndices;
      m_VSInPosition.topology = draw->topology;
      m_VSInPosition.indexByteStride = draw->indexByteWidth;
      m_VSInPosition.baseVertex = draw->baseVertex;
      BoundVBuffer ib = m_Ctx.CurPipelineState().GetIBuffer();
      m_VSInPosition.indexResourceId = ib.resourceId;
      m_VSInPosition.indexByteOffset = ib.byteOffset + draw->indexOffset * draw->indexByteWidth;

      if((draw->flags & DrawFlags::Indexed) && m_VSInPosition.indexByteStride == 0)
        m_VSInPosition.indexByteStride = 4U;

      {
        const FormatElement &el = vsinConfig.columns[elIdx];

        m_VSInPosition.instanced = el.perinstance;
        m_VSInPosition.instStepRate = el.instancerate;

        if(el.buffer < vbs.count() && !vsinConfig.genericsEnabled[elIdx])
        {
          m_VSInPosition.vertexResourceId = vbs[el.buffer].resourceId;
          m_VSInPosition.vertexByteStride = vbs[el.buffer].byteStride;
          m_VSInPosition.vertexByteOffset = vbs[el.buffer].byteOffset + el.offset +
                                            draw->vertexOffset * m_VSInPosition.vertexByteStride;
        }
        else
        {
          m_VSInPosition.vertexResourceId = ResourceId();
          m_VSInPosition.vertexByteStride = 0;
          m_VSInPosition.vertexByteOffset = 0;
        }

        m_VSInPosition.format = el.format;
      }

      elIdx = m_ModelVSIn->secondaryColumn();

      if(elIdx >= 0 && elIdx < vsinConfig.columns.count())
      {
        const FormatElement &el = vsinConfig.columns[elIdx];

        m_VSInSecondary.instanced = el.perinstance;
        m_VSInSecondary.instStepRate = el.instancerate;

        if(el.buffer < vbs.count() && !vsinConfig.genericsEnabled[elIdx])
        {
          m_VSInSecondary.vertexResourceId = vbs[el.buffer].resourceId;
          m_VSInSecondary.vertexByteStride = vbs[el.buffer].byteStride;
          m_VSInSecondary.vertexByteOffset = vbs[el.buffer].byteOffset + el.offset +
                                             draw->vertexOffset * m_VSInSecondary.vertexByteStride;
        }
        else
        {
          m_VSInSecondary.vertexResourceId = ResourceId();
          m_VSInSecondary.vertexByteStride = 0;
          m_VSInSecondary.vertexByteOffset = 0;
        }

        m_VSInSecondary.format = el.format;
        m_VSInSecondary.showAlpha = m_ModelVSIn->secondaryAlpha();
      }
    }

    const BufferConfiguration &vsoutConfig = m_ModelVSOut->getConfig();

    m_PostVSPosition = MeshFormat();
    m_PostVSSecondary = MeshFormat();

    if(!vsoutConfig.columns.empty())
    {
      int elIdx = m_ModelVSOut->posColumn();
      if(elIdx < 0 || elIdx >= vsoutConfig.columns.count())
        elIdx = 0;

      m_PostVSPosition = m_PostVS;
      m_PostVSPosition.vertexByteOffset += vsoutConfig.columns[elIdx].offset;
      m_PostVSPosition.unproject = vsoutConfig.columns[elIdx].systemValue == ShaderBuiltin::Position;

      // if geometry/tessellation is enabled, don't unproject VS output data
      if(m_Ctx.CurPipelineState().GetShader(ShaderStage::Tess_Eval) != ResourceId() ||
         m_Ctx.CurPipelineState().GetShader(ShaderStage::Geometry) != ResourceId())
        m_PostVSPosition.unproject = false;

      elIdx = m_ModelVSOut->secondaryColumn();

      if(elIdx >= 0 && elIdx < vsoutConfig.columns.count())
      {
        m_PostVSSecondary = m_PostVS;
        m_PostVSSecondary.vertexByteOffset += vsoutConfig.columns[elIdx].offset;
        m_PostVSSecondary.showAlpha = m_ModelVSOut->secondaryAlpha();
      }
    }

    const BufferConfiguration &gsoutConfig = m_ModelGSOut->getConfig();

    m_PostGSPosition = MeshFormat();
    m_PostGSSecondary = MeshFormat();

    if(!gsoutConfig.columns.empty())
    {
      int elIdx = m_ModelGSOut->posColumn();
      if(elIdx < 0 || elIdx >= gsoutConfig.columns.count())
        elIdx = 0;

      m_PostGSPosition = m_PostGS;
      m_PostGSPosition.vertexByteOffset += gsoutConfig.columns[elIdx].offset;
      m_PostGSPosition.unproject = gsoutConfig.columns[elIdx].systemValue == ShaderBuiltin::Position;

      elIdx = m_ModelGSOut->secondaryColumn();

      if(elIdx >= 0 && elIdx < gsoutConfig.columns.count())
      {
        m_PostGSSecondary = m_PostGS;
        m_PostGSSecondary.vertexByteOffset += gsoutConfig.columns[elIdx].offset;
        m_PostGSSecondary.showAlpha = m_ModelGSOut->secondaryAlpha();
      }
    }

    m_PostGSPosition.indexByteStride = 0;

    if(!(draw->flags & DrawFlags::Indexed))
      m_PostVSPosition.indexByteStride = m_VSInPosition.indexByteStride = 0;
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
}

void BufferViewer::configureDrawRange()
{
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  int curIndex = ui->drawRange->currentIndex();

  bool instanced = true;

  // don't check the flags, check if there are actually multiple instances
  if(m_Ctx.IsCaptureLoaded())
    instanced = draw && draw->numInstances > 1;

  ui->drawRange->blockSignals(true);
  ui->drawRange->clear();
  if(instanced)
    ui->drawRange->addItems(
        {tr("This instance"), tr("Previous instances"), tr("All instances"), tr("Whole pass")});
  else
    ui->drawRange->addItems({tr("This draw"), tr("Previous instances (N/A)"),
                             tr("All instances (N/A)"), tr("Whole pass")});

  // preserve the previously selected index
  ui->drawRange->setCurrentIndex(qMax(0, curIndex));
  ui->drawRange->blockSignals(false);

  ui->drawRange->adjustSize();

  ui->drawRange->setEnabled(m_CurStage != MeshDataStage::VSIn);

  curIndex = ui->drawRange->currentIndex();

  m_Config.showPrevInstances = (curIndex >= 1);
  m_Config.showAllInstances = (curIndex >= 2);
  m_Config.showWholePass = (curIndex >= 3);
}

void BufferViewer::ApplyRowAndColumnDims(int numColumns, RDTableView *view)
{
  int start = 0;

  QList<int> widths;

  // vertex/element
  widths << m_IdxColWidth;

  // mesh view only - index
  if(m_MeshView)
    widths << m_IdxColWidth;

  for(int i = start; i < numColumns; i++)
    widths << m_DataColWidth;

  view->verticalHeader()->setDefaultSectionSize(m_DataRowHeight);

  view->setColumnWidths(widths);
}

void BufferViewer::UpdateCurrentMeshConfig()
{
  BBoxData bbox;

  uint32_t eventId = m_Ctx.CurEvent();

  {
    QMutexLocker autolocker(&m_BBoxLock);
    if(m_BBoxes.contains(eventId))
      bbox = m_BBoxes[eventId];
  }

  m_Config.type = m_CurStage;
  switch(m_CurStage)
  {
    case MeshDataStage::VSIn:
      m_Config.position = m_VSInPosition;
      m_Config.second = m_VSInSecondary;
      break;
    case MeshDataStage::VSOut:
      m_Config.position = m_PostVSPosition;
      m_Config.second = m_PostVSSecondary;
      break;
    case MeshDataStage::GSOut:
      m_Config.position = m_PostGSPosition;
      m_Config.second = m_PostGSSecondary;
      break;
    default: break;
  }

  BufferItemModel *model = currentBufferModel();
  int stage = currentStageIndex();

  m_Config.showBBox = false;

  if(model)
  {
    int posEl = model->posColumn();
    if(posEl >= 0 && posEl < model->getConfig().columns.count() &&
       posEl < bbox.bounds[stage].Min.count())
    {
      m_Config.minBounds = bbox.bounds[stage].Min[posEl];
      m_Config.maxBounds = bbox.bounds[stage].Max[posEl];
      m_Config.showBBox = !isCurrentRasterOut();

      int compCount = model->getConfig().columns[posEl].format.compCount;

      UI_UpdateBoundingBoxLabels(compCount);
    }
  }
}

void BufferViewer::render_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(m_CurrentCamera)
    m_CurrentCamera->MouseMove(e);

  if(e->buttons() & Qt::RightButton)
    render_clicked(e);

  // display if any mouse buttons are held while moving.
  if(e->buttons() != Qt::NoButton)
  {
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  }
}

void BufferViewer::render_clicked(QMouseEvent *e)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  QPoint curpos = e->pos();

  curpos *= ui->render->devicePixelRatioF();

  if((e->buttons() & Qt::RightButton) && m_Output)
  {
    m_Ctx.Replay().AsyncInvoke(lit("PickVertex"), [this, curpos](IReplayController *r) {
      uint32_t instanceSelected = 0;
      uint32_t vertSelected = 0;

      rdctie(vertSelected, instanceSelected) =
          m_Output->PickVertex(m_Ctx.CurEvent(), (uint32_t)curpos.x(), (uint32_t)curpos.y());

      if(vertSelected != ~0U)
      {
        GUIInvoke::call(this, [this, vertSelected, instanceSelected] {
          int row = (int)vertSelected;

          if(instanceSelected != m_Config.curInstance)
            ui->instance->setValue(instanceSelected);

          BufferItemModel *model = currentBufferModel();

          if(model && row >= 0 && row < model->rowCount())
            ScrollToRow(currentTable(), row);

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

void BufferViewer::ScrollToRow(RDTableView *view, int row)
{
  int hs = view->horizontalScrollBar()->value();

  view->scrollTo(view->model()->index(row, 0), QAbstractItemView::PositionAtTop);
  view->clearSelection();
  view->selectRow(row);

  view->horizontalScrollBar()->setValue(hs);
}

void BufferViewer::ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                              const rdcstr &format)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  m_IsBuffer = true;
  m_ByteOffset = byteOffset;
  m_ByteSize = byteSize;
  m_BufferID = id;

  updateWindowTitle();

  BufferDescription *buf = m_Ctx.GetBuffer(id);
  if(buf)
    m_ObjectByteSize = buf->length;

  processFormat(format);
}

void BufferViewer::ViewTexture(uint32_t arrayIdx, uint32_t mip, ResourceId id, const rdcstr &format)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  m_IsBuffer = false;
  m_TexArrayIdx = arrayIdx;
  m_TexMip = mip;
  m_BufferID = id;

  updateWindowTitle();

  TextureDescription *tex = m_Ctx.GetTexture(id);
  if(tex)
    m_ObjectByteSize = tex->byteSize;

  processFormat(format);
}

void BufferViewer::updateWindowTitle()
{
  if(!m_MeshView)
    setWindowTitle(m_Ctx.GetResourceName(m_BufferID) + lit(" - Contents"));
}

void BufferViewer::on_resourceDetails_clicked()
{
  if(!m_Ctx.HasResourceInspector())
    m_Ctx.ShowResourceInspector();

  m_Ctx.GetResourceInspector()->Inspect(m_BufferID);

  ToolWindowManager::raiseToolWindow(m_Ctx.GetResourceInspector()->Widget());
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

void BufferViewer::RT_UpdateAndDisplay(IReplayController *)
{
  if(m_Output)
  {
    m_Config.cam = m_CurrentCamera->camera();
    m_Output->SetMeshDisplay(m_Config);
  }

  GUIInvoke::call(this, [this]() { ui->render->update(); });
}

RDTableView *BufferViewer::tableForStage(MeshDataStage stage)
{
  if(stage == MeshDataStage::VSIn)
    return ui->vsinData;
  else if(stage == MeshDataStage::VSOut)
    return ui->vsoutData;
  else if(stage == MeshDataStage::GSOut)
    return ui->gsoutData;

  return NULL;
}

BufferItemModel *BufferViewer::modelForStage(MeshDataStage stage)
{
  if(stage == MeshDataStage::VSIn)
    return m_ModelVSIn;
  else if(stage == MeshDataStage::VSOut)
    return m_ModelVSOut;
  else if(stage == MeshDataStage::GSOut)
    return m_ModelGSOut;

  return NULL;
}

bool BufferViewer::isCurrentRasterOut()
{
  BufferItemModel *model = currentBufferModel();
  int stage = currentStageIndex();

  // if geometry/tessellation is enabled, only the GS out stage is rasterized output
  if((m_Ctx.CurPipelineState().GetShader(ShaderStage::Tess_Eval) != ResourceId() ||
      m_Ctx.CurPipelineState().GetShader(ShaderStage::Geometry) != ResourceId()) &&
     m_CurStage != MeshDataStage::GSOut)
    return false;

  if(model)
  {
    int posEl = model->posColumn();
    if(posEl >= 0 && posEl < model->getConfig().columns.count())
    {
      return model->getConfig().columns[posEl].systemValue == ShaderBuiltin::Position;
    }
  }

  return false;
}

int BufferViewer::currentStageIndex()
{
  if(m_CurStage == MeshDataStage::VSIn)
    return 0;
  else if(m_CurStage == MeshDataStage::VSOut)
    return 1;
  else if(m_CurStage == MeshDataStage::GSOut)
    return 2;

  return 0;
}

void BufferViewer::Reset()
{
  m_Output = NULL;

  configureDrawRange();

  ClearModels();

  ui->vsinData->setColumnWidths({40, 40});
  ui->vsoutData->setColumnWidths({40, 40});
  ui->gsoutData->setColumnWidths({40, 40});

  m_BBoxes.clear();

  ICaptureContext *ctx = &m_Ctx;

  // while a capture is loaded, pass NULL into the widget
  if(!m_Ctx.IsCaptureLoaded())
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
  updateCheckerboardColours();
}

void BufferViewer::updateCheckerboardColours()
{
  ui->render->setColours(Formatter::DarkCheckerColor(), Formatter::LightCheckerColor());
}

void BufferViewer::ClearModels()
{
  for(BufferItemModel *m : {m_ModelVSIn, m_ModelVSOut, m_ModelGSOut})
  {
    if(!m)
      continue;

    m->beginReset();
    m->endReset(BufferConfiguration());
  }
}

void BufferViewer::CalcColumnWidth(int maxNumRows)
{
  ResourceFormat floatFmt;
  floatFmt.compByteWidth = 4;
  floatFmt.compType = CompType::Float;
  floatFmt.compCount = 1;

  ResourceFormat intFmt;
  intFmt.compByteWidth = 4;
  intFmt.compType = CompType::UInt;
  intFmt.compCount = 1;

  QString headerText = lit("ColumnSizeTest");

  BufferConfiguration bufconfig;

  bufconfig.columns.clear();
  bufconfig.columns.push_back(
      FormatElement(headerText, 0, 0, false, 1, false, maxNumRows, floatFmt, false, false));
  bufconfig.columns.push_back(
      FormatElement(headerText, 0, 4, false, 1, false, 1, floatFmt, false, false));
  bufconfig.columns.push_back(
      FormatElement(headerText, 0, 8, false, 1, false, 1, floatFmt, false, false));
  bufconfig.columns.push_back(
      FormatElement(headerText, 0, 12, false, 1, false, 1, intFmt, true, false));
  bufconfig.columns.push_back(
      FormatElement(headerText, 0, 16, false, 1, false, 1, intFmt, false, false));

  bufconfig.numRows = 2;
  bufconfig.unclampedNumRows = 0;
  bufconfig.baseVertex = 0;

  if(bufconfig.indices)
    bufconfig.indices->deref();

  bufconfig.indices = new BufferData;
  bufconfig.indices->stride = sizeof(uint32_t);
  bufconfig.indices->storage.resize(sizeof(uint32_t) * 2);

  uint32_t *indices = (uint32_t *)bufconfig.indices->data();
  indices[0] = 0;
  indices[1] = 1000000;

  bufconfig.buffers.clear();

  struct TestData
  {
    float f[4];
    uint32_t ui[3];
  };

  BufferData *bufdata = new BufferData;
  bufdata->stride = sizeof(TestData);
  bufdata->storage.resize(sizeof(TestData));
  bufconfig.buffers.push_back(bufdata);

  TestData *test = (TestData *)bufdata->data();

  test->f[0] = 1.0f;
  test->f[1] = 1.2345e-20f;
  test->f[2] = 123456.7890123456789f;
  test->f[3] = -1.0f;

  test->ui[1] = 0x12345678;
  test->ui[2] = 0xffffffff;

  m_ModelVSIn->beginReset();

  m_ModelVSIn->endReset(bufconfig);

  // measure this data so we can use this as column widths
  ui->vsinData->resizeColumnsToContents();

  // index/element column
  m_IdxColWidth = ui->vsinData->columnWidth(0);

  int col = 1;
  if(m_MeshView)
    col = 2;

  m_DataColWidth = 10;
  for(int c = 0; c < 5; c++)
  {
    int colWidth = ui->vsinData->columnWidth(col + c);
    m_DataColWidth = qMax(m_DataColWidth, colWidth);
  }

  ui->vsinData->resizeRowsToContents();

  m_DataRowHeight = ui->vsinData->rowHeight(0);
}

void BufferViewer::data_selected(const QItemSelection &selected, const QItemSelection &deselected)
{
  QObject *sender = QObject::sender();
  RDTableView *view = qobject_cast<RDTableView *>(sender);
  if(view == NULL)
    view = qobject_cast<RDTableView *>(sender->parent());

  if(view == NULL)
    return;

  m_CurView = view;

  if(selected.count() > 0)
  {
    UpdateHighlightVerts();

    SyncViews(view, true, false);

    INVOKE_MEMFN(RT_UpdateAndDisplay);
  }
}

void BufferViewer::data_scrolled(int scrollvalue)
{
  QObject *sender = QObject::sender();
  RDTableView *view = qobject_cast<RDTableView *>(sender);
  while(sender != NULL && view == NULL)
  {
    sender = sender->parent();
    view = qobject_cast<RDTableView *>(sender);
  }

  if(view == NULL)
    return;

  SyncViews(view, false, true);
}

void BufferViewer::camGuess_changed(double value)
{
  m_Config.ortho = (ui->matrixType->currentIndex() == 1);

  m_Config.fov = ui->fovGuess->value();

  m_Config.aspect = 1.0f;

  // take a guess for the aspect ratio, for if the user hasn't overridden it
  Viewport vp = m_Ctx.CurPipelineState().GetViewport(0);
  m_Config.aspect = (vp.width > 0.0f && vp.height > 0.0f) ? (vp.width / vp.height) : 1.0f;

  if(ui->aspectGuess->value() > 0.0)
    m_Config.aspect = ui->aspectGuess->value();

  // use estimates from post vs data (calculated from vertex position data) if the user
  // hasn't overridden the values
  m_Config.position.nearPlane = 0.1f;

  if(m_CurStage == MeshDataStage::VSOut)
    m_Config.position.nearPlane = m_PostVS.nearPlane;
  else if(m_CurStage == MeshDataStage::GSOut)
    m_Config.position.nearPlane = m_PostGS.nearPlane;

  if(ui->nearGuess->value() > 0.0)
    m_Config.position.nearPlane = ui->nearGuess->value();

  m_Config.position.farPlane = 100.0f;

  if(m_CurStage == MeshDataStage::VSOut)
    m_Config.position.farPlane = m_PostVS.farPlane;
  else if(m_CurStage == MeshDataStage::GSOut)
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

  BufferConfiguration bufconfig;

  QList<FormatElement> cols = FormatElement::ParseFormatString(format, 0, true, errors);

  int maxNumRows = 1;

  for(const FormatElement &c : cols)
    maxNumRows = qMax(maxNumRows, (int)c.matrixdim);

  CalcColumnWidth(maxNumRows);

  ClearModels();

  m_Format = format;

  ui->formatSpecifier->setFormat(format);

  uint32_t stride = 0;
  for(const FormatElement &el : cols)
    stride += el.byteSize();

  stride = qMax(1U, stride);

  ui->byteRangeStart->setSingleStep((int)stride);
  ui->byteRangeLength->setSingleStep((int)stride);

  ui->byteRangeStart->setMaximum((int)m_ObjectByteSize);
  ui->byteRangeLength->setMaximum((int)m_ObjectByteSize);

  ui->byteRangeStart->setValue((int)m_ByteOffset);
  ui->byteRangeLength->setValue((int)m_ByteSize);

  ui->formatSpecifier->setErrors(errors);

  OnEventChanged(m_Ctx.CurEvent());
}

void BufferViewer::on_byteRangeStart_valueChanged(int value)
{
  m_ByteOffset = value;

  processFormat(m_Format);
}

void BufferViewer::on_byteRangeLength_valueChanged(int value)
{
  m_ByteSize = value;

  processFormat(m_Format);
}

void BufferViewer::exportData(const BufferExport &params)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(!m_Ctx.CurDrawcall())
    return;

  if(!m_CurView)
    return;

  QString filter;
  if(params.format == BufferExport::CSV)
    filter = tr("CSV Files (*.csv)");
  else if(params.format == BufferExport::RawBytes)
    filter = tr("Binary Files (*.bin)");

  QString filename = RDDialog::getSaveFileName(this, tr("Export buffer to bytes"), QString(),
                                               tr("%1;;All files (*)").arg(filter));

  if(filename.isEmpty())
    return;

  QFile *f = new QFile(filename);

  QIODevice::OpenMode flags = QIODevice::WriteOnly | QFile::Truncate;

  if(params.format == BufferExport::CSV)
    flags |= QIODevice::Text;

  if(!f->open(flags))
  {
    delete f;
    RDDialog::critical(this, tr("Error exporting file"),
                       tr("Couldn't open file '%1' for writing").arg(filename));
    return;
  }

  if(m_MeshView)
  {
    ANALYTIC_SET(Export.MeshOutput, true);
  }
  else
  {
    ANALYTIC_SET(Export.RawBuffer, true);
  }

  BufferItemModel *model = (BufferItemModel *)m_CurView->model();

  LambdaThread *exportThread = new LambdaThread([this, params, model, f]() {
    if(params.format == BufferExport::RawBytes)
    {
      const BufferConfiguration &config = model->getConfig();

      if(!m_MeshView)
      {
        // this is the simplest possible case, we just dump the contents of the first buffer, as
        // it's tightly packed
        f->write((const char *)config.buffers[0]->data(), int(config.buffers[0]->size()));
      }
      else
      {
        // cache column data for the inner loop
        QVector<CachedElData> cache;

        CacheDataForIteration(cache, config.columns, config.buffers, config.curInstance);

        // go row by row, finding the start of the row and dumping out the elements using their
        // offset and sizes
        for(int i = 0; i < model->rowCount(); i++)
        {
          // manually calculate the index so that we get the real offset (not the displayed offset)
          // in the case of vertex output.
          uint32_t idx = i;

          if(config.indices && config.indices->hasData())
          {
            idx = CalcIndex(config.indices, i, config.baseVertex, config.primRestart);

            // completely omit primitive restart indices
            if(config.primRestart && idx == config.primRestart)
              continue;
          }

          for(int col = 0; col < cache.count(); col++)
          {
            const CachedElData &d = cache[col];
            const FormatElement *el = d.el;

            if(d.data)
            {
              const char *bytes = (const char *)d.data;

              if(!el->perinstance)
                bytes += d.stride * idx;

              if(bytes + d.byteSize <= (const char *)d.end)
              {
                f->write(bytes, d.byteSize);
                continue;
              }
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
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(!m_Ctx.CurDrawcall())
    return;

  if(!m_CurView)
    return;

  QModelIndex idx = m_CurView->selectionModel()->currentIndex();

  if(!idx.isValid())
  {
    GUIInvoke::call(this, [this]() {
      RDDialog::critical(this, tr("Error debugging"),
                         tr("Error debugging vertex - make sure a valid vertex is selected"));
    });
    return;
  }

  uint32_t vertid =
      m_CurView->model()->data(m_CurView->model()->index(idx.row(), 0), Qt::DisplayRole).toUInt();
  uint32_t index =
      m_CurView->model()->data(m_CurView->model()->index(idx.row(), 1), Qt::DisplayRole).toUInt();

  bool done = false;
  ShaderDebugTrace *trace = NULL;

  m_Ctx.Replay().AsyncInvoke([this, &done, &trace, vertid, index](IReplayController *r) {
    trace = r->DebugVertex(vertid, m_Config.curInstance, index, m_Ctx.CurDrawcall()->instanceOffset,
                           m_Ctx.CurDrawcall()->vertexOffset);

    if(trace->states.isEmpty())
    {
      r->FreeTrace(trace);
      trace = NULL;
    }

    done = true;

  });

  QString debugContext = tr("Vertex %1").arg(vertid);

  if(m_Ctx.CurDrawcall()->numInstances > 1)
    debugContext += tr(", Instance %1").arg(m_Config.curInstance);

  // wait a short while before displaying the progress dialog (which won't show if we're already
  // done by the time we reach it)
  for(int i = 0; !done && i < 100; i++)
    QThread::msleep(5);

  ShowProgressDialog(this, tr("Debugging %1").arg(debugContext), [&done]() { return done; });

  if(!trace)
  {
    RDDialog::critical(this, tr("Error debugging"),
                       tr("Error debugging vertex - make sure a valid vertex is selected"));
    return;
  }

  const ShaderReflection *shaderDetails =
      m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Vertex);
  const ShaderBindpointMapping &bindMapping =
      m_Ctx.CurPipelineState().GetBindpointMapping(ShaderStage::Vertex);
  ResourceId pipeline = m_Ctx.CurPipelineState().GetGraphicsPipelineObject();

  // viewer takes ownership of the trace
  IShaderViewer *s = m_Ctx.DebugShader(&bindMapping, shaderDetails, pipeline, trace, debugContext);

  m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
}

void BufferViewer::changeEvent(QEvent *event)
{
  if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
  {
    updateCheckerboardColours();
    ui->render->update();
  }
}

void BufferViewer::SyncViews(RDTableView *primary, bool selection, bool scroll)
{
  if(!ui->syncViews->isChecked())
    return;

  RDTableView *views[] = {ui->vsinData, ui->vsoutData, ui->gsoutData};

  int horizScrolls[ARRAY_COUNT(views)] = {0};

  for(size_t i = 0; i < ARRAY_COUNT(views); i++)
    horizScrolls[i] = views[i]->horizontalScrollBar()->value();

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

  for(size_t i = 0; i < ARRAY_COUNT(views); i++)
    views[i]->horizontalScrollBar()->setValue(horizScrolls[i]);
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
    m_CurStage = MeshDataStage::VSIn;
  else if(index == 1)
    m_CurStage = MeshDataStage::VSOut;
  else if(index == 2)
    m_CurStage = MeshDataStage::GSOut;

  configureDrawRange();

  on_resetCamera_clicked();
  ui->autofitCamera->setEnabled(!isCurrentRasterOut());

  EnableCameraGuessControls();

  UpdateCurrentMeshConfig();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_toggleControls_toggled(bool checked)
{
  ui->cameraControlsGroup->setVisible(checked);

  // temporarily set minimum bounds to the longest float we could format, to ensure the minimum size
  // we calculate below is as big as needs to be (sigh...). This is necessary because Qt doesn't
  // properly propagate the minimum size up through the scroll area and instead sizes it down much
  // smaller.
  FloatVector prev = m_Config.minBounds;

  m_Config.minBounds.x = 1.0f;
  m_Config.minBounds.y = 1.2345e-20f;
  m_Config.minBounds.z = 123456.7890123456789f;
  m_Config.minBounds.w = 1.2345e+20f;

  UI_UpdateBoundingBoxLabels(4);

  m_Config.minBounds = prev;

  ui->cameraControlsWidget->setMinimumSize(ui->cameraControlsWidget->minimumSizeHint());
  ui->cameraControlsScroll->setMinimumWidth(ui->cameraControlsWidget->minimumSizeHint().width() +
                                            ui->cameraControlsScroll->verticalScrollBar()->width());

  UI_UpdateBoundingBoxLabels();

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

  m_Config.solidShadeMode = (SolidShade)qMax(0, index);

  m_ModelVSIn->setSecondaryColumn(m_ModelVSIn->secondaryColumn(),
                                  m_Config.solidShadeMode == SolidShade::Secondary,
                                  m_ModelVSIn->secondaryAlpha());
  m_ModelVSOut->setSecondaryColumn(m_ModelVSOut->secondaryColumn(),
                                   m_Config.solidShadeMode == SolidShade::Secondary,
                                   m_ModelVSOut->secondaryAlpha());
  m_ModelGSOut->setSecondaryColumn(m_ModelGSOut->secondaryColumn(),
                                   m_Config.solidShadeMode == SolidShade::Secondary,
                                   m_ModelGSOut->secondaryAlpha());

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_drawRange_currentIndexChanged(int index)
{
  configureDrawRange();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_controlType_currentIndexChanged(int index)
{
  m_Arcball->Reset(FloatVector(), 10.0f);
  m_Flycam->Reset(FloatVector());

  if(index == 0)
  {
    m_CurrentCamera = m_Arcball;

    UI_ResetArcball();
  }
  else
  {
    m_CurrentCamera = m_Flycam;
    if(isCurrentRasterOut())
      m_Flycam->Reset(FloatVector(0.0f, 0.0f, 0.0f, 0.0f));
    else
      m_Flycam->Reset(FloatVector(0.0f, 0.0f, -10.0f, 0.0f));
    on_autofitCamera_clicked();
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

void BufferViewer::on_viewIndex_valueChanged(int value)
{
  m_Config.curView = value;
  OnEventChanged(m_Ctx.CurEvent());
}

void BufferViewer::on_rowOffset_valueChanged(int value)
{
  ScrollToRow(ui->vsinData, value);
  ScrollToRow(ui->vsoutData, value);
  ScrollToRow(ui->gsoutData, value);
}

void BufferViewer::on_autofitCamera_clicked()
{
  if(m_CurStage != MeshDataStage::VSIn)
    return;

  ui->controlType->setCurrentIndex(1);

  BBoxData bbox;

  {
    QMutexLocker autolock(&m_BBoxLock);
    if(m_BBoxes.contains(m_Ctx.CurEvent()))
      bbox = m_BBoxes[m_Ctx.CurEvent()];
  }

  BufferItemModel *model = NULL;
  int stage = 0;

  switch(m_CurStage)
  {
    case MeshDataStage::VSIn:
      model = m_ModelVSIn;
      stage = 0;
      break;
    case MeshDataStage::VSOut:
      model = m_ModelVSOut;
      stage = 1;
      break;
    case MeshDataStage::GSOut:
      model = m_ModelGSOut;
      stage = 2;
      break;
    default: break;
  }

  if(bbox.bounds[stage].Min.isEmpty())
    return;

  if(!model)
    return;

  int posEl = model->posColumn();

  if(posEl < 0 || posEl >= bbox.bounds[stage].Min.count())
    return;

  FloatVector diag;
  diag.x = bbox.bounds[stage].Max[posEl].x - bbox.bounds[stage].Min[posEl].x;
  diag.y = bbox.bounds[stage].Max[posEl].y - bbox.bounds[stage].Min[posEl].y;
  diag.z = bbox.bounds[stage].Max[posEl].z - bbox.bounds[stage].Min[posEl].z;

  float len = qSqrt(diag.x * diag.x + diag.y * diag.y + diag.z * diag.z);

  if(diag.x >= 0.0f && diag.y >= 0.0f && diag.z >= 0.0f && len >= 1.0e-6f && len <= 1.0e+10f)
  {
    FloatVector mid;
    mid.x = bbox.bounds[stage].Min[posEl].x + diag.x * 0.5f;
    mid.y = bbox.bounds[stage].Min[posEl].y + diag.y * 0.5f;
    mid.z = bbox.bounds[stage].Min[posEl].z + diag.z * 0.5f;

    mid.z -= len;

    m_Flycam->Reset(mid);
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}
