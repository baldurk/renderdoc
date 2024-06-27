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

#include "BufferViewer.h"
#include <float.h>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QItemSelection>
#include <QMenu>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QToolTip>
#include <QtMath>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/CollapseGroupBox.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDSplitter.h"
#include "Windows/Dialogs/AxisMappingDialog.h"
#include "ui_BufferViewer.h"

struct FixedVarTag
{
  FixedVarTag() = default;
  FixedVarTag(uint32_t size) : valid(true), padding(true), byteSize(size) {}
  FixedVarTag(rdcstr varName, uint32_t offset)
      : valid(true), padding(false), name(varName), byteOffset(offset)
  {
  }
  bool valid = false;
  bool padding = false;
  bool matrix = false;
  bool rowmajor = false;
  rdcstr name;
  union
  {
    uint32_t byteOffset;
    uint32_t byteSize;
  };
};

Q_DECLARE_METATYPE(FixedVarTag);

static const uint32_t MaxVisibleRows = 10000;

namespace NativeScanCode
{
enum
{
#if defined(Q_OS_WIN32)
  Key_A = 30,
  Key_S = 31,
  Key_D = 32,
  Key_F = 33,
  Key_W = 17,
  Key_R = 19,
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  Key_A = 30 + 8,
  Key_S = 31 + 8,
  Key_D = 32 + 8,
  Key_F = 33 + 8,
  Key_W = 17 + 8,
  Key_R = 19 + 8,
#elif defined(Q_OS_MACOS)
  // scan codes not supported on OS X
  Key_A = 0xDEADBEF1,
  Key_S = 0xDEADBEF2,
  Key_D = 0xDEADBEF3,
  Key_F = 0xDEADBEF4,
  Key_W = 0xDEADBEF5,
  Key_R = 0xDEADBEF6,
#else
#error "Unknown platform! Define NativeScanCode"
#endif
};
};    // namespace NativeScanCode

namespace NativeVirtualKey
{
enum
{
#if defined(Q_OS_WIN32)
  Key_A = quint32('A'),
  Key_S = quint32('S'),
  Key_D = quint32('D'),
  Key_F = quint32('F'),
  Key_W = quint32('W'),
  Key_R = quint32('R'),
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  Key_A = quint32('a'),
  Key_S = quint32('s'),
  Key_D = quint32('d'),
  Key_F = quint32('f'),
  Key_W = quint32('w'),
  Key_R = quint32('r'),
#elif defined(Q_OS_MACOS)
  Key_A = 0x00,
  Key_S = 0x01,
  Key_D = 0x02,
  Key_F = 0x03,
  Key_W = 0x0D,
  Key_R = 0x0F,
#else
#error "Unknown platform! Define NativeVirtualKey"
#endif
};
};    // namespace NativeVirtualKey

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
      m_DragStartPos = e->pos();
    }
    else
    {
      m_DragStartPos = QPoint(-1, -1);
    }
  }

  enum class KeyPressDirection
  {
    None,
    Left,
    Right,
    Forward,
    Back,
    Up,
    Down,
  };

  KeyPressDirection GetDirection(QKeyEvent *e)
  {
    // if we have a native scancode, we expect to be able to match it. If we don't then don't get
    // any false positives by checking the virtual key
    if(e->nativeScanCode() > 1)
    {
      switch(e->nativeScanCode())
      {
        case NativeScanCode::Key_A: return KeyPressDirection::Left;
        case NativeScanCode::Key_D: return KeyPressDirection::Right;
        case NativeScanCode::Key_W: return KeyPressDirection::Forward;
        case NativeScanCode::Key_S: return KeyPressDirection::Back;
        case NativeScanCode::Key_R: return KeyPressDirection::Up;
        case NativeScanCode::Key_F: return KeyPressDirection::Down;
        default: break;
      }
    }
    else
    {
      switch(e->nativeVirtualKey())
      {
        case NativeVirtualKey::Key_A: return KeyPressDirection::Left;
        case NativeVirtualKey::Key_D: return KeyPressDirection::Right;
        case NativeVirtualKey::Key_W: return KeyPressDirection::Forward;
        case NativeVirtualKey::Key_S: return KeyPressDirection::Back;
        case NativeVirtualKey::Key_R: return KeyPressDirection::Up;
        case NativeVirtualKey::Key_F: return KeyPressDirection::Down;
        default: break;
      }
    }

    // handle arrow keys, we can do this safely with Qt::Key
    switch(e->key())
    {
      case Qt::Key_Left: return KeyPressDirection::Left;
      case Qt::Key_Right: return KeyPressDirection::Right;
      case Qt::Key_Up: return KeyPressDirection::Forward;
      case Qt::Key_Down: return KeyPressDirection::Back;
      case Qt::Key_PageUp: return KeyPressDirection::Up;
      case Qt::Key_PageDown: return KeyPressDirection::Down;
      default: break;
    }

    return KeyPressDirection::None;
  }

  virtual void KeyUp(QKeyEvent *e)
  {
    KeyPressDirection dir = GetDirection(e);

    if(dir == KeyPressDirection::Left || dir == KeyPressDirection::Right)
      setMove(Direction::Horiz, 0);
    if(dir == KeyPressDirection::Forward || dir == KeyPressDirection::Back)
      setMove(Direction::Fwd, 0);
    if(dir == KeyPressDirection::Up || dir == KeyPressDirection::Down)
      setMove(Direction::Vert, 0);

    if(e->modifiers() & Qt::ShiftModifier)
      m_CurrentSpeed = 3.0f;
    else
      m_CurrentSpeed = 1.0f;
  }

  virtual void KeyDown(QKeyEvent *e)
  {
    KeyPressDirection dir = GetDirection(e);

    switch(dir)
    {
      case KeyPressDirection::None: break;
      case KeyPressDirection::Left: setMove(Direction::Horiz, -1); break;
      case KeyPressDirection::Right: setMove(Direction::Horiz, 1); break;
      case KeyPressDirection::Forward: setMove(Direction::Fwd, 1); break;
      case KeyPressDirection::Back: setMove(Direction::Fwd, -1); break;
      case KeyPressDirection::Up: setMove(Direction::Vert, 1); break;
      case KeyPressDirection::Down: setMove(Direction::Vert, -1); break;
    }

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
    // this isn't a 'true arcball' but it handles extreme aspect ratios
    // better. We basically 'centre' around the from point always being
    // 0,0 (straight out of the screen) as if you're always dragging
    // the arcball from the middle, and just use the relative movement
    int minDimension = qMin(m_WinSize.width(), m_WinSize.height());

    float ax = 0.0f, ay = 0.0f;
    float bx = ((float)(to.x() - from.x()) / (float)minDimension) * 2.0f;
    float by = ((float)(to.y() - from.y()) / (float)minDimension) * 2.0f;

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

struct BufferElementProperties
{
  ResourceFormat format;
  int buffer = 0;
  ShaderBuiltin systemValue = ShaderBuiltin::Undefined;
  bool perinstance = false;
  bool perprimitive = false;
  bool floatCastWrong = false;
  int instancerate = 1;
};

struct BufferConfiguration
{
  uint32_t curInstance = 0, curView = 0;
  uint32_t numRows = 0, unclampedNumRows = 0;
  uint32_t pagingOffset = 0;

  Packing::Rules packing;
  ShaderConstant fixedVars;
  rdcarray<ShaderVariable> evalVars;
  uint32_t repeatStride = 1;
  uint32_t repeatOffset = 0;

  QString statusString;

  bool noVertices = false;
  bool noInstances = false;

  // we can have two index buffers for VSOut data:
  // the original index buffer is used for the displayed value (in displayIndices), and the actual
  // potentially remapped or permuated index buffer used for fetching data (in indices).
  BufferData *displayIndices = NULL;
  int32_t displayBaseVertex = 0;
  BufferData *indices = NULL;
  int32_t baseVertex = 0;

  rdcfixedarray<uint32_t, 3> dispatchSize;
  rdcarray<TaskGroupSize> taskSizes;
  rdcarray<uint32_t> meshletVertexPrefixCounts;
  uint32_t taskOrMeshletOffset = 0;
  uint64_t perPrimitiveOffset = 0;
  uint32_t perPrimitiveStride = 0;
  Topology topology = Topology::TriangleList;

  rdcarray<ShaderConstant> columns;
  rdcarray<BufferElementProperties> props;

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
    pagingOffset = o.pagingOffset;

    packing = o.packing;
    fixedVars = o.fixedVars;
    evalVars = o.evalVars;
    repeatStride = o.repeatStride;
    repeatOffset = o.repeatOffset;

    statusString = o.statusString;

    noVertices = o.noVertices;
    noInstances = o.noInstances;

    displayIndices = o.displayIndices;
    if(displayIndices)
      displayIndices->ref();
    displayBaseVertex = o.displayBaseVertex;

    indices = o.indices;
    if(indices)
      indices->ref();

    baseVertex = o.baseVertex;
    meshletVertexPrefixCounts = o.meshletVertexPrefixCounts;
    dispatchSize = o.dispatchSize;
    taskSizes = o.taskSizes;
    taskOrMeshletOffset = o.taskOrMeshletOffset;
    perPrimitiveOffset = o.perPrimitiveOffset;
    perPrimitiveStride = o.perPrimitiveStride;
    topology = o.topology;

    columns = o.columns;
    props = o.props;
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

    meshletVertexPrefixCounts.clear();
    dispatchSize = {};
    taskSizes.clear();

    buffers.clear();
    columns.clear();
    props.clear();
    generics.clear();
    genericsEnabled.clear();
    numRows = 0;
    unclampedNumRows = 0;

    statusString.clear();

    noVertices = false;
    noInstances = false;
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
        const BufferElementProperties &prop = props[i];

        if(prop.systemValue == ShaderBuiltin::Position)
        {
          posEl = i;
          break;
        }
      }

      // look for an exact match
      for(int i = 0; posEl == -1 && i < columns.count(); i++)
      {
        const ShaderConstant &el = columns[i];

        if(QString(el.name).compare(lit("POSITION"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("POSITION0"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("POS"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("POS0"), Qt::CaseInsensitive) == 0)
        {
          posEl = i;
          break;
        }
      }

      // try anything containing position
      for(int i = 0; posEl == -1 && i < columns.count(); i++)
      {
        const ShaderConstant &el = columns[i];

        if(QString(el.name).contains(lit("POSITION"), Qt::CaseInsensitive))
        {
          posEl = i;
          break;
        }
      }

      // OK last resort, just look for 'pos'
      for(int i = 0; posEl == -1 && i < columns.count(); i++)
      {
        const ShaderConstant &el = columns[i];

        if(QString(el.name).contains(lit("POS"), Qt::CaseInsensitive))
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
        const ShaderConstant &el = columns[i];

        if(QString(el.name).compare(lit("TEXCOORD"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("TEXCOORD0"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("TEX"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("TEX0"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("UV"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("UV0"), Qt::CaseInsensitive) == 0)
        {
          secondEl = i;
          break;
        }
      }

      for(int i = 0; secondEl == -1 && i < columns.count(); i++)
      {
        const ShaderConstant &el = columns[i];

        if(QString(el.name).compare(lit("COLOR"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("COLOR0"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("COL"), Qt::CaseInsensitive) == 0 ||
           QString(el.name).compare(lit("COL0"), Qt::CaseInsensitive) == 0)
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

static QString interpretVariant(const QVariant &v, const ShaderConstant &el,
                                const BufferElementProperties &prop)
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
    uint32_t u = v.toUInt();

    if(prop.floatCastWrong)
    {
      float f = (float)u;
      memcpy(&u, &f, sizeof(f));
    }

    const bool hexDisplay = bool(el.type.flags & ShaderVariableFlags::HexDisplay);
    const bool binDisplay = bool(el.type.flags & ShaderVariableFlags::BinaryDisplay);

    if(hexDisplay && prop.format.type == ResourceFormatType::Regular)
      ret = Formatter::HexFormat(u, prop.format.compByteWidth);
    else if(binDisplay && prop.format.type == ResourceFormatType::Regular)
      ret = Formatter::BinFormat(u, prop.format.compByteWidth);
    else
      ret = Formatter::Format(u, hexDisplay);
  }
  else if(vt == QMetaType::Int || vt == QMetaType::Short || vt == QMetaType::SChar)
  {
    int32_t i = v.toInt();

    if(prop.floatCastWrong)
    {
      float f = (float)i;
      memcpy(&i, &f, sizeof(f));
    }

    if(i >= 0)
      ret = lit(" ") + Formatter::Format(i);
    else
      ret = Formatter::Format(i);
  }
  else if(vt == QMetaType::ULongLong)
  {
    const bool hexDisplay = bool(el.type.flags & ShaderVariableFlags::HexDisplay);
    const bool binDisplay = bool(el.type.flags & ShaderVariableFlags::BinaryDisplay);

    if(binDisplay)
      ret = Formatter::BinFormat((uint64_t)v.toULongLong(), 8);
    else
      ret = Formatter::Format((uint64_t)v.toULongLong(), hexDisplay);
  }
  else if(vt == QMetaType::LongLong)
  {
    int64_t i = v.toLongLong();
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
  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    int ret = config.numRows;
    if(config.pagingOffset > 0)
      ret++;

    if(ret == 0)
    {
      if(!config.statusString.isEmpty())
        ret += config.statusString.count(QLatin1Char('\n')) + 1;
      if(config.noVertices)
        ret++;
      if(config.noInstances)
        ret++;
    }

    return ret;
  }
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
          const ShaderConstant &el = elementForColumn(section);

          if(el.type.columns == 1 || role == columnGroupRole)
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

      if(config.pagingOffset > 0)
      {
        if(row == 0)
        {
          if(role == Qt::DisplayRole)
            return lit("...");
          return QVariant();
        }
        row--;
      }

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
          const ShaderConstant &el = elementForColumn(col);
          const BufferElementProperties &prop = propForColumn(col);

          if((el.type.flags & ShaderVariableFlags::RGBDisplay) && prop.buffer < config.buffers.size())
          {
            const byte *data = config.buffers[prop.buffer]->data();
            const byte *end = config.buffers[prop.buffer]->end();

            data += config.buffers[prop.buffer]->stride * row;
            data += el.byteOffset;

            // only slightly wasteful, we need to fetch all variants together
            // since some formats are packed and can't be read individually
            QVariantList list = GetVariants(prop.format, el, data, end);

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
              else
              {
                return QVariant();
              }

              if(role == Qt::BackgroundRole)
                return QBrush(rgb);
              else if(role == Qt::ForegroundRole)
                return QBrush(contrastingColor(rgb, QColor::fromRgb(0, 0, 0)));
            }
          }
        }
      }

      if(role == Qt::BackgroundRole && meshView && !config.meshletVertexPrefixCounts.empty())
      {
        auto it = std::upper_bound(config.meshletVertexPrefixCounts.begin(),
                                   config.meshletVertexPrefixCounts.end(), row);

        if(it != config.meshletVertexPrefixCounts.begin())
          it--;

        size_t meshletIdx = it - config.meshletVertexPrefixCounts.begin();

        return meshletIdx % 2 ? view->palette().color(QPalette::AlternateBase)
                              : view->palette().color(QPalette::Base);
      }

      if(role == Qt::DisplayRole)
      {
        if(config.numRows == 0 &&
           (config.noInstances || config.noVertices || !config.statusString.isEmpty()))
        {
          if(col < 2)
            return lit("---");

          if(col != 2)
            return QVariant();

          if(!config.statusString.isEmpty())
          {
            return config.statusString.split(QLatin1Char('\n'))[row];
          }
          else if(config.noVertices && config.noInstances)
          {
            if(row == 0)
              return lit("No Vertices");
            else
              return lit("No Instances");
          }
          else if(config.noVertices)
          {
            return lit("No Vertices");
          }
          else if(config.noInstances)
          {
            return lit("No Instances");
          }
        }

        if(config.unclampedNumRows > config.pagingOffset + config.numRows && row >= config.numRows - 2)
        {
          if(meshView)
          {
            if(col < 2 && row == config.numRows - 1)
              return QString::number(config.unclampedNumRows - 1);
          }
          else
          {
            if(col == 0 && row == config.numRows - 1)
              return QString::number(config.unclampedNumRows - 1);
          }

          return lit("...");
        }

        if(col >= 0 && col < totalColumnCount && row < config.numRows)
        {
          if(col == 0)
          {
            if(meshView && !config.meshletVertexPrefixCounts.empty())
            {
              auto it = std::upper_bound(config.meshletVertexPrefixCounts.begin(),
                                         config.meshletVertexPrefixCounts.end(), row);

              if(it != config.meshletVertexPrefixCounts.begin())
                it--;

              size_t meshletIdx = it - config.meshletVertexPrefixCounts.begin();

              return QFormatStr("%1[%2]")
                  .arg(meshletIdx + config.taskOrMeshletOffset)
                  .arg(row + config.pagingOffset - *it);
            }
            else
            {
              return row + config.pagingOffset;
            }
          }

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

          const ShaderConstant &el = elementForColumn(col);
          const BufferElementProperties &prop = propForColumn(col);

          if(useGenerics(col))
            return interpretGeneric(col, el, prop);

          uint32_t instIdx = 0;
          if(prop.instancerate > 0)
            instIdx = config.curInstance / prop.instancerate;

          if(prop.buffer < config.buffers.size())
          {
            const byte *data = config.buffers[prop.buffer]->data();
            const byte *end = config.buffers[prop.buffer]->end();

            if(prop.perprimitive)
            {
              uint32_t prim = row / RENDERDOC_NumVerticesPerPrimitive(config.topology);
              data += config.perPrimitiveOffset;
              data += config.perPrimitiveStride * prim;
            }
            else if(!prop.perinstance)
            {
              data += config.buffers[prop.buffer]->stride * idx;
            }
            else
            {
              data += config.buffers[prop.buffer]->stride * instIdx;
            }

            data += el.byteOffset;

            // only slightly wasteful, we need to fetch all variants together
            // since some formats are packed and can't be read individually
            QVariantList list = GetVariants(prop.format, el, data, end);

            int comp = componentForIndex(col);

            if(comp < list.count())
            {
              uint32_t rowdim = el.type.rows;
              uint32_t coldim = el.type.columns;

              if(rowdim == 1)
              {
                QVariant v = list[comp];

                if(el.type.pointerTypeID != ~0U)
                {
                  PointerVal ptr;
                  ptr.pointer = v.toULongLong();
                  ptr.pointerTypeID = el.type.pointerTypeID;
                  v = ToQStr(ptr);
                }

                RichResourceTextInitialise(v, getCaptureContext(view));

                if(RichResourceTextCheck(v))
                  return v;

                return interpretVariant(v, el, prop);
              }
              else
              {
                QString ret;

                for(uint32_t r = 0; r < rowdim; r++)
                {
                  if(r > 0)
                    ret += lit("\n");

                  ret += interpretVariant(list[r * coldim + comp], el, prop);
                }

                return ret;
              }
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

  const ShaderConstant &elementForColumn(int col) const
  {
    if(col >= reservedColumnCount())
      col -= reservedColumnCount();
    return config.columns[columnLookup[col]];
  }

  const BufferElementProperties &propForColumn(int col) const
  {
    if(col >= reservedColumnCount())
      col -= reservedColumnCount();
    return config.props[columnLookup[col]];
  }

  bool useGenerics(int col) const
  {
    if(col >= reservedColumnCount())
      col -= reservedColumnCount();
    col = columnLookup[col];
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
  int componentForIndex(int col) const
  {
    if(col >= reservedColumnCount())
      col -= reservedColumnCount();
    return componentLookup[col];
  }
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
      uint32_t columnCount = config.columns[i].type.columns;

      for(uint32_t c = 0; c < columnCount; c++)
      {
        columnLookup.push_back(i);
        componentLookup.push_back((int)c);
      }
    }
  }

  QString outOfBounds() const { return lit("---"); }
  QString interpretGeneric(int col, const ShaderConstant &el, const BufferElementProperties &prop) const
  {
    int comp = componentForIndex(col);

    if(col >= reservedColumnCount())
      col -= reservedColumnCount();
    col = columnLookup[col];

    if(col < config.generics.size())
    {
      if(prop.format.compType == CompType::Float)
      {
        return interpretVariant(QVariant(config.generics[col].floatValue[comp]), el, prop);
      }
      else if(prop.format.compType == CompType::SInt)
      {
        return interpretVariant(QVariant(config.generics[col].intValue[comp]), el, prop);
      }
      else if(prop.format.compType == CompType::UInt)
      {
        return interpretVariant(QVariant(config.generics[col].uintValue[comp]), el, prop);
      }
    }

    return outOfBounds();
  }
};

struct CachedElData
{
  const ShaderConstant *el = NULL;
  const BufferElementProperties *prop = NULL;

  const byte *data = NULL;
  const byte *end = NULL;

  size_t stride;
  int byteSize;
  uint32_t instIdx = 0;

  int numColumns = 0;

  QByteArray nulls;
};

struct PopulateBufferData
{
  int sequence;

  int inHoriz;
  int out1Horiz;
  int out2Horiz;

  int inVert;
  int out1Vert;
  int out2Vert;

  CBufferData cb;

  // {In, Out1, Out2} x {primary, secondary}
  QString highlightNames[6];

  bool meshDispatch = false;

  BufferConfiguration inConfig, out1Config, out2Config;
  MeshFormat postOut1, postOut2;
};

struct CalcBoundingBoxData
{
  uint32_t eventId;

  BufferConfiguration input[3];

  BBoxData output;
};

void CacheDataForIteration(QVector<CachedElData> &cache, const rdcarray<ShaderConstant> &columns,
                           const rdcarray<BufferElementProperties> &props,
                           const QList<BufferData *> buffers, uint32_t inst)
{
  cache.reserve(columns.count());

  for(int col = 0; col < columns.count(); col++)
  {
    const ShaderConstant &el = columns[col];
    const BufferElementProperties &prop = props[col];

    CachedElData d;

    d.el = &el;
    d.prop = &prop;

    d.byteSize = el.type.arrayByteStride;
    d.nulls = QByteArray(d.byteSize, '\0');
    d.numColumns = el.type.columns;

    if(prop.instancerate > 0)
      d.instIdx = inst / prop.instancerate;

    if(prop.buffer < buffers.size())
    {
      d.data = buffers[prop.buffer]->data();
      d.end = buffers[prop.buffer]->end();

      d.stride = buffers[prop.buffer]->stride;

      d.data += el.byteOffset;

      if(prop.perinstance)
        d.data += d.stride * d.instIdx;
    }

    if(prop.perprimitive)
      d.end = d.data;

    cache.push_back(d);
  }
}

static void ConfigureStatusColumn(rdcarray<ShaderConstant> &columns,
                                  rdcarray<BufferElementProperties> &props)
{
  ShaderConstant f;
  f.name = "STATUS";
  f.type.columns = 1;
  f.type.rows = 1;

  BufferElementProperties p;
  p.format.type = ResourceFormatType::Regular;
  p.format.compType = CompType::UInt;
  p.format.compCount = 1;
  p.format.compByteWidth = 4;

  columns.push_back(f);
  props.push_back(p);
}

static void ConfigureColumnsForShader(ICaptureContext &ctx, int32_t streamSelect,
                                      const ShaderReflection *shader,
                                      rdcarray<ShaderConstant> &columns,
                                      rdcarray<BufferElementProperties> &props)
{
  if(!shader)
    return;

  columns.reserve(shader->outputSignature.count());
  props.reserve(shader->outputSignature.count());

  int i = 0, posidx = -1;
  for(const SigParameter &sig : shader->outputSignature)
  {
    if(sig.stream != (uint32_t)streamSelect)
      continue;

    if(sig.systemValue == ShaderBuiltin::OutputIndices)
      continue;

    ShaderConstant f;
    BufferElementProperties p;

    f.name = !sig.varName.isEmpty() ? sig.varName : sig.semanticIdxName;
    if(sig.perPrimitiveRate)
      f.name += lit(" (Per-Prim)");
    f.type.rows = 1;
    f.type.columns = sig.compCount;

    p.buffer = 0;
    p.perinstance = false;
    p.perprimitive = sig.perPrimitiveRate;
    p.instancerate = 1;
    p.systemValue = sig.systemValue;
    p.format.type = ResourceFormatType::Regular;
    p.format.compByteWidth = qMax<uint32_t>(sizeof(float), VarTypeByteSize(sig.varType));
    p.format.compCount = sig.compCount;
    p.format.compType = VarTypeCompType(sig.varType);

    f.type.arrayByteStride = p.format.compByteWidth * p.format.compCount;

    if(sig.systemValue == ShaderBuiltin::Position)
      posidx = i;

    columns.push_back(f);
    props.push_back(p);

    i++;
  }

  // shift position attribute up to first, keeping order otherwise
  // the same
  if(posidx > 0)
  {
    columns.insert(0, columns.takeAt(posidx));
    props.insert(0, props.takeAt(posidx));
  }

  i = 0;
  uint32_t perPrimOffset = 0, perVertOffset = 0;
  for(i = 0; i < columns.count(); i++)
  {
    BufferElementProperties &prop = props[i];
    ShaderConstant &el = columns[i];

    uint numComps = el.type.columns;
    uint elemSize = prop.format.compByteWidth > 4 ? 8U : 4U;

    MeshDataStage outStage = MeshDataStage::VSOut;

    switch(shader->stage)
    {
      case ShaderStage::Vertex: outStage = MeshDataStage::VSOut; break;
      case ShaderStage::Hull: outStage = MeshDataStage::GSOut; break;
      case ShaderStage::Domain: outStage = MeshDataStage::GSOut; break;
      case ShaderStage::Geometry: outStage = MeshDataStage::GSOut; break;
      case ShaderStage::Task: outStage = MeshDataStage::TaskOut; break;
      case ShaderStage::Mesh: outStage = MeshDataStage::MeshOut; break;
      default: break;
    }

    uint32_t &offset = prop.perprimitive ? perPrimOffset : perVertOffset;

    if(ctx.CurPipelineState().HasAlignedPostVSData(outStage))
    {
      if(numComps == 2)
        offset = AlignUp(offset, 2U * elemSize);
      else if(numComps > 2)
        offset = AlignUp(offset, 4U * elemSize);
    }

    el.byteOffset = offset;

    offset += numComps * elemSize;
  }
}

static void ConfigureColumnsForMeshPipe(ICaptureContext &ctx, PopulateBufferData *bufdata)
{
  bufdata->inConfig.statusString = lit("No input visualisation supported for mesh shaders");

  ConfigureStatusColumn(bufdata->inConfig.columns, bufdata->inConfig.props);

  const ShaderReflection *ts = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Task);

  if(ts && bufdata->out1Config.statusString.isEmpty())
  {
    bufdata->out1Config.columns = ts->taskPayload.variables;
    bufdata->out1Config.props.resize(bufdata->out1Config.columns.size());
  }
  else
  {
    if(bufdata->out1Config.statusString.isEmpty())
      bufdata->out1Config.statusString = lit("No output visualisation supported for task shaders");
    ConfigureStatusColumn(bufdata->out1Config.columns, bufdata->out1Config.props);
  }

  const ShaderReflection *ms = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Mesh);

  ConfigureColumnsForShader(ctx, 0, ms, bufdata->out2Config.columns, bufdata->out2Config.props);
}

static void ConfigureColumnsForVertexPipe(ICaptureContext &ctx, PopulateBufferData *bufdata)
{
  const ActionDescription *action = ctx.CurAction();

  bufdata->inConfig.numRows = 0;
  bufdata->inConfig.unclampedNumRows = 0;

  bufdata->inConfig.noVertices = false;
  bufdata->inConfig.noInstances = false;

  rdcarray<VertexInputAttribute> vinputs = ctx.CurPipelineState().GetVertexInputs();

  bufdata->inConfig.columns.reserve(vinputs.count());
  bufdata->inConfig.columns.clear();
  bufdata->inConfig.props.reserve(vinputs.count());
  bufdata->inConfig.props.clear();
  bufdata->inConfig.genericsEnabled.resize(vinputs.count());
  bufdata->inConfig.generics.resize(vinputs.count());

  for(const VertexInputAttribute &a : vinputs)
  {
    if(!a.used)
      continue;

    ShaderConstant f;
    f.name = a.name;
    f.byteOffset = a.byteOffset;
    f.type.columns = a.format.compCount;
    f.type.rows = 1;
    f.type.arrayByteStride = f.type.matrixByteStride = a.format.ElementSize();

    BufferElementProperties p;
    p.buffer = a.vertexBuffer;
    p.perinstance = a.perInstance;
    p.instancerate = a.instanceRate;
    p.floatCastWrong = a.floatCastWrong;
    p.format = a.format;

    bufdata->inConfig.genericsEnabled[bufdata->inConfig.columns.count()] = false;

    if(a.genericEnabled)
    {
      bufdata->inConfig.genericsEnabled[bufdata->inConfig.columns.count()] = true;
      bufdata->inConfig.generics[bufdata->inConfig.columns.count()] = a.genericValue;
    }

    bufdata->inConfig.columns.push_back(f);
    bufdata->inConfig.props.push_back(p);
  }

  bufdata->inConfig.numRows = action->numIndices;
  bufdata->inConfig.unclampedNumRows = 0;

  // calculate an upper bound on the valid number of rows just in case it's an invalid value (e.g.
  // 0xdeadbeef) and we want to clamp.
  uint32_t numRowsUpperBound = 0;

  if(action->flags & ActionFlags::Indexed)
  {
    // In an indexed draw we clamp to however many indices are available in the index buffer

    BoundVBuffer ib = ctx.CurPipelineState().GetIBuffer();

    uint32_t bytesAvailable = ib.byteSize;

    if(bytesAvailable == ~0U)
    {
      BufferDescription *buf = ctx.GetBuffer(ib.resourceId);
      if(buf)
      {
        uint64_t offset = ib.byteOffset + action->indexOffset * ib.byteStride;
        if(offset > buf->length)
          bytesAvailable = 0;
        else
          bytesAvailable = buf->length - offset;
      }
      else
      {
        bytesAvailable = 0;
      }
    }

    // drawing more than this many indices will read off the end of the index buffer - which while
    // technically not invalid is certainly not intended, so serves as a good 'upper bound'
    numRowsUpperBound = bytesAvailable / qMax(1U, ib.byteStride);
  }
  else
  {
    // for a non-indexed draw, we take the largest vertex buffer
    rdcarray<BoundVBuffer> VBs = ctx.CurPipelineState().GetVBuffers();

    for(const BoundVBuffer &vb : VBs)
    {
      if(vb.byteStride == 0)
        continue;

      uint32_t bytesAvailable = vb.byteSize;

      if(bytesAvailable == ~0U)
      {
        BufferDescription *buf = ctx.GetBuffer(vb.resourceId);
        if(buf)
        {
          if(vb.byteOffset > buf->length)
            bytesAvailable = 0;
          else
            bytesAvailable = buf->length - vb.byteOffset;
        }
        else
        {
          bytesAvailable = 0;
        }
      }

      numRowsUpperBound = qMax(numRowsUpperBound, bytesAvailable / qMax(1U, vb.byteStride));
    }

    // if there are no vertex buffers we can't clamp.
    if(numRowsUpperBound == 0)
      numRowsUpperBound = ~0U;
  }

  // if we have significantly clamped, then set the unclamped number of rows and clamp.
  if(numRowsUpperBound != ~0U && numRowsUpperBound + 100 < bufdata->inConfig.numRows)
  {
    bufdata->inConfig.unclampedNumRows = bufdata->inConfig.numRows;
    bufdata->inConfig.numRows = numRowsUpperBound + 100;
  }

  if((action->flags & ActionFlags::Drawcall) && action->numIndices == 0)
    bufdata->inConfig.noVertices = true;

  if((action->flags & ActionFlags::Instanced) && action->numInstances == 0)
  {
    bufdata->inConfig.noInstances = true;
    bufdata->inConfig.numRows = bufdata->inConfig.unclampedNumRows = 0;
  }

  bufdata->out1Config.columns.clear();
  bufdata->out1Config.props.clear();
  bufdata->out2Config.columns.clear();
  bufdata->out2Config.props.clear();

  const ShaderReflection *vs = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Vertex);
  const ShaderReflection *last = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Geometry);
  if(last == NULL)
    last = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Domain);

  ConfigureColumnsForShader(ctx, 0, vs, bufdata->out1Config.columns, bufdata->out1Config.props);
  ConfigureColumnsForShader(ctx, ctx.CurPipelineState().GetRasterizedStream(), last,
                            bufdata->out2Config.columns, bufdata->out2Config.props);
}

static void ConfigureColumns(ICaptureContext &ctx, PopulateBufferData *bufdata)
{
  const ActionDescription *action = ctx.CurAction();

  if(action && (action->flags & ActionFlags::MeshDispatch))
  {
    ConfigureColumnsForMeshPipe(ctx, bufdata);
  }
  else if(action && (action->flags & ActionFlags::Drawcall))
  {
    ConfigureColumnsForVertexPipe(ctx, bufdata);
  }
  else
  {
    IEventBrowser *eb = ctx.GetEventBrowser();

    bufdata->inConfig.statusString = bufdata->out1Config.statusString =
        bufdata->out2Config.statusString =
            lit("No current draw action\nSelected EID @%1 - %2\nEffective EID: @%3 - %4")
                .arg(ctx.CurSelectedEvent())
                .arg(QString(eb->GetEventName(ctx.CurSelectedEvent())))
                .arg(ctx.CurEvent())
                .arg(QString(eb->GetEventName(ctx.CurEvent())));

    ConfigureStatusColumn(bufdata->inConfig.columns, bufdata->inConfig.props);
    ConfigureStatusColumn(bufdata->out1Config.columns, bufdata->out1Config.props);
    ConfigureStatusColumn(bufdata->out2Config.columns, bufdata->out2Config.props);

    bufdata->inConfig.genericsEnabled.push_back(false);
    bufdata->inConfig.generics.push_back(PixelValue());
  }
}

static void RT_FetchMeshPipeData(IReplayController *r, ICaptureContext &ctx, PopulateBufferData *data)
{
  uint32_t numIndices = data->postOut2.numIndices;

  if(data->inConfig.indices)
    data->inConfig.indices->deref();

  data->inConfig.indices = NULL;

  data->out1Config.numRows = data->postOut1.numIndices;
  data->out1Config.unclampedNumRows = 0;

  if(data->out1Config.indices)
    data->out1Config.indices->deref();
  if(data->out1Config.displayIndices)
    data->out1Config.displayIndices->deref();
  data->out1Config.displayIndices = NULL;

  data->out1Config.dispatchSize = data->postOut1.dispatchSize;
  data->out1Config.taskSizes = data->postOut1.taskSizes;

  if(data->postOut1.vertexResourceId != ResourceId())
  {
    BufferData *postts = new BufferData;
    postts->storage =
        r->GetBufferData(data->postOut1.vertexResourceId, data->postOut1.vertexByteOffset, 0);

    postts->stride = data->postOut1.vertexByteStride;

    // ref passes to model
    data->out1Config.buffers.push_back(postts);
  }

  data->out1Config.statusString = data->postOut1.status;

  if(data->out2Config.indices)
    data->out2Config.indices->deref();
  if(data->out2Config.displayIndices)
    data->out2Config.displayIndices->deref();
  data->out2Config.displayIndices = NULL;

  uint32_t count = 0;
  for(const MeshletSize &meshletSize : data->postOut2.meshletSizes)
  {
    data->out2Config.meshletVertexPrefixCounts.push_back(count);
    count += meshletSize.numIndices;
  }

  data->out2Config.numRows = numIndices;
  data->out2Config.unclampedNumRows = 0;

  data->out2Config.topology = data->postOut2.topology;
  data->out2Config.perPrimitiveOffset = data->postOut2.perPrimitiveOffset;
  data->out2Config.perPrimitiveStride = data->postOut2.perPrimitiveStride;

  bytebuf idata = r->GetBufferData(data->postOut2.indexResourceId, data->postOut2.indexByteOffset,
                                   numIndices * data->postOut2.indexByteStride);

  data->out2Config.indices = new BufferData();
  data->out2Config.indices->storage.resize(sizeof(uint32_t) * numIndices);
  uint32_t *indices = (uint32_t *)data->out2Config.indices->data();

  memcpy(indices, idata.data(), qMin(idata.size(), numIndices * sizeof(uint32_t)));

  if(data->postOut2.vertexResourceId != ResourceId())
  {
    BufferData *postms = new BufferData;
    postms->storage =
        r->GetBufferData(data->postOut2.vertexResourceId, data->postOut2.vertexByteOffset, 0);

    postms->stride = data->postOut2.vertexByteStride;

    // ref passes to model
    data->out2Config.buffers.push_back(postms);
  }

  data->out2Config.perPrimitiveOffset = data->postOut2.perPrimitiveOffset;
  data->out2Config.perPrimitiveStride = data->postOut2.perPrimitiveStride;

  data->out2Config.statusString = data->postOut2.status;
}

static void RT_FetchVertexPipeData(IReplayController *r, ICaptureContext &ctx,
                                   PopulateBufferData *data)
{
  const ActionDescription *action = ctx.CurAction();

  BoundVBuffer ib = ctx.CurPipelineState().GetIBuffer();

  rdcarray<BoundVBuffer> vbs = ctx.CurPipelineState().GetVBuffers();

  uint32_t numIndices = action ? action->numIndices : 0;

  bytebuf idata;
  if(ib.resourceId != ResourceId() && action && (action->flags & ActionFlags::Indexed))
  {
    uint64_t readBytes = numIndices * ib.byteStride;
    uint32_t offset = action->indexOffset * ib.byteStride;

    if(ib.byteSize > offset)
      readBytes = qMin(ib.byteSize - offset, readBytes);
    else
      readBytes = 0;

    if(readBytes > 0)
      idata = r->GetBufferData(ib.resourceId, ib.byteOffset + offset, readBytes);
  }

  if(data->inConfig.indices)
    data->inConfig.indices->deref();

  data->inConfig.indices = new BufferData();

  if(action && ib.byteStride != 0 && !idata.isEmpty())
    data->inConfig.indices->storage.resize(
        sizeof(uint32_t) *
        qMin(numIndices, (((uint32_t)idata.size() + ib.byteStride - 1) / ib.byteStride)));
  else if(action && (action->flags & ActionFlags::Indexed))
    data->inConfig.indices->storage.resize(sizeof(uint32_t));

  uint32_t *indices = (uint32_t *)data->inConfig.indices->data();

  uint32_t maxIndex = 0;
  if(action)
    maxIndex = qMax(1U, numIndices) - 1;

  if(action && !idata.isEmpty())
  {
    maxIndex = 0;
    if(ib.byteStride == 1)
    {
      uint8_t primRestart = data->inConfig.primRestart & 0xff;

      for(size_t i = 0; i < idata.size() && (uint32_t)i < numIndices; i++)
      {
        indices[i] = (uint32_t)idata[i];
        if(primRestart && indices[i] == primRestart)
          continue;

        maxIndex = qMax(maxIndex, indices[i]);
      }
    }
    else if(ib.byteStride == 2)
    {
      uint16_t primRestart = data->inConfig.primRestart & 0xffff;

      uint16_t *src = (uint16_t *)idata.data();
      for(size_t i = 0; i < idata.size() / sizeof(uint16_t) && (uint32_t)i < numIndices; i++)
      {
        indices[i] = (uint32_t)src[i];
        if(primRestart && indices[i] == primRestart)
          continue;

        maxIndex = qMax(maxIndex, indices[i]);
      }
    }
    else if(ib.byteStride == 4)
    {
      uint32_t primRestart = data->inConfig.primRestart;

      memcpy(indices, idata.data(), qMin(idata.size(), numIndices * sizeof(uint32_t)));

      for(uint32_t i = 0; i < idata.size() / sizeof(uint32_t) && i < numIndices; i++)
      {
        if(primRestart && indices[i] == primRestart)
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

    for(int c = 0; c < data->inConfig.columns.count(); c++)
    {
      const ShaderConstant &col = data->inConfig.columns[c];
      const BufferElementProperties &prop = data->inConfig.props[c];

      if(prop.buffer == vbIdx)
      {
        used = true;

        maxAttrOffset = qMax(maxAttrOffset, col.byteOffset);

        if(prop.perinstance)
          pi = true;
        else
          pv = true;
      }
    }

    vbIdx++;

    uint32_t maxIdx = 0;
    uint32_t offset = 0;

    if(used && action)
    {
      if(pi)
      {
        maxIdx = qMax(1U, action->numInstances) - 1;
        offset = action->instanceOffset;
      }
      if(pv)
      {
        maxIdx = qMax(maxIndex, maxIdx);
        offset = action->vertexOffset;

        if(action->baseVertex > 0)
          maxIdx = qMax(maxIdx, maxIdx + (uint32_t)action->baseVertex);
      }

      if(pi && pv)
        qCritical() << "Buffer used for both instance and vertex rendering!";
    }

    BufferData *buf = new BufferData;
    if(used)
    {
      uint64_t readBytes = qMax(maxIdx, maxIdx + 1) * vb.byteStride + maxAttrOffset;

      // if the stride is 0, allow reading at most one float4. This will still get clamped by the
      // declared vertex buffer size below
      if(vb.byteStride == 0)
        readBytes += 16;

      offset *= vb.byteStride;

      if(vb.byteSize > offset)
        readBytes = qMin(vb.byteSize - offset, readBytes);
      else
        readBytes = 0;

      if(readBytes > 0)
        buf->storage = r->GetBufferData(vb.resourceId, vb.byteOffset + offset, readBytes);

      buf->stride = vb.byteStride;
    }
    // ref passes to model
    data->inConfig.buffers.push_back(buf);
  }

  if(data->postOut1.numIndices <= data->inConfig.numRows)
  {
    data->out1Config.numRows = data->postOut1.numIndices;
    data->out1Config.unclampedNumRows = 0;
  }
  else
  {
    // the vertex shader can't run any expansion, so apply the same clamping to it as we applied to
    // the inputs. This protects against draws with an invalid number of vertices.
    data->out1Config.numRows = data->inConfig.numRows;
    data->out1Config.unclampedNumRows = data->inConfig.unclampedNumRows;
  }

  data->out1Config.statusString = data->postOut1.status;

  data->out1Config.baseVertex = data->postOut1.baseVertex;
  data->out1Config.displayBaseVertex = data->inConfig.baseVertex;

  if(action && data->postOut1.indexResourceId != ResourceId() &&
     (action->flags & ActionFlags::Indexed))
    idata = r->GetBufferData(data->postOut1.indexResourceId, data->postOut1.indexByteOffset,
                             numIndices * data->postOut1.indexByteStride);

  indices = NULL;
  if(data->out1Config.indices)
    data->out1Config.indices->deref();
  if(data->out1Config.displayIndices)
    data->out1Config.displayIndices->deref();

  {
    // display the same index values
    data->out1Config.displayIndices = data->inConfig.indices;
    data->out1Config.displayIndices->ref();

    data->out1Config.indices = new BufferData();
    if(action && ib.byteStride != 0 && !idata.isEmpty())
    {
      data->out1Config.indices->storage.resize(sizeof(uint32_t) * numIndices);
      indices = (uint32_t *)data->out1Config.indices->data();

      if(ib.byteStride == 1)
      {
        for(size_t i = 0; i < idata.size() && (uint32_t)i < numIndices; i++)
          indices[i] = (uint32_t)idata[i];
      }
      else if(ib.byteStride == 2)
      {
        uint16_t *src = (uint16_t *)idata.data();
        for(size_t i = 0; i < idata.size() / sizeof(uint16_t) && (uint32_t)i < numIndices; i++)
          indices[i] = (uint32_t)src[i];
      }
      else if(ib.byteStride == 4)
      {
        memcpy(indices, idata.data(), qMin(idata.size(), numIndices * sizeof(uint32_t)));
      }
    }
  }

  if(data->postOut1.vertexResourceId != ResourceId())
  {
    BufferData *postvs = new BufferData;
    postvs->storage =
        r->GetBufferData(data->postOut1.vertexResourceId, data->postOut1.vertexByteOffset, 0);

    postvs->stride = data->postOut1.vertexByteStride;

    // ref passes to model
    data->out1Config.buffers.push_back(postvs);
  }

  data->out2Config.statusString = data->postOut2.status;

  data->out2Config.numRows = data->postOut2.numIndices;
  data->out2Config.unclampedNumRows = 0;
  data->out2Config.baseVertex = data->postOut2.baseVertex;
  data->out2Config.displayBaseVertex = data->inConfig.baseVertex;

  indices = NULL;
  data->out2Config.indices = NULL;

  if(data->postOut2.vertexResourceId != ResourceId())
  {
    BufferData *postgs = new BufferData;
    postgs->storage =
        r->GetBufferData(data->postOut2.vertexResourceId, data->postOut2.vertexByteOffset, 0);

    postgs->stride = data->postOut2.vertexByteStride;

    // ref passes to model
    data->out2Config.buffers.push_back(postgs);
  }
}

static int MaxNumRows(const ShaderConstant &c)
{
  int ret = c.type.rows;

  if(c.type.baseType != VarType::Enum)
  {
    for(const ShaderConstant &child : c.type.members)
      ret = qMax(ret, MaxNumRows(child));
  }

  return ret;
}

static void UnrollConstant(rdcstr prefix, uint32_t baseOffset, const ShaderConstant &constant,
                           rdcarray<ShaderConstant> &columns,
                           rdcarray<BufferElementProperties> &props)
{
  bool isArray = constant.type.elements > 1;

  rdcstr baseName = constant.name;

  if(!prefix.isEmpty())
    baseName = prefix + "." + baseName;

  if(constant.type.baseType == VarType::Enum || constant.type.members.isEmpty())
  {
    BufferElementProperties prop;
    prop.format = GetInterpretedResourceFormat(constant);

    ShaderConstant c = constant;
    c.byteOffset += baseOffset;

    if(isArray)
    {
      for(uint32_t a = 0; a < constant.type.elements; a++)
      {
        c.name = QFormatStr("%1[%2]").arg(baseName).arg(a);
        columns.push_back(c);
        props.push_back(prop);
        c.byteOffset += constant.type.arrayByteStride;
      }
    }
    else
    {
      c.name = baseName;
      columns.push_back(c);
      props.push_back(prop);
    }

    return;
  }

  // struct, expand by members
  uint32_t arraySize = qMax(1U, constant.type.elements);
  if(arraySize == ~0U)
    arraySize = 1U;
  for(uint32_t a = 0; a < arraySize; a++)
  {
    for(const ShaderConstant &child : constant.type.members)
    {
      UnrollConstant(isArray ? QFormatStr("%1[%2]").arg(baseName).arg(a) : QString(baseName),
                     baseOffset + constant.byteOffset + a * constant.type.arrayByteStride, child,
                     columns, props);
    }
  }
}

static void UnrollConstant(const ShaderConstant &constant, rdcarray<ShaderConstant> &columns,
                           rdcarray<BufferElementProperties> &props)
{
  UnrollConstant("", 0, constant, columns, props);
}

QList<BufferViewer *> BufferViewer::m_CBufferViews;

BufferViewer::BufferViewer(ICaptureContext &ctx, bool meshview, QWidget *parent)
    : QFrame(parent), ui(new Ui::BufferViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->render->SetContext(m_Ctx);

  byteRangeStart = (RDSpinBox64 *)ui->byteRangeStart;
  byteRangeLength = (RDSpinBox64 *)ui->byteRangeLength;

  byteRangeStart->configure();
  byteRangeLength->configure();

  byteRangeStart->setMinimum(0ULL);
  byteRangeLength->setMinimum(0ULL);

  m_ModelIn = new BufferItemModel(ui->inTable, true, meshview, this);
  m_ModelOut1 = new BufferItemModel(ui->out1Table, false, meshview, this);
  m_ModelOut2 = new BufferItemModel(ui->out2Table, false, meshview, this);

  // we keep the old UI names for serialised layouts compatibility
  QString containerNames[] = {
      lit("vsinData"),
      lit("vsoutData"),
      lit("gsoutData"),
  };

  for(size_t i = 0; i < 3; i++)
  {
    m_Containers[i] = new QWidget(this);
    // for layout compatibility
    m_Containers[i]->setObjectName(containerNames[i]);

    QVBoxLayout *layout = new QVBoxLayout(m_Containers[i]);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
  }

  if(meshview)
  {
    m_Containers[0]->layout()->addWidget(ui->inTable);
    m_Containers[0]->layout()->addWidget(ui->fixedVars);
    m_Containers[1]->layout()->addWidget(ui->out1Table);
    m_Containers[2]->layout()->addWidget(ui->out2Table);

    ui->fixedVars->setVisible(false);
  }

  m_MeshView = meshview;

  ui->formatSpecifier->setContext(&m_Ctx);

  m_Flycam = new FlycamWrapper();
  m_Arcball = new ArcballWrapper();
  m_CurrentCamera = m_Arcball;

  m_Output = NULL;

  memset(&m_Config, 0, sizeof(m_Config));
  m_Config.type = MeshDataStage::VSIn;
  m_Config.wireframeDraw = true;
  m_Config.exploderScale = 1.0f;

  ui->outputTabs->setCurrentIndex(0);
  m_CurStage = MeshDataStage::VSIn;

  ui->inTable->setFont(Formatter::FixedFont());
  ui->out1Table->setFont(Formatter::FixedFont());
  ui->out2Table->setFont(Formatter::FixedFont());

  ui->minBoundsLabel->setFont(Formatter::FixedFont());
  ui->maxBoundsLabel->setFont(Formatter::FixedFont());

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
  m_ExportCSV = new QAction(this);
  m_ExportCSV->setIcon(Icons::save());
  m_ExportBytes = new QAction(this);
  m_ExportBytes->setIcon(Icons::save());

  m_ExportMenu->addAction(m_ExportCSV);
  m_ExportMenu->addAction(m_ExportBytes);

  m_DebugVert = new QAction(tr("&Debug this Vertex"), this);
  m_DebugVert->setIcon(Icons::wrench());

  m_FilterMesh = new QAction(tr("&Filter to this Meshlet"), this);
  m_FilterMesh->setIcon(Icons::filter());

  m_RemoveFilter = new QAction(tr("&Remove Filter"), this);
  m_RemoveFilter->setIcon(Icons::arrow_undo());

  m_GotoTask = new QAction(tr("&Go to task"), this);
  m_GotoTask->setIcon(Icons::arrow_join());

  ui->exportDrop->setMenu(m_ExportMenu);

  QObject::connect(m_ExportMenu, &QMenu::aboutToShow, this, &BufferViewer::updateExportActionNames);

  QObject::connect(m_ExportCSV, &QAction::triggered,
                   [this] { exportData(BufferExport(BufferExport::CSV)); });
  QObject::connect(m_ExportBytes, &QAction::triggered,
                   [this] { exportData(BufferExport(BufferExport::RawBytes)); });
  QObject::connect(m_DebugVert, &QAction::triggered, this, &BufferViewer::debugVertex);
  QObject::connect(m_RemoveFilter, &QAction::triggered,
                   [this]() { SetMeshFilter(MeshFilter::None); });
  QObject::connect(m_FilterMesh, &QAction::triggered, [this]() {
    QModelIndex idx = m_CurView->selectionModel()->currentIndex();

    if(!idx.isValid())
      return;

    uint32_t taskIndex = 0, meshletIndex = 0;
    GetIndicesForMeshRow((uint32_t)idx.row(), taskIndex, meshletIndex);

    SetMeshFilter(MeshFilter::Mesh, taskIndex, meshletIndex);
  });
  QObject::connect(m_GotoTask, &QAction::triggered, [this]() {
    // if there's a filter then by definition only one task is visible, just scroll to it
    if(m_CurMeshFilter != MeshFilter::None)
    {
      ShowMeshData(MeshDataStage::TaskOut);
      ScrollToRow(0, MeshDataStage::TaskOut);
      return;
    }

    QModelIndex idx = m_CurView->selectionModel()->currentIndex();

    if(!idx.isValid())
      return;

    uint32_t taskIndex = 0, meshletIndex = 0;
    GetIndicesForMeshRow((uint32_t)idx.row(), taskIndex, meshletIndex);

    ShowMeshData(MeshDataStage::TaskOut);
    ScrollToRow((int)taskIndex, MeshDataStage::TaskOut);
  });

  QObject::connect(ui->exportDrop, &QToolButton::clicked,
                   [this] { exportData(BufferExport(BufferExport::CSV)); });

  ui->inTable->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->out1Table->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->out2Table->setContextMenuPolicy(Qt::CustomContextMenu);

  ui->fixedVars->setContextMenuPolicy(Qt::CustomContextMenu);

  ui->fixedVars->setFrameShape(QFrame::NoFrame);

  QMenu *menu = new QMenu(this);

  ui->inTable->setCustomHeaderSizing(true);
  ui->out1Table->setCustomHeaderSizing(true);
  ui->out2Table->setCustomHeaderSizing(true);

  ui->inTable->setAllowKeyboardSearches(false);
  ui->out1Table->setAllowKeyboardSearches(false);
  ui->out2Table->setAllowKeyboardSearches(false);

  QObject::connect(ui->fixedVars, &RDTreeWidget::customContextMenuRequested, this,
                   &BufferViewer::fixedVars_contextMenu);

  QObject::connect(ui->inTable, &RDTableView::customContextMenuRequested,
                   [this, menu](const QPoint &pos) { stageRowMenu(MeshDataStage::VSIn, menu, pos); });

  menu = new QMenu(this);

  QObject::connect(
      ui->out1Table, &RDTableView::customContextMenuRequested,
      [this, menu](const QPoint &pos) { stageRowMenu(MeshDataStage::VSOut, menu, pos); });

  menu = new QMenu(this);

  QObject::connect(
      ui->out2Table, &RDTableView::customContextMenuRequested,
      [this, menu](const QPoint &pos) { stageRowMenu(MeshDataStage::GSOut, menu, pos); });

  ui->dockarea->setAllowFloatingWindow(false);

  ui->controlType->addItems({tr("Arcball"), tr("Flycam")});
  ui->controlType->adjustSize();

  configureDrawRange();

  ui->visualisation->clear();
  ui->visualisation->addItems(
      {tr("None"), tr("Solid Colour"), tr("Flat Shaded"), tr("Secondary"), tr("Exploded")});
  ui->visualisation->adjustSize();
  ui->visualisation->setCurrentIndex(0);

  ui->matrixType->addItems({tr("Perspective"), tr("Orthographic")});

  ui->axisMappingCombo->addItems({tr("Y-up, left handed"), tr("Y-up, right handed"),
                                  tr("Z-up, left handed"), tr("Z-up, right handed"), tr("Custom...")});
  ui->axisMappingCombo->setCurrentIndex(0);

  // wireframe only available on solid shaded options
  ui->wireframeRender->setEnabled(false);

  ui->setFormat->setVisible(false);

  ui->fovGuess->setValue(90.0);

  on_controlType_currentIndexChanged(0);

  QObject::connect(ui->inTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->out1Table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->out2Table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);

  m_CurView = ui->inTable;
  m_CurFixed = false;

  QObject::connect(ui->inTable, &RDTableView::clicked, [this]() {
    m_CurView = ui->inTable;
    m_CurFixed = false;
  });
  QObject::connect(ui->out1Table, &RDTableView::clicked, [this]() { m_CurView = ui->out1Table; });
  QObject::connect(ui->out2Table, &RDTableView::clicked, [this]() { m_CurView = ui->out2Table; });

  QObject::connect(ui->fixedVars, &RDTreeWidget::clicked, [this]() {
    m_CurView = NULL;
    m_CurFixed = true;
  });

  QObject::connect(ui->inTable->verticalScrollBar(), &QScrollBar::valueChanged, this,
                   &BufferViewer::data_scrolled);
  QObject::connect(ui->out1Table->verticalScrollBar(), &QScrollBar::valueChanged, this,
                   &BufferViewer::data_scrolled);
  QObject::connect(ui->out2Table->verticalScrollBar(), &QScrollBar::valueChanged, this,
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
      m_Ctx.Extensions().MenuDisplaying(m_MeshView ? PanelMenu::MeshPreview : PanelMenu::BufferViewer,
                                        extensionsMenu, ui->extensions, {});
    });
  }

  QObject::connect(ui->render, &CustomPaintWidget::mouseMove, this, &BufferViewer::render_mouseMove);
  QObject::connect(ui->render, &CustomPaintWidget::clicked, this, &BufferViewer::render_clicked);
  QObject::connect(ui->render, &CustomPaintWidget::keyPress, this, &BufferViewer::render_keyPress);
  QObject::connect(ui->render, &CustomPaintWidget::keyRelease, this,
                   &BufferViewer::render_keyRelease);
  QObject::connect(ui->render, &CustomPaintWidget::mouseWheel, this,
                   &BufferViewer::render_mouseWheel);

  // event filter to pick up tooltip events
  ui->fixedVars->setTooltipElidedItems(false);
  ui->fixedVars->installEventFilter(this);

  Reset();

  m_Ctx.AddCaptureViewer(this);
}

void BufferViewer::GetIndicesForMeshRow(uint32_t row, uint32_t &taskIndex, uint32_t &meshletIdx)
{
  const BufferConfiguration &config2 = m_ModelOut2->getConfig();

  auto it = std::upper_bound(config2.meshletVertexPrefixCounts.begin(),
                             config2.meshletVertexPrefixCounts.end(), row);

  if(it != config2.meshletVertexPrefixCounts.begin())
    it--;

  meshletIdx = uint32_t(it - config2.meshletVertexPrefixCounts.begin());

  const BufferConfiguration &config1 = m_ModelOut1->getConfig();

  taskIndex = 0;

  uint32_t meshletCounter = 0;
  for(taskIndex = 0; taskIndex < meshletIdx && taskIndex < config1.taskSizes.size(); taskIndex++)
  {
    meshletCounter += config1.taskSizes[taskIndex].x * config1.taskSizes[taskIndex].y *
                      config1.taskSizes[taskIndex].z;

    if(meshletIdx < meshletCounter)
      break;
  }

  taskIndex += config1.taskOrMeshletOffset;
  meshletIdx += config2.taskOrMeshletOffset;
}

void BufferViewer::SetupRawView()
{
  ui->formatSpecifier->setVisible(true);
  ui->outputTabs->setVisible(false);
  ui->out1Table->setVisible(false);
  ui->out2Table->setVisible(false);

  m_Containers[0]->setVisible(false);
  m_Containers[1]->setVisible(false);
  m_Containers[2]->setVisible(false);

  // hide buttons we don't want in the toolbar
  ui->syncViews->setVisible(false);
  ui->instanceLabel->setVisible(false);
  ui->instance->setVisible(false);
  ui->viewLabel->setVisible(false);
  ui->viewIndex->setVisible(false);
  ui->dockarea->setVisible(false);

  ui->meshFilterLabel->setVisible(false);
  ui->resetMeshFilterButton->setVisible(false);

  ui->inTable->setFrameShape(QFrame::NoFrame);

  ui->inTable->setPinnedColumns(1);
  ui->inTable->setColumnGroupRole(columnGroupRole);

  m_delegate = new RichTextViewDelegate(ui->inTable);
  ui->inTable->setItemDelegate(m_delegate);

  ui->inTable->viewport()->installEventFilter(this);

  ui->inTable->setMouseTracking(true);

  ui->formatSpecifier->setWindowTitle(tr("Buffer Format"));

  QObject::connect(ui->formatSpecifier, &BufferFormatSpecifier::processFormat,
                   [this](const QString &format) {
                     m_PagingByteOffset = 0;
                     processFormat(format);
                   });

  ui->fixedVars->setColumns({tr("Name"), tr("Value"), tr("Byte Offset"), tr("Type")});
  {
    ui->fixedVars->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->fixedVars->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    ui->fixedVars->header()->setSectionResizeMode(2, QHeaderView::Interactive);
  }

  ui->fixedVars->setFont(Formatter::FixedFont());

  m_FixedGroup = new CollapseGroupBox(this);
  m_RepeatedGroup = new CollapseGroupBox(this);

  m_RepeatedControlBar = new QFrame(this);

  m_RepeatedControlBar->setFrameShape(QFrame::Panel);
  m_RepeatedControlBar->setFrameShadow(QFrame::Raised);

  QHBoxLayout *controlLayout = new QHBoxLayout(m_RepeatedControlBar);
  controlLayout->setSpacing(2);
  controlLayout->setContentsMargins(6, 2, 6, 2);

  m_RepeatedOffset = new RDLabel(this);

  QFrame *line = new QFrame(this);
  line->setFrameShape(QFrame::VLine);
  line->setFrameShadow(QFrame::Sunken);

  controlLayout->addWidget(line);

  controlLayout->addWidget(m_RepeatedOffset);

  controlLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  QVBoxLayout *fixedLayout = new QVBoxLayout(m_FixedGroup);
  fixedLayout->setSpacing(0);
  fixedLayout->setContentsMargins(0, 0, 0, 0);

  QVBoxLayout *repeatedLayout = new QVBoxLayout(m_RepeatedGroup);
  repeatedLayout->setSpacing(3);
  repeatedLayout->setContentsMargins(2, 0, 0, 0);

  repeatedLayout->addWidget(m_RepeatedControlBar);

  m_FixedGroup->setTitle(tr("Fixed SoA data"));
  m_RepeatedGroup->setTitle(tr("Repeated AoS values"));

  m_VLayout = new QVBoxLayout(this);
  m_VLayout->setSpacing(3);
  m_VLayout->setContentsMargins(3, 3, 3, 3);

  m_OuterSplitter = new RDSplitter(Qt::Vertical, this);
  m_OuterSplitter->setHandleWidth(12);
  m_OuterSplitter->setChildrenCollapsible(false);

  m_InnerSplitter = new RDSplitter(Qt::Vertical, this);
  m_InnerSplitter->setHandleWidth(12);
  m_InnerSplitter->setChildrenCollapsible(false);

  m_InnerSplitter->setVisible(false);

  // inner splitter is only used when we have these groups, so we can add these unconditionally
  m_InnerSplitter->addWidget(m_FixedGroup);
  m_InnerSplitter->addWidget(m_RepeatedGroup);

  m_VLayout->addWidget(ui->meshToolbar);
  // 0 will be variable, but set it to something here so QSplitter doesn't barf
  m_OuterSplitter->insertWidget(0, ui->inTable);
  m_OuterSplitter->insertWidget(1, ui->formatSpecifier);
  m_VLayout->addWidget(m_OuterSplitter);
}

void BufferViewer::SetupMeshView()
{
  // hide buttons we don't want in the toolbar
  ui->byteRangeLine->setVisible(false);
  ui->byteRangeStartLabel->setVisible(false);
  byteRangeStart->setVisible(false);
  ui->byteRangeLengthLabel->setVisible(false);
  byteRangeLength->setVisible(false);

  ui->meshFilterLabel->setVisible(false);
  ui->resetMeshFilterButton->setVisible(false);

  ui->fixedVars->setVisible(false);
  ui->showPadding->setVisible(false);

  ui->fixedVars->setColumns({tr("Name"), tr("Value"), tr("Type")});
  {
    ui->fixedVars->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->fixedVars->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  }

  ui->fixedVars->setFont(Formatter::FixedFont());

  ui->resourceDetails->setVisible(false);
  ui->formatSpecifier->setVisible(false);
  ui->cameraControlsGroup->setVisible(false);

  ui->minBoundsLabel->setText(lit("---"));
  ui->maxBoundsLabel->setText(lit("---"));

  ui->outputTabs->setWindowTitle(tr("Preview"));
  ui->dockarea->addToolWindow(ui->outputTabs, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(ui->outputTabs, ToolWindowManager::HideCloseButton);

  ui->inTable->setFrameShape(QFrame::NoFrame);
  ui->dockarea->addToolWindow(
      m_Containers[0], ToolWindowManager::AreaReference(
                           ToolWindowManager::TopOf, ui->dockarea->areaOf(ui->outputTabs), 0.5f));
  ui->dockarea->setToolWindowProperties(m_Containers[0], ToolWindowManager::HideCloseButton);

  ui->out1Table->setFrameShape(QFrame::NoFrame);
  ui->dockarea->addToolWindow(
      m_Containers[1], ToolWindowManager::AreaReference(
                           ToolWindowManager::RightOf, ui->dockarea->areaOf(m_Containers[0]), 0.5f));
  ui->dockarea->setToolWindowProperties(m_Containers[1], ToolWindowManager::HideCloseButton);

  ui->out2Table->setFrameShape(QFrame::NoFrame);
  ui->dockarea->addToolWindow(
      m_Containers[2], ToolWindowManager::AreaReference(
                           ToolWindowManager::AddTo, ui->dockarea->areaOf(m_Containers[1]), 0.5f));
  ui->dockarea->setToolWindowProperties(m_Containers[2], ToolWindowManager::HideCloseButton);

  ToolWindowManager::raiseToolWindow(m_Containers[1]);

  updateLabelsAndLayout();

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
    model->setSecondaryColumn(-1, m_Config.visualisationMode == Visualisation::Secondary, false);

    UI_ConfigureFormats();
    on_resetCamera_clicked();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(m_SelectPosColumn, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setPosColumn(m_ContextColumn);

    UI_ConfigureFormats();
    on_resetCamera_clicked();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(m_SelectSecondColumn, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setSecondaryColumn(m_ContextColumn,
                              m_Config.visualisationMode == Visualisation::Secondary, false);

    UI_ConfigureFormats();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });
  QObject::connect(m_SelectSecondAlphaColumn, &QAction::triggered, [this]() {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    model->setSecondaryColumn(m_ContextColumn,
                              m_Config.visualisationMode == Visualisation::Secondary, true);
    UI_ConfigureFormats();
    UpdateCurrentMeshConfig();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  });

  ui->inTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->out1Table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->out2Table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

  ui->inTable->setPinnedColumns(2);
  ui->out1Table->setPinnedColumns(2);
  ui->out2Table->setPinnedColumns(2);

  ui->inTable->setColumnGroupRole(columnGroupRole);
  ui->out1Table->setColumnGroupRole(columnGroupRole);
  ui->out2Table->setColumnGroupRole(columnGroupRole);

  QObject::connect(ui->inTable->horizontalHeader(), &QHeaderView::customContextMenuRequested,
                   [this](const QPoint &pos) { meshHeaderMenu(MeshDataStage::VSIn, pos); });
  QObject::connect(ui->out1Table->horizontalHeader(), &QHeaderView::customContextMenuRequested,
                   [this](const QPoint &pos) { meshHeaderMenu(MeshDataStage::VSOut, pos); });
  QObject::connect(ui->out2Table->horizontalHeader(), &QHeaderView::customContextMenuRequested,
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
  m_CurFixed = false;
  m_ContextColumn = modelForStage(stage)->elementIndexForColumn(col);

  bool perPrim = modelForStage(stage)->propForColumn(col).perprimitive;

  if(perPrim)
  {
    m_SelectPosColumn->setEnabled(false);
    m_SelectSecondColumn->setEnabled(false);
    m_SelectSecondAlphaColumn->setEnabled(false);
  }
  else
  {
    m_SelectPosColumn->setEnabled(true);
    m_SelectSecondColumn->setEnabled(true);

    m_SelectSecondAlphaColumn->setEnabled(modelForStage(stage)->elementForColumn(col).type.columns ==
                                          4);
  }

  m_HeaderMenu->popup(tableForStage(stage)->horizontalHeader()->mapToGlobal(pos));
}

void BufferViewer::fixedVars_contextMenu(const QPoint &pos)
{
  RDTreeWidgetItem *item = ui->fixedVars->itemAt(pos);

  m_CurView = NULL;
  m_CurFixed = true;

  updateExportActionNames();

  QMenu contextMenu(this);

  QAction expandAll(tr("&Expand All"), this);
  QAction collapseAll(tr("C&ollapse All"), this);
  QAction copy(tr("&Copy"), this);
  QAction showPadding(tr("&Show Padding"), this);
  QAction removeFilter(tr("&Remove Filter"), this);
  QAction filterTask(tr("&Filter to this Task"), this);
  QAction gotoMesh(tr("&Go to meshes"), this);

  expandAll.setIcon(Icons::arrow_out());
  collapseAll.setIcon(Icons::arrow_in());
  copy.setIcon(Icons::copy());
  removeFilter.setIcon(Icons::arrow_undo());
  filterTask.setIcon(Icons::filter());
  gotoMesh.setIcon(Icons::arrow_join());
  showPadding.setCheckable(true);
  showPadding.setChecked(ui->showPadding->isChecked());

  expandAll.setEnabled(item && item->childCount() > 0);
  removeFilter.setEnabled(item && m_CurMeshFilter != MeshFilter::None);
  filterTask.setEnabled(item);
  gotoMesh.setEnabled(item);
  collapseAll.setEnabled(expandAll.isEnabled());

  contextMenu.addAction(&expandAll);
  contextMenu.addAction(&collapseAll);
  contextMenu.addAction(&copy);

  contextMenu.addSeparator();

  int idx = ui->fixedVars->indexOfTopLevelItem(item);

  if(m_MeshView)
  {
    contextMenu.addAction(&removeFilter);
    contextMenu.addAction(&filterTask);
    contextMenu.addAction(&gotoMesh);

    const BufferConfiguration &config1 = m_ModelOut1->getConfig();

    // if we're already filtering to a task, don't offer to filter any more. However if we're
    // filtered to a mesh allow 'broadening' the filter back to the task
    // also don't allow filtering at all if there is no task shader bound
    filterTask.setEnabled(!config1.taskSizes.empty() && m_CurMeshFilter != MeshFilter::TaskGroup);

    if(config1.taskSizes.empty() ||
       config1.taskSizes[idx].x * config1.taskSizes[idx].y * config1.taskSizes[idx].z == 0)
      gotoMesh.setEnabled(false);

    // if there's a filter don't enable goto mesh as normally we just scroll to the first mesh - it
    // would be redundant and potentially annoying to be able to and doesn't do anything useful
    if(m_CurMeshFilter != MeshFilter::None)
      gotoMesh.setEnabled(false);
  }
  else
  {
    contextMenu.addAction(&showPadding);
  }

  contextMenu.addSeparator();

  contextMenu.addAction(m_ExportCSV);
  contextMenu.addAction(m_ExportBytes);

  QObject::connect(&removeFilter, &QAction::triggered, [this]() { SetMeshFilter(MeshFilter::None); });
  QObject::connect(&filterTask, &QAction::triggered, [this, idx]() {
    // if there's no filter, select this task. If we were mesh filtering, filter back to all meshes
    // under the current task (don't use idx there, since it will just be 0)
    if(m_CurMeshFilter == MeshFilter::None)
      SetMeshFilter(MeshFilter::TaskGroup, idx);
    else
      SetMeshFilter(MeshFilter::TaskGroup, m_FilteredTaskGroup);
  });
  QObject::connect(&gotoMesh, &QAction::triggered, [this, idx]() {
    const BufferConfiguration &config1 = m_ModelOut1->getConfig();

    uint32_t meshletIndex = 0;
    for(int i = 0; i < idx && i < config1.taskSizes.count(); i++)
    {
      meshletIndex += config1.taskSizes[i].x * config1.taskSizes[i].y * config1.taskSizes[i].z;
    }

    const BufferConfiguration &config2 = m_ModelOut2->getConfig();
    uint32_t vertexOffset = config2.meshletVertexPrefixCounts[meshletIndex];

    ShowMeshData(MeshDataStage::MeshOut);
    ScrollToRow((int)vertexOffset, MeshDataStage::MeshOut);
  });

  QObject::connect(&expandAll, &QAction::triggered,
                   [this, item]() { ui->fixedVars->expandAllItems(item); });

  QObject::connect(&collapseAll, &QAction::triggered,
                   [this, item]() { ui->fixedVars->collapseAllItems(item); });
  QObject::connect(&copy, &QAction::triggered,
                   [this, item, pos]() { ui->fixedVars->copyItem(pos, item); });
  QObject::connect(&showPadding, &QAction::triggered,
                   [this]() { ui->showPadding->setChecked(!ui->showPadding->isChecked()); });

  RDDialog::show(&contextMenu, ui->fixedVars->viewport()->mapToGlobal(pos));
}

void BufferViewer::stageRowMenu(MeshDataStage stage, QMenu *menu, const QPoint &pos)
{
  m_CurView = tableForStage(stage);
  m_CurFixed = false;

  updateExportActionNames();

  menu->clear();

  menu->setToolTipsVisible(true);

  QModelIndex idx = m_CurView->selectionModel()->currentIndex();

  const ActionDescription *action = m_Ctx.CurAction();

  if(action && (action->flags & ActionFlags::MeshDispatch))
  {
    if(stage == MeshDataStage::GSOut)
    {
      const BufferConfiguration &config = m_ModelOut2->getConfig();

      auto it = std::upper_bound(config.meshletVertexPrefixCounts.begin(),
                                 config.meshletVertexPrefixCounts.end(), (uint32_t)idx.row());

      if(it != config.meshletVertexPrefixCounts.begin())
        it--;

      size_t meshletIdx = it - config.meshletVertexPrefixCounts.begin();

      m_RemoveFilter->setEnabled(m_CurMeshFilter != MeshFilter::None);

      menu->addAction(m_RemoveFilter);
      menu->addAction(m_FilterMesh);
      menu->addAction(m_GotoTask);
      menu->addSeparator();

      m_GotoTask->setEnabled(m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Task));
    }
  }

  if(m_MeshView && stage != MeshDataStage::GSOut)
  {
    const ShaderReflection *shaderDetails =
        m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Vertex);

    m_DebugVert->setEnabled(false);

    if(!m_Ctx.APIProps().shaderDebugging)
    {
      m_DebugVert->setToolTip(tr("This API does not support shader debugging"));
    }
    else if(!m_Ctx.CurAction() ||
            !(m_Ctx.CurAction()->flags & (ActionFlags::Drawcall | ActionFlags::MeshDispatch)))
    {
      m_DebugVert->setToolTip(tr("No draw call selected"));
    }
    else if(!shaderDetails)
    {
      m_DebugVert->setToolTip(tr("No vertex shader bound"));
    }
    else if(!shaderDetails->debugInfo.debuggable)
    {
      m_DebugVert->setToolTip(
          tr("This shader doesn't support debugging: %1").arg(shaderDetails->debugInfo.debugStatus));
    }
    else
    {
      m_DebugVert->setEnabled(true);
      m_DebugVert->setToolTip(QString());
    }

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
  else if(stage == MeshDataStage::TaskOut)
    contextMenu = ContextMenu::MeshPreview_TaskOutVertex;
  else if(stage == MeshDataStage::MeshOut)
    contextMenu = ContextMenu::MeshPreview_MeshOutVertex;

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

  m_CBufferViews.removeOne(this);
}

void BufferViewer::OnCaptureLoaded()
{
  Reset();

  if(!m_MeshView)
    return;

  WindowingData winData = ui->render->GetWidgetWindowingData();

  m_Ctx.Replay().BlockInvoke([winData, this](IReplayController *r) {
    m_Output = r->CreateOutput(winData, ReplayOutputType::Mesh);

    ui->render->SetOutput(m_Output);

    RT_UpdateAndDisplay(r);
  });
}

void BufferViewer::OnCaptureClosed()
{
  Reset();

  if(!m_MeshView)
    ToolWindowManager::closeToolWindow(this);
}

void BufferViewer::FillScrolls(PopulateBufferData *bufdata)
{
  bufdata->inHoriz = ui->inTable->horizontalScrollBar()->value();
  bufdata->out1Horiz = ui->out1Table->horizontalScrollBar()->value();
  bufdata->out2Horiz = ui->out2Table->horizontalScrollBar()->value();

  bufdata->inVert = ui->inTable->indexAt(QPoint(0, 0)).row();
  bufdata->out1Vert = ui->out1Table->indexAt(QPoint(0, 0)).row();
  bufdata->out2Vert = ui->out2Table->indexAt(QPoint(0, 0)).row();

  if(bufdata->meshDispatch)
  {
    bufdata->out1Horiz = ui->fixedVars->horizontalScrollBar()->value();
    bufdata->out1Vert = ui->fixedVars->indexOfTopLevelItem(ui->fixedVars->itemAt(QPoint(0, 0)));
  }
}

void BufferViewer::OnEventChanged(uint32_t eventId)
{
  PopulateBufferData *bufdata = new PopulateBufferData;

  m_Sequence++;
  bufdata->sequence = m_Sequence;

  if(m_Scrolls)
  {
    bufdata->inHoriz = m_Scrolls->inHoriz;
    bufdata->out1Horiz = m_Scrolls->out1Horiz;
    bufdata->out2Horiz = m_Scrolls->out2Horiz;

    bufdata->inVert = m_Scrolls->inVert;
    bufdata->out1Vert = m_Scrolls->out1Vert;
    bufdata->out2Vert = m_Scrolls->out2Vert;

    delete m_Scrolls;
    m_Scrolls = NULL;
  }
  else
  {
    FillScrolls(bufdata);
  }

  // remove any pending scrolls, which have been applied. If nothing changes over the data
  // population the above scroll preserving will work.
  // however if m_Scroll is set while data is populationg, we'll apply it when it comes to the end
  m_Scroll[(int)MeshDataStage::VSIn] = QPoint(-1, -1);
  m_Scroll[(int)MeshDataStage::VSOut] = QPoint(-1, -1);
  m_Scroll[(int)MeshDataStage::GSOut] = QPoint(-1, -1);

  bufdata->highlightNames[0] = m_ModelIn->posName();
  bufdata->highlightNames[1] = m_ModelIn->secondaryName();
  bufdata->highlightNames[2] = m_ModelOut1->posName();
  bufdata->highlightNames[3] = m_ModelOut1->secondaryName();
  bufdata->highlightNames[4] = m_ModelOut2->posName();
  bufdata->highlightNames[5] = m_ModelOut2->secondaryName();

  const ActionDescription *action = m_Ctx.CurAction();

  bufdata->meshDispatch = action && (action->flags & ActionFlags::MeshDispatch);

  configureDrawRange();

  if(m_MeshView)
  {
    ClearModels();

    CalcColumnWidth();

    ClearModels();

    const PipeState &pipe = m_Ctx.CurPipelineState();

    if(pipe.IsRestartEnabled() && action && (action->flags & ActionFlags::Indexed))
    {
      bufdata->inConfig.primRestart = pipe.GetRestartIndex();

      if(pipe.GetIBuffer().byteStride == 1)
        bufdata->inConfig.primRestart &= 0xff;
      else if(pipe.GetIBuffer().byteStride == 2)
        bufdata->inConfig.primRestart &= 0xffff;

      bufdata->out1Config.primRestart = bufdata->inConfig.primRestart;
      // GS Out doesn't use primitive restart because it is post-expansion
    }

    ConfigureColumns(m_Ctx, bufdata);

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
    // update with the current cbuffer for the current slot
    if(IsCBufferView())
    {
      UsedDescriptor cb = m_Ctx.CurPipelineState().GetConstantBlock(
          m_CBufferSlot.stage, m_CBufferSlot.slot, m_CBufferSlot.arrayIdx);
      m_BufferID = cb.descriptor.resource;
      m_ByteOffset = cb.descriptor.byteOffset;
      m_ByteSize = cb.descriptor.byteSize;

      const ShaderReflection *reflection =
          m_Ctx.CurPipelineState().GetShaderReflection(m_CBufferSlot.stage);
      bufdata->cb.valid =
          (reflection != NULL && m_CBufferSlot.slot < reflection->constantBlocks.size());
      if(bufdata->cb.valid)
      {
        bufdata->cb.bytesBacked = reflection->constantBlocks[m_CBufferSlot.slot].bufferBacked ||
                                  reflection->constantBlocks[m_CBufferSlot.slot].inlineDataBytes;
        bufdata->cb.compileConstants =
            reflection->constantBlocks[m_CBufferSlot.slot].compileConstants;
      }

      ui->setFormat->setEnabled(bufdata->cb.bytesBacked);
      if(ui->setFormat->isEnabled())
        ui->setFormat->setToolTip(tr("Specify a custom format for this constant buffer"));
      else
        ui->setFormat->setToolTip(tr("Cannot specify custom format without backing memory"));

      bufdata->cb.pipe = m_CBufferSlot.stage == ShaderStage::Compute
                             ? m_Ctx.CurPipelineState().GetComputePipelineObject()
                             : m_Ctx.CurPipelineState().GetGraphicsPipelineObject();
      bufdata->cb.shader = m_Ctx.CurPipelineState().GetShader(m_CBufferSlot.stage);
      bufdata->cb.entryPoint = m_Ctx.CurPipelineState().GetShaderEntryPoint(m_CBufferSlot.stage);

      if(m_Format.isEmpty())
      {
        // stage, slot, and array index are all invariant when viewing a constant buffer
        // ee only need to use the actual bound shader as a key.
        RDTreeViewExpansionState &prevShaderExpansionState =
            ui->fixedVars->getInternalExpansion(qHash(ToQStr(m_CurCBuffer.shader)));

        ui->fixedVars->saveExpansion(prevShaderExpansionState, 0);
      }
    }

    ParsedFormat parsed = BufferFormatter::ParseFormatString(m_Format, m_ByteSize, IsCBufferView());

    bufdata->inConfig.fixedVars = parsed.fixed;
    bufdata->inConfig.packing = parsed.packing;

    if(parsed.repeating.type.baseType != VarType::Unknown)
    {
      bufdata->inConfig.repeatStride = parsed.repeating.type.arrayByteStride;
      bufdata->inConfig.repeatOffset = parsed.repeating.byteOffset;

      UnrollConstant(parsed.repeating, bufdata->inConfig.columns, bufdata->inConfig.props);
    }
    else
    {
      bufdata->inConfig.repeatStride = 1U;
      bufdata->inConfig.repeatOffset = parsed.fixed.type.arrayByteStride;
    }

    if((m_Format.isEmpty() || !bufdata->cb.bytesBacked) && IsCBufferView())
    {
      if(bufdata->cb.valid)
      {
        const ShaderReflection *reflection =
            m_Ctx.CurPipelineState().GetShaderReflection(m_CBufferSlot.stage);

        bufdata->inConfig.fixedVars.type.members =
            reflection->constantBlocks[m_CBufferSlot.slot].variables;

        if(IsD3D(m_Ctx.APIProps().pipelineType))
          bufdata->inConfig.packing = Packing::D3DCB;
        else
          bufdata->inConfig.packing = BufferFormatter::EstimatePackingRules(
              reflection->resourceId, bufdata->inConfig.fixedVars.type.members);
      }
    }

    ClearModels();
  }

  updateLabelsAndLayout();

  bufdata->inConfig.curInstance = bufdata->out1Config.curInstance =
      bufdata->out2Config.curInstance = m_Config.curInstance;
  bufdata->inConfig.curView = bufdata->out1Config.curView = bufdata->out2Config.curView =
      m_Config.curView;

  m_ModelIn->beginReset();
  m_ModelOut1->beginReset();
  m_ModelOut2->beginReset();

  bufdata->inConfig.baseVertex = action ? action->baseVertex : 0;

  ui->formatSpecifier->setEnabled(!IsCBufferView() || bufdata->cb.bytesBacked);

  ui->instance->setEnabled(action && (action->flags & ActionFlags::Instanced));
  if(!ui->instance->isEnabled())
    ui->instance->setValue(0);

  if(action)
    ui->instance->setMaximum(qMax(0, int(action->numInstances) - 1));

  uint32_t numViews = m_Ctx.CurPipelineState().MultiviewBroadcastCount();

  if(action && numViews > 1)
  {
    ui->viewIndex->setEnabled(true);
    ui->viewIndex->setMaximum(qMax(0, int(numViews) - 1));
  }
  else
  {
    ui->viewIndex->setEnabled(false);
    ui->viewIndex->setValue(0);
  }

  QPointer<BufferViewer> me(this);

  m_Ctx.Replay().AsyncInvoke([this, me, bufdata](IReplayController *r) {
    if(!me)
      return;

    BufferData *buf = NULL;

    if(m_MeshView)
    {
      if(bufdata->meshDispatch)
      {
        bufdata->postOut1 = r->GetPostVSData(0, bufdata->inConfig.curView, MeshDataStage::TaskOut);
        bufdata->postOut2 = r->GetPostVSData(0, bufdata->inConfig.curView, MeshDataStage::MeshOut);

        // apply mesh/task filtering to mesh data here, which will also propagate to preview
        if(m_FilteredMeshGroup != ~0U)
        {
          bufdata->out1Config.taskOrMeshletOffset = m_FilteredTaskGroup;

          // find this meshlet's offset in the index buffer and filter to only it
          uint32_t indexCount = 0, vertexCount = 0;
          for(uint32_t i = 0; i <= m_FilteredMeshGroup && i < bufdata->postOut2.meshletSizes.size();
              i++)
          {
            MeshletSize meshletSize = bufdata->postOut2.meshletSizes[i];
            uint32_t numIndices = meshletSize.numIndices;
            if(i == m_FilteredMeshGroup)
            {
              bufdata->postOut2.meshletIndexOffset = vertexCount;
              bufdata->postOut2.meshletOffset = m_FilteredMeshGroup;
              bufdata->out2Config.taskOrMeshletOffset = m_FilteredMeshGroup;
              bufdata->postOut2.numIndices = numIndices;
              bufdata->postOut2.meshletSizes = {meshletSize};
              bufdata->postOut2.indexByteOffset += indexCount * bufdata->postOut2.indexByteStride;
            }
            indexCount += numIndices;
            vertexCount += meshletSize.numVertices;
          }
        }
        else if(m_FilteredTaskGroup != ~0U)
        {
          bufdata->out1Config.taskOrMeshletOffset = m_FilteredTaskGroup;

          // find the relevant task and which mesh indices it corresponds to
          uint32_t meshletCounter = 0;
          for(uint32_t taskIndex = 0;
              taskIndex <= m_FilteredTaskGroup && taskIndex < bufdata->postOut1.taskSizes.size();
              taskIndex++)
          {
            uint32_t numMeshesInTask = bufdata->postOut1.taskSizes[taskIndex].x *
                                       bufdata->postOut1.taskSizes[taskIndex].y *
                                       bufdata->postOut1.taskSizes[taskIndex].z;

            // once we've found the desired task, filter our view to only its meshes
            if(taskIndex == m_FilteredTaskGroup)
            {
              bufdata->postOut2.numIndices = 0;

              rdcarray<MeshletSize> meshletSizes;
              meshletSizes.reserve(numMeshesInTask);

              uint32_t indexCount = 0, vertexCount = 0;
              for(uint32_t i = 0;
                  i < meshletCounter + numMeshesInTask && i < bufdata->postOut2.meshletSizes.size();
                  i++)
              {
                uint32_t indicesInMeshlet = bufdata->postOut2.meshletSizes[i].numIndices;

                if(i >= meshletCounter)
                {
                  bufdata->postOut2.numIndices += indicesInMeshlet;
                  meshletSizes.push_back(bufdata->postOut2.meshletSizes[i]);
                }

                if(i == meshletCounter)
                {
                  bufdata->postOut2.meshletIndexOffset = vertexCount;
                  bufdata->postOut2.meshletOffset = meshletCounter;
                  bufdata->out2Config.taskOrMeshletOffset = meshletCounter;
                  bufdata->postOut2.indexByteOffset += indexCount * bufdata->postOut2.indexByteStride;
                }
                indexCount += indicesInMeshlet;
                vertexCount += bufdata->postOut2.meshletSizes[i].numVertices;
              }

              bufdata->postOut2.meshletSizes = meshletSizes;

              break;
            }

            meshletCounter += numMeshesInTask;
          }
        }

        RT_FetchMeshPipeData(r, m_Ctx, bufdata);
      }
      else
      {
        bufdata->postOut1 = r->GetPostVSData(bufdata->inConfig.curInstance,
                                             bufdata->inConfig.curView, MeshDataStage::VSOut);
        bufdata->postOut2 = r->GetPostVSData(bufdata->inConfig.curInstance,
                                             bufdata->inConfig.curView, MeshDataStage::GSOut);

        RT_FetchVertexPipeData(r, m_Ctx, bufdata);
      }

      if(!me)
        return;
    }
    else
    {
      buf = new BufferData;

      // calculate tight stride
      buf->stride = std::max(1U, bufdata->inConfig.repeatStride);

      // we want to fetch the data for fixed and repeated sections (either of which might be 0)
      // but calculate the number of rows etc for the repeated sections based on just the data
      // available for it
      const uint64_t fixedLength = bufdata->inConfig.repeatOffset;

      // the "permanent" repeated range starts after the fixed data and goes for m_ByteSize
      uint64_t repeatedRangeStart = m_ByteOffset + fixedLength;
      uint64_t repeatedRangeEnd = m_ByteOffset + m_ByteSize;

      // if the byte size is unbounded, the end is unbounded - fix the potential overflow from
      // adding the offset
      if(m_ByteSize == UINT64_MAX)
        repeatedRangeEnd = UINT64_MAX;

      // get the underlying buffer length
      uint64_t bufferLength = 0;

      if(m_IsBuffer && m_BufferID != ResourceId())
      {
        const BufferDescription *desc = m_Ctx.GetBuffer(m_BufferID);
        if(desc)
          bufferLength = desc->length;
      }

      // clamp the range to the buffer length, which may end up with it being empty
      repeatedRangeEnd = qMin(repeatedRangeEnd, bufferLength);
      repeatedRangeStart = qMin(repeatedRangeStart, bufferLength);

      // store the number of rows unclamped without the paging window
      bufdata->inConfig.unclampedNumRows =
          uint32_t((repeatedRangeEnd - repeatedRangeStart + buf->stride - 1) / buf->stride);

      // advance the range by the paging offset
      repeatedRangeStart = qMin(repeatedRangeEnd, repeatedRangeStart + m_PagingByteOffset);

      // calculate the length clamped to the MaxVisibleRows
      const uint64_t clampedRepeatedLength =
          qMin(repeatedRangeEnd - repeatedRangeStart, uint64_t(buf->stride * (MaxVisibleRows + 2)));

      if(m_IsBuffer)
      {
        if(m_BufferID == ResourceId())
        {
          buf->storage.clear();
        }
        else if(repeatedRangeStart > fixedLength)
        {
          // if the repeated range subsection we're fetching is paged further in, we still need to
          // fetch the fixed data from the 'start'
          if(fixedLength > 0)
            buf->storage = r->GetBufferData(m_BufferID, m_ByteOffset, fixedLength);
          // then append the data from where we're paged to
          buf->storage.append(r->GetBufferData(m_BufferID, repeatedRangeStart, clampedRepeatedLength));
        }
        else
        {
          // otherwise we can fetch it all at once
          buf->storage =
              r->GetBufferData(m_BufferID, m_ByteOffset, fixedLength + clampedRepeatedLength);
        }
      }
      else
      {
        buf->storage = r->GetTextureData(m_BufferID, m_TexSub);

        // recalculate total size for this subresource based on the data returned
        if(!buf->storage.empty())
          m_ObjectByteSize = buf->storage.size();
      }

      uint32_t repeatedDataAvailable = uint32_t(buf->size());
      if(repeatedDataAvailable > fixedLength)
        repeatedDataAvailable -= fixedLength;

      bufdata->inConfig.pagingOffset = uint32_t(m_PagingByteOffset / buf->stride);
      bufdata->inConfig.numRows = uint32_t((repeatedDataAvailable + buf->stride - 1) / buf->stride);

      // ownership passes to model
      bufdata->inConfig.buffers.push_back(buf);

      if(!me)
      {
        delete buf;
        return;
      }
    }

    // for cbuffers, if the format is empty or if we're not buffer-backed and don't have inline
    // data, we evaluate variables here and don't use the format override with a fetched buffer
    if((m_Format.isEmpty() || !bufdata->cb.bytesBacked) && IsCBufferView())
    {
      // only fetch the cbuffer constants if this binding is currently valid
      if(bufdata->cb.valid)
        bufdata->inConfig.evalVars = r->GetCBufferVariableContents(
            bufdata->cb.pipe, bufdata->cb.shader, m_CBufferSlot.stage, bufdata->cb.entryPoint,
            m_CBufferSlot.slot, m_BufferID, m_ByteOffset, m_ByteSize);
    }

    GUIInvoke::call(this, [this, bufdata]() {
      if(bufdata->sequence != m_Sequence)
        return;

      if(!bufdata->out1Config.statusString.isEmpty())
      {
        bufdata->out1Config.columns.clear();
        bufdata->out1Config.props.clear();
        ConfigureStatusColumn(bufdata->out1Config.columns, bufdata->out1Config.props);
      }

      if(!bufdata->out2Config.statusString.isEmpty())
      {
        bufdata->out2Config.columns.clear();
        bufdata->out2Config.props.clear();
        ConfigureStatusColumn(bufdata->out2Config.columns, bufdata->out2Config.props);
      }

      m_ModelIn->endReset(bufdata->inConfig);
      m_ModelOut1->endReset(bufdata->out1Config);
      m_ModelOut2->endReset(bufdata->out2Config);

      m_Out1Data = bufdata->postOut1;
      m_Out2Data = bufdata->postOut2;

      m_CurCBuffer = bufdata->cb;

      // if we didn't have a position column selected before, or the name has changed, re-guess
      if(m_ModelIn->posColumn() == -1 ||
         bufdata->highlightNames[0] != bufdata->inConfig.columnName(m_ModelIn->posColumn()))
        m_ModelIn->setPosColumn(-1);
      // similarly for secondary columns
      if(m_ModelIn->secondaryColumn() == -1 ||
         bufdata->highlightNames[1] != bufdata->inConfig.columnName(m_ModelIn->secondaryColumn()))
        m_ModelIn->setSecondaryColumn(-1, m_Config.visualisationMode == Visualisation::Secondary,
                                      false);

      // and as above for VS Out / GS Out
      if(m_ModelOut1->posColumn() == -1 ||
         bufdata->highlightNames[2] != bufdata->out1Config.columnName(m_ModelOut1->posColumn()))
        m_ModelOut1->setPosColumn(-1);
      if(m_ModelOut1->secondaryColumn() == -1 ||
         bufdata->highlightNames[3] != bufdata->out1Config.columnName(m_ModelOut1->secondaryColumn()))
        m_ModelOut1->setSecondaryColumn(-1, m_Config.visualisationMode == Visualisation::Secondary,
                                        false);

      if(m_ModelOut2->posColumn() == -1 ||
         bufdata->highlightNames[4] != bufdata->out2Config.columnName(m_ModelOut2->posColumn()))
        m_ModelOut2->setPosColumn(-1);
      if(m_ModelOut2->secondaryColumn() == -1 ||
         bufdata->highlightNames[5] != bufdata->out2Config.columnName(m_ModelOut2->secondaryColumn()))
        m_ModelOut2->setSecondaryColumn(-1, m_Config.visualisationMode == Visualisation::Secondary,
                                        false);

      EnableCameraGuessControls();

      populateBBox(bufdata);

      UI_ConfigureFormats();
      UpdateCurrentMeshConfig();

      ApplyRowAndColumnDims(
          m_ModelIn->columnCount(), ui->inTable,
          bufdata->inConfig.statusString.isEmpty() ? m_DataColWidth : m_ErrorColWidth);
      ApplyRowAndColumnDims(
          m_ModelOut1->columnCount(), ui->out1Table,
          bufdata->out1Config.statusString.isEmpty() ? m_DataColWidth : m_ErrorColWidth);
      ApplyRowAndColumnDims(
          m_ModelOut2->columnCount(), ui->out2Table,
          bufdata->out2Config.statusString.isEmpty() ? m_DataColWidth : m_ErrorColWidth);

      uint32_t numRows = qMax(qMax(bufdata->inConfig.numRows, bufdata->out1Config.numRows),
                              bufdata->out2Config.numRows);

      if(!m_MeshView)
        numRows = qMax(numRows, bufdata->inConfig.unclampedNumRows);

      ui->rowOffset->setMaximum((int)qMax(1U, numRows) - 1);

      ScrollToRow(ui->inTable, qMin(int(bufdata->inConfig.numRows) - 1, bufdata->inVert));
      ScrollToRow(ui->out1Table, qMin(int(bufdata->out1Config.numRows) - 1, bufdata->out1Vert));
      ScrollToRow(ui->out2Table, qMin(int(bufdata->out2Config.numRows) - 1, bufdata->out2Vert));

      ui->inTable->horizontalScrollBar()->setValue(bufdata->inHoriz);
      ui->out1Table->horizontalScrollBar()->setValue(bufdata->out1Horiz);
      ui->out2Table->horizontalScrollBar()->setValue(bufdata->out2Horiz);

      for(MeshDataStage stage : {MeshDataStage::VSIn, MeshDataStage::VSOut, MeshDataStage::GSOut})
      {
        int i = (int)stage;

        if(m_Scroll[i].y() >= 0)
          ScrollToRow(tableForStage(stage), m_Scroll[i].y());
        if(m_Scroll[i].x() >= 0)
          ScrollToColumn(tableForStage(stage), m_Scroll[i].x());

        m_Scroll[i] = QPoint(-1, -1);
      }

      if(m_MeshView)
      {
        RDTreeViewExpansionState state;
        ui->fixedVars->saveExpansion(state, 0);

        ui->fixedVars->beginUpdate();

        ui->fixedVars->clear();

        if(bufdata->meshDispatch && !bufdata->out1Config.statusString.isEmpty())
        {
          RDTreeWidgetItem *n =
              new RDTreeWidgetItem({lit("-, -, -"), bufdata->out1Config.statusString, QString()});

          ui->fixedVars->addTopLevelItem(n);
        }
        else if(bufdata->meshDispatch && !bufdata->out1Config.taskSizes.empty())
        {
          const ActionDescription *action = m_Ctx.CurAction();

          uint32_t i = 0;
          for(uint32_t x = 0; x < bufdata->out1Config.dispatchSize[0]; x++)
          {
            for(uint32_t y = 0; y < bufdata->out1Config.dispatchSize[1]; y++)
            {
              for(uint32_t z = 0; z < bufdata->out1Config.dispatchSize[2]; z++)
              {
                TaskGroupSize size = bufdata->out1Config.taskSizes[i];

                RDTreeWidgetItem *n = NULL;

                if(m_CurMeshFilter == MeshFilter::None || m_FilteredTaskGroup == i)
                {
                  n = new RDTreeWidgetItem(
                      {QFormatStr("%1, %2, %3").arg(x).arg(y).arg(z),
                       QFormatStr("Dispatched [%1, %2, %3]").arg(size.x).arg(size.y).arg(size.z),
                       lit("Task Group")});

                  ui->fixedVars->addTopLevelItem(n);
                }

                if(n && !bufdata->out1Config.columns.empty())
                {
                  UI_AddTaskPayloads(n, i * bufdata->out1Config.buffers[0]->stride,
                                     bufdata->out1Config.columns, bufdata->out1Config.buffers[0]);
                }

                i++;
              }
            }
          }
        }

        ui->fixedVars->endUpdate();

        ResourceId shader = m_Ctx.CurPipelineState().GetShader(ShaderStage::Task);

        // if we have saved expansion state for the new shader, apply it, otherwise apply the
        // previous one to get any overlap (e.g. two different shaders with very similar or
        // identical constants)
        if(ui->fixedVars->hasInternalExpansion(qHash(ToQStr(shader))))
          ui->fixedVars->applyExpansion(ui->fixedVars->getInternalExpansion(qHash(ToQStr(shader))),
                                        0);
        else
          ui->fixedVars->applyExpansion(state, 0);

        if(bufdata->out1Vert >= 0 && bufdata->out1Vert < ui->fixedVars->topLevelItemCount())
        {
          ScrollToRow(bufdata->out1Vert, MeshDataStage::TaskOut);
          ui->fixedVars->horizontalScrollBar()->setValue(bufdata->out1Horiz);
        }
      }

      if(!m_MeshView)
      {
        m_RepeatedOffset->setText(
            tr("Starting at: %1 bytes")
                .arg(Formatter::HumanFormat(m_ByteOffset + bufdata->inConfig.repeatOffset,
                                            Formatter::OffsetSize)));

        {
          rdcarray<ShaderVariable> vars;

          if(m_BufferID == ResourceId() || m_Format.isEmpty())
          {
            vars = bufdata->inConfig.evalVars;
          }
          else
          {
            ShaderVariable var = InterpretShaderVar(bufdata->inConfig.fixedVars,
                                                    bufdata->inConfig.buffers[0]->storage.begin(),
                                                    bufdata->inConfig.buffers[0]->storage.end());

            vars.swap(var.members);
          }

          bool wasEmpty = ui->fixedVars->topLevelItemCount() == 0;

          RDTreeViewExpansionState state;
          ui->fixedVars->saveExpansion(state, 0);

          ui->fixedVars->beginUpdate();

          ui->fixedVars->clear();

          if(!vars.isEmpty())
          {
            UI_AddFixedVariables(ui->fixedVars->invisibleRootItem(), 0,
                                 bufdata->inConfig.fixedVars.type.members, vars);

            if(IsCBufferView() && !bufdata->cb.bytesBacked)
              UI_RemoveOffsets(ui->fixedVars->invisibleRootItem());
          }

          ui->fixedVars->endUpdate();

          if(wasEmpty)
          {
            // Expand before resizing so that collapsed data will already be visible when expanded
            ui->fixedVars->expandAll();
            for(int i = 0; i < ui->fixedVars->header()->count(); i++)
              ui->fixedVars->resizeColumnToContents(i);
            ui->fixedVars->collapseAll();
          }

          // if we have saved expansion state for the new shader, apply it, otherwise apply the
          // previous one to get any overlap (e.g. two different shaders with very similar or
          // identical constants)
          if(ui->fixedVars->hasInternalExpansion(qHash(ToQStr(m_CurCBuffer.shader))))
            ui->fixedVars->applyExpansion(
                ui->fixedVars->getInternalExpansion(qHash(ToQStr(m_CurCBuffer.shader))), 0);
          else
            ui->fixedVars->applyExpansion(state, 0);
        }

        on_rowOffset_valueChanged(ui->rowOffset->value());

        const bool prev = (bufdata->inConfig.pagingOffset > 0);
        const bool next = (bufdata->inConfig.numRows >= MaxVisibleRows);

        if(prev && next)
        {
          ui->inTable->setIndexWidget(m_ModelIn->index(0, 0), MakePreviousPageButton());
          ui->inTable->setIndexWidget(m_ModelIn->index(0, 1), MakeNextPageButton());

          ui->inTable->setIndexWidget(m_ModelIn->index(MaxVisibleRows + 1, 0),
                                      MakePreviousPageButton());
          ui->inTable->setIndexWidget(m_ModelIn->index(MaxVisibleRows + 1, 1), MakeNextPageButton());
        }
        else if(prev)
        {
          ui->inTable->setIndexWidget(m_ModelIn->index(0, 0), MakePreviousPageButton());
        }
        else if(next)
        {
          ui->inTable->setIndexWidget(m_ModelIn->index(MaxVisibleRows, 1), MakeNextPageButton());
        }
      }

      // we're done with it, the buffer configurations are individually copied/refcounted
      delete bufdata;

      INVOKE_MEMFN(RT_UpdateAndDisplay);
    });
  });
}

void BufferViewer::populateBBox(PopulateBufferData *bufdata)
{
  const ActionDescription *action = m_Ctx.CurAction();

  if(action && m_MeshView)
  {
    uint32_t eventId = action->eventId;
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

    bbox->input[0] = bufdata->inConfig;
    bbox->input[1] = bufdata->out1Config;
    bbox->input[2] = bufdata->out1Config;

    QPointer<BufferViewer> me(this);

    // fire up a thread to calculate the bounding box
    LambdaThread *thread = new LambdaThread([this, me, bbox] {
      if(!me)
        return;

      calcBoundingData(*bbox);

      if(!me)
        return;

      GUIInvoke::call(this, [this, bbox]() { UI_UpdateBoundingBox(*bbox); });
    });
    thread->setName(lit("BBox calc"));
    thread->selfDelete(true);
    thread->start();

    // give the thread a few ms to finish, so we don't get a tiny flicker on small/fast meshes
    thread->wait(10);
  }
}

QVariant BufferViewer::persistData()
{
  QVariantMap state;
  state = ui->dockarea->saveState();
  state[lit("axisMappingIndex")] = ui->axisMappingCombo->currentIndex();
  QVariantList xAxisMapping = {QVariant(m_Config.axisMapping.xAxis.x),
                               QVariant(m_Config.axisMapping.xAxis.y),
                               QVariant(m_Config.axisMapping.xAxis.z)};
  state[lit("xAxisMapping")] = xAxisMapping;
  QVariantList yAxisMapping = {QVariant(m_Config.axisMapping.yAxis.x),
                               QVariant(m_Config.axisMapping.yAxis.y),
                               QVariant(m_Config.axisMapping.yAxis.z)};
  state[lit("yAxisMapping")] = yAxisMapping;
  QVariantList zAxisMapping = {QVariant(m_Config.axisMapping.zAxis.x),
                               QVariant(m_Config.axisMapping.zAxis.y),
                               QVariant(m_Config.axisMapping.zAxis.z)};
  state[lit("zAxisMapping")] = zAxisMapping;

  return state;
}

void BufferViewer::setPersistData(const QVariant &persistData)
{
  QVariantMap state = persistData.toMap();

  ui->dockarea->restoreState(state);
  previousAxisMappingIndex = state[lit("axisMappingIndex")].toInt();
  ui->axisMappingCombo->setCurrentIndex(previousAxisMappingIndex);
  if(!state[lit("xAxisMapping")].toList().isEmpty())
  {
    m_Config.axisMapping.xAxis.x = state[lit("xAxisMapping")].toList()[0].toInt();
    m_Config.axisMapping.xAxis.y = state[lit("xAxisMapping")].toList()[1].toInt();
    m_Config.axisMapping.xAxis.z = state[lit("xAxisMapping")].toList()[2].toInt();
    m_Config.axisMapping.yAxis.x = state[lit("yAxisMapping")].toList()[0].toInt();
    m_Config.axisMapping.yAxis.y = state[lit("yAxisMapping")].toList()[1].toInt();
    m_Config.axisMapping.yAxis.z = state[lit("yAxisMapping")].toList()[2].toInt();
    m_Config.axisMapping.zAxis.x = state[lit("zAxisMapping")].toList()[0].toInt();
    m_Config.axisMapping.zAxis.y = state[lit("zAxisMapping")].toList()[1].toInt();
    m_Config.axisMapping.zAxis.z = state[lit("zAxisMapping")].toList()[2].toInt();
  }
}

void BufferViewer::UI_FixedAddMatrixRows(RDTreeWidgetItem *n, const ShaderConstant &c,
                                         const ShaderVariable &v)
{
  const bool showPadding = ui->showPadding->isChecked() && m_CurCBuffer.bytesBacked;

  if(v.rows > 1)
  {
    uint32_t vecSize = VarTypeByteSize(v.type) * v.columns;

    FixedVarTag tag = n->tag().value<FixedVarTag>();
    tag.matrix = true;
    tag.rowmajor = v.RowMajor();
    n->setTag(QVariant::fromValue(tag));

    if(v.ColMajor())
      vecSize = VarTypeByteSize(v.type) * v.rows;

    for(uint32_t r = 0; r < v.rows; r++)
    {
      n->addChild(new RDTreeWidgetItem({QFormatStr("%1.row%2").arg(v.name).arg(r), RowString(v, r),
                                        QString(), RowTypeString(v)}));

      if(showPadding && v.RowMajor() && c.type.matrixByteStride > vecSize)
      {
        uint32_t size = c.type.matrixByteStride - vecSize;

        RDTreeWidgetItem *pad = new RDTreeWidgetItem({
            tr(""),
            QFormatStr("%1 bytes").arg(Formatter::HumanFormat(size, Formatter::OffsetSize)),
            QString(),
            tr("Padding"),
        });

        pad->setItalic(true);
        pad->setTag(QVariant::fromValue(FixedVarTag(size)));

        n->addChild(pad);
      }
    }

    if(showPadding && v.ColMajor() && c.type.matrixByteStride > vecSize)
    {
      uint32_t size = c.type.matrixByteStride - vecSize;

      RDTreeWidgetItem *pad = new RDTreeWidgetItem({
          tr(""),
          QFormatStr("%1 bytes each column").arg(Formatter::HumanFormat(size, Formatter::OffsetSize)),
          QString(),
          tr("Padding"),
      });

      pad->setItalic(true);
      pad->setTag(QVariant::fromValue(FixedVarTag(size)));

      n->addChild(pad);
    }
  }
}

static void TaskAddMatrixRows(RDTreeWidgetItem *n, const ShaderConstant &c, const ShaderVariable &v)
{
  if(v.rows > 1)
  {
    uint32_t vecSize = VarTypeByteSize(v.type) * v.columns;

    if(v.ColMajor())
      vecSize = VarTypeByteSize(v.type) * v.rows;

    for(uint32_t r = 0; r < v.rows; r++)
    {
      n->addChild(new RDTreeWidgetItem(
          {QFormatStr("%1.row%2").arg(v.name).arg(r), RowString(v, r), RowTypeString(v)}));
    }
  }
}

void BufferViewer::UI_AddTaskPayloads(RDTreeWidgetItem *root, size_t baseOffset,
                                      const rdcarray<ShaderConstant> &consts, BufferData *buffer)
{
  uint32_t offset = 0;

  for(size_t idx = 0; idx < consts.size(); idx++)
  {
    const ShaderConstant &c = consts[idx];
    ShaderVariable v = InterpretShaderVar(c, buffer->data() + baseOffset + offset, buffer->end());

    RDTreeWidgetItem *n = new RDTreeWidgetItem({v.name, VarString(v, c), TypeString(v, c)});

    root->addChild(n);

    TaskAddMatrixRows(n, c, v);

    // if it's an array the value (v) will be expanded with one element in each of v.members, but
    // the constant (c) will just have the type with a number of elements
    if(c.type.elements > 1)
    {
      ShaderConstant noarray = c;
      noarray.type.elements = 1;

      // calculate the tight scalar-packed advance, so we can detect padding
      uint32_t elSize = BufferFormatter::GetVarAdvance(Packing::Scalar, noarray);

      for(uint32_t e = 0; e < v.members.size(); e++)
      {
        const uint32_t elOffset = (uint32_t)baseOffset + c.byteOffset + c.type.arrayByteStride * e;

        RDTreeWidgetItem *el = new RDTreeWidgetItem(
            {v.members[e].name, VarString(v.members[e], c), TypeString(v.members[e], c)});

        // if it's an array of structs we can recurse, just need to do the outer iteration here
        // because v.members[...].members will be the actual struct members because of the expansion
        if(c.type.baseType == VarType::Struct)
        {
          UI_AddTaskPayloads(el, elOffset, c.type.members, buffer);
        }
        else
        {
          // otherwise just expand by hand since there will be no more members in c.type.members for
          // us to recurse with
          TaskAddMatrixRows(el, c, v.members[e]);
        }

        n->addChild(el);

        // don't count the padding in the last struct in an array of structs, it will be handled as
        // padding after the array
        if(c.type.baseType == VarType::Struct && e + 1 == v.members.size())
          break;
      }
    }
    // for single structs, recurse
    else if(v.type == VarType::Struct)
    {
      UI_AddTaskPayloads(n, c.byteOffset, c.type.members, buffer);
    }

    // advance by the tight scalar-packed advance, so we can detect padding
    offset += BufferFormatter::GetVarAdvance(Packing::Scalar, c);
  }
}

void BufferViewer::UI_AddFixedVariables(RDTreeWidgetItem *root, uint32_t baseOffset,
                                        const rdcarray<ShaderConstant> &consts,
                                        const rdcarray<ShaderVariable> &vars)
{
  const bool showPadding = ui->showPadding->isChecked() && m_CurCBuffer.bytesBacked;

  if(consts.size() != vars.size())
    qCritical() << "Shader variable mismatch";

  uint32_t offset = 0;

  for(size_t idx = 0; idx < consts.size() && idx < vars.size(); idx++)
  {
    const ShaderConstant &c = consts[idx];
    const ShaderVariable &v = vars[idx];

    if(showPadding && c.byteOffset > offset)
    {
      uint32_t size = c.byteOffset - offset;

      RDTreeWidgetItem *pad = new RDTreeWidgetItem({
          QString(),
          QFormatStr("%1 bytes").arg(Formatter::HumanFormat(size, Formatter::OffsetSize)),
          QString(),
          tr("Padding"),
      });

      pad->setItalic(true);
      pad->setTag(QVariant::fromValue(FixedVarTag(size)));

      root->addChild(pad);

      offset = c.byteOffset;
    }

    QVariant offsetStr = Formatter::HumanFormat(baseOffset + c.byteOffset, Formatter::OffsetSize);

    if(c.bitFieldSize != 0)
    {
      offsetStr =
          offsetStr.toString() +
          QFormatStr(" (bits %1:%2)").arg(c.bitFieldOffset).arg(c.bitFieldOffset + c.bitFieldSize);
    }

    if(m_CurCBuffer.compileConstants)
      offsetStr = lit("-");

    RDTreeWidgetItem *n =
        new RDTreeWidgetItem({v.name, VarString(v, c), offsetStr, TypeString(v, c)});

    n->setTag(QVariant::fromValue(FixedVarTag(v.name, baseOffset + c.byteOffset)));

    root->addChild(n);

    UI_FixedAddMatrixRows(n, c, v);

    // if it's an array the value (v) will be expanded with one element in each of v.members, but
    // the constant (c) will just have the type with a number of elements
    if(c.type.elements > 1)
    {
      ShaderConstant noarray = c;
      noarray.type.elements = 1;

      // calculate the tight scalar-packed advance, so we can detect padding
      uint32_t elSize = BufferFormatter::GetVarAdvance(Packing::Scalar, noarray);

      for(uint32_t e = 0; e < v.members.size(); e++)
      {
        const uint32_t elOffset = baseOffset + c.byteOffset + c.type.arrayByteStride * e;

        RDTreeWidgetItem *el = new RDTreeWidgetItem({
            v.members[e].name,
            VarString(v.members[e], c),
            m_CurCBuffer.compileConstants ? lit("-")
                                          : Formatter::HumanFormat(elOffset, Formatter::OffsetSize),
            TypeString(v.members[e], c),
        });

        el->setTag(QVariant::fromValue(FixedVarTag(v.members[e].name, elOffset)));

        // if it's an array of structs we can recurse, just need to do the outer iteration here
        // because v.members[...].members will be the actual struct members because of the expansion
        if(c.type.baseType == VarType::Struct)
        {
          UI_AddFixedVariables(el, elOffset, c.type.members, v.members[e].members);
        }
        else
        {
          // otherwise just expand by hand since there will be no more members in c.type.members for
          // us to recurse with
          UI_FixedAddMatrixRows(el, c, v.members[e]);
        }

        n->addChild(el);

        // don't count the padding in the last struct in an array of structs, it will be handled as
        // padding after the array
        if(c.type.baseType == VarType::Struct && e + 1 == v.members.size())
          break;

        if(showPadding && c.type.arrayByteStride > elSize)
        {
          uint32_t size = c.type.arrayByteStride - elSize;

          RDTreeWidgetItem *pad = new RDTreeWidgetItem({
              QString(),
              QFormatStr("%1 bytes").arg(Formatter::HumanFormat(size, Formatter::OffsetSize)),
              QString(),
              tr("Padding"),
          });

          pad->setItalic(true);
          pad->setTag(QVariant::fromValue(FixedVarTag(size)));

          n->addChild(pad);
        }
      }
    }
    // for single structs, recurse
    else if(v.type == VarType::Struct)
    {
      UI_AddFixedVariables(n, c.byteOffset, c.type.members, v.members);
    }

    // advance by the tight scalar-packed advance, so we can detect padding
    offset += BufferFormatter::GetVarAdvance(Packing::Scalar, c);
  }
}

void BufferViewer::UI_RemoveOffsets(RDTreeWidgetItem *root)
{
  for(int i = 0; i < root->childCount(); i++)
  {
    RDTreeWidgetItem *item = root->child(i);
    item->setText(2, QVariant());
    UI_RemoveOffsets(item);
  }
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

      if(s.columns[i].type.columns == 1)
        maxvec.y = maxvec.z = maxvec.w = 0.0;
      else if(s.columns[i].type.columns == 2)
        maxvec.z = maxvec.w = 0.0;
      else if(s.columns[i].type.columns == 3)
        maxvec.w = 0.0;

      minOutputList.push_back(maxvec);
      maxOutputList.push_back(FloatVector(-maxvec.x, -maxvec.y, -maxvec.z, -maxvec.w));
    }

    QVector<CachedElData> cache;

    CacheDataForIteration(cache, s.columns, s.props, s.buffers, bbox.input[0].curInstance);

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
        const ShaderConstant *el = d.el;
        const BufferElementProperties *prop = d.prop;

        float *minOut = (float *)&minOutputList[col];
        float *maxOut = (float *)&maxOutputList[col];

        if(d.data)
        {
          const byte *bytes = d.data;

          if(!prop->perinstance)
            bytes += d.stride * idx;

          QVariantList list = GetVariants(prop->format, *el, bytes, d.end);

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
        compCount = model->getConfig().columns[posEl].type.columns;
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

        if(!isCurrentRasterOut())
        {
          // apply axis mapping to midpoint
          FloatVector transformedMid;
          transformedMid.x = m_Config.axisMapping.xAxis.x * mid.x +
                             m_Config.axisMapping.yAxis.x * mid.y +
                             m_Config.axisMapping.zAxis.x * mid.z;
          transformedMid.y = m_Config.axisMapping.xAxis.y * mid.x +
                             m_Config.axisMapping.yAxis.y * mid.y +
                             m_Config.axisMapping.zAxis.y * mid.z;
          transformedMid.z = m_Config.axisMapping.xAxis.z * mid.x +
                             m_Config.axisMapping.yAxis.z * mid.y +
                             m_Config.axisMapping.zAxis.z * mid.z;
          mid = transformedMid;
        }

        m_Arcball->Reset(mid, len * 0.7f);

        GUIInvoke::call(this, [this, len]() { ui->camSpeed->setValue(len / 200.0f); });
      }
    }
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::UI_ConfigureFormats()
{
  if(!m_MeshView)
    return;

  const ActionDescription *action = m_Ctx.CurAction();

  if(action && (action->flags & ActionFlags::MeshDispatch))
    UI_ConfigureMeshPipeFormats();
  else
    UI_ConfigureVertexPipeFormats();
}

void BufferViewer::UI_ConfigureVertexPipeFormats()
{
  const PipeState &pipe = m_Ctx.CurPipelineState();

  rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
  const ActionDescription *action = m_Ctx.CurAction();

  if(action)
  {
    m_InPosition = MeshFormat();
    m_InSecondary = MeshFormat();

    m_InPosition.allowRestart = pipe.IsRestartEnabled() && (action->flags & ActionFlags::Indexed);
    m_InPosition.restartIndex = pipe.GetRestartIndex();

    const BufferConfiguration &vsinConfig = m_ModelIn->getConfig();

    if(!vsinConfig.columns.empty())
    {
      int elIdx = m_ModelIn->posColumn();
      if(elIdx < 0 || elIdx >= vsinConfig.columns.count())
        elIdx = 0;

      if(vsinConfig.unclampedNumRows > 0)
        m_InPosition.numIndices = vsinConfig.numRows;
      else
        m_InPosition.numIndices = action->numIndices;

      if((action->flags & ActionFlags::Instanced) && action->numInstances == 0)
        m_InPosition.numIndices = 0;

      BoundVBuffer ib = pipe.GetIBuffer();
      m_InPosition.topology = pipe.GetPrimitiveTopology();
      m_InPosition.indexByteStride = ib.byteStride;
      m_InPosition.baseVertex = action->baseVertex;
      m_InPosition.indexResourceId = ib.resourceId;

      uint32_t drawIdxByteOffs = action->indexOffset * ib.byteStride;
      m_InPosition.indexByteOffset = ib.byteOffset + drawIdxByteOffs;
      if(ib.byteSize >= ~0U)
        m_InPosition.indexByteSize = ib.byteSize;
      else if(drawIdxByteOffs > ib.byteSize)
        m_InPosition.indexByteSize = 0;
      else
        m_InPosition.indexByteSize = ib.byteSize - drawIdxByteOffs;

      if((action->flags & ActionFlags::Indexed) && m_InPosition.indexByteStride == 0)
        m_InPosition.indexByteStride = 4U;

      {
        const ShaderConstant &el = vsinConfig.columns[elIdx];
        const BufferElementProperties &prop = vsinConfig.props[elIdx];

        m_InPosition.instanced = prop.perinstance;
        m_InPosition.instStepRate = prop.instancerate;

        if(prop.buffer < vbs.count() && !vsinConfig.genericsEnabled[elIdx])
        {
          m_InPosition.vertexResourceId = vbs[prop.buffer].resourceId;
          m_InPosition.vertexByteStride = vbs[prop.buffer].byteStride;
          m_InPosition.vertexByteOffset = vbs[prop.buffer].byteOffset + el.byteOffset +
                                          action->vertexOffset * m_InPosition.vertexByteStride;
          m_InPosition.vertexByteSize = vbs[prop.buffer].byteSize;
        }
        else
        {
          m_InPosition.vertexResourceId = ResourceId();
          m_InPosition.vertexByteStride = 0;
          m_InPosition.vertexByteOffset = 0;
        }

        m_InPosition.format = prop.format;
      }

      elIdx = m_ModelIn->secondaryColumn();

      if(elIdx >= 0 && elIdx < vsinConfig.columns.count())
      {
        const ShaderConstant &el = vsinConfig.columns[elIdx];
        const BufferElementProperties &prop = vsinConfig.props[elIdx];

        m_InSecondary.instanced = prop.perinstance;
        m_InSecondary.instStepRate = prop.instancerate;

        if(prop.buffer < vbs.count() && !vsinConfig.genericsEnabled[elIdx])
        {
          m_InSecondary.vertexResourceId = vbs[prop.buffer].resourceId;
          m_InSecondary.vertexByteStride = vbs[prop.buffer].byteStride;
          m_InSecondary.vertexByteOffset = vbs[prop.buffer].byteOffset + el.byteOffset +
                                           action->vertexOffset * m_InSecondary.vertexByteStride;
          m_InSecondary.vertexByteSize = vbs[prop.buffer].byteSize;
        }
        else
        {
          m_InSecondary.vertexResourceId = ResourceId();
          m_InSecondary.vertexByteStride = 0;
          m_InSecondary.vertexByteOffset = 0;
        }

        m_InSecondary.format = prop.format;
        m_InSecondary.showAlpha = m_ModelIn->secondaryAlpha();
      }
    }

    const BufferConfiguration &out1Config = m_ModelOut1->getConfig();

    m_Out1Position = MeshFormat();
    m_Out1Secondary = MeshFormat();

    if(!out1Config.columns.empty())
    {
      int elIdx = m_ModelOut1->posColumn();
      if(elIdx < 0 || elIdx >= out1Config.columns.count())
        elIdx = 0;

      const ShaderConstant &el = out1Config.columns[elIdx];
      const BufferElementProperties &prop = out1Config.props[elIdx];

      m_Out1Position = m_Out1Data;
      m_Out1Position.vertexByteOffset += el.byteOffset;
      m_Out1Position.unproject = prop.systemValue == ShaderBuiltin::Position;
      m_Out1Position.format.compCount = el.type.columns;

      // if geometry/tessellation is enabled, don't unproject VS output data
      if(m_Ctx.CurPipelineState().GetShader(ShaderStage::Tess_Eval) != ResourceId() ||
         m_Ctx.CurPipelineState().GetShader(ShaderStage::Geometry) != ResourceId())
        m_Out1Position.unproject = false;

      elIdx = m_ModelOut1->secondaryColumn();

      if(elIdx >= 0 && elIdx < out1Config.columns.count())
      {
        m_Out1Secondary = m_Out1Data;
        m_Out1Secondary.vertexByteOffset += out1Config.columns[elIdx].byteOffset;
        m_Out1Secondary.format = prop.format;
        m_Out1Secondary.showAlpha = m_ModelOut1->secondaryAlpha();
      }
    }

    m_Out1Position.allowRestart = m_InPosition.allowRestart;
    m_Out1Position.restartIndex = m_InPosition.restartIndex;

    const BufferConfiguration &out2Config = m_ModelOut2->getConfig();

    m_Out2Position = MeshFormat();
    m_Out2Secondary = MeshFormat();

    if(!out2Config.columns.empty())
    {
      int elIdx = m_ModelOut2->posColumn();
      if(elIdx < 0 || elIdx >= out2Config.columns.count())
        elIdx = 0;

      const ShaderConstant &el = out2Config.columns[elIdx];
      const BufferElementProperties &prop = out2Config.props[elIdx];

      m_Out2Position = m_Out2Data;
      m_Out2Position.vertexByteOffset += el.byteOffset;
      m_Out2Position.unproject = prop.systemValue == ShaderBuiltin::Position;

      elIdx = m_ModelOut2->secondaryColumn();

      if(elIdx >= 0 && elIdx < out2Config.columns.count())
      {
        m_Out2Secondary = m_Out2Data;
        m_Out2Secondary.vertexByteOffset += out2Config.columns[elIdx].byteOffset;
        m_Out2Secondary.showAlpha = m_ModelOut2->secondaryAlpha();
      }
    }

    m_Out2Position.allowRestart = false;

    m_Out2Position.indexByteStride = 0;

    if(!(action->flags & ActionFlags::Indexed))
      m_Out1Position.indexByteStride = m_InPosition.indexByteStride = 0;
  }
  else
  {
    m_InPosition = MeshFormat();
    m_InSecondary = MeshFormat();

    m_Out1Position = MeshFormat();
    m_Out1Secondary = MeshFormat();

    m_Out2Position = MeshFormat();
    m_Out2Secondary = MeshFormat();
  }
}

void BufferViewer::UI_ConfigureMeshPipeFormats()
{
  const PipeState &pipe = m_Ctx.CurPipelineState();

  const ActionDescription *action = m_Ctx.CurAction();

  m_InPosition = MeshFormat();
  m_InSecondary = MeshFormat();

  // out1 is task shaders, which do not have displayable data
  m_Out1Position = MeshFormat();
  m_Out1Secondary = MeshFormat();

  const BufferConfiguration &out2Config = m_ModelOut2->getConfig();

  m_Out2Position = MeshFormat();
  m_Out2Secondary = MeshFormat();
  m_Out2Position.allowRestart = false;

  if(!out2Config.columns.empty())
  {
    int elIdx = m_ModelOut2->posColumn();
    if(elIdx < 0 || elIdx >= out2Config.columns.count())
      elIdx = 0;

    const ShaderConstant &el = out2Config.columns[elIdx];
    const BufferElementProperties &prop = out2Config.props[elIdx];

    m_Out2Position = m_Out2Data;
    m_Out2Position.vertexByteOffset += el.byteOffset;
    m_Out2Position.unproject = prop.systemValue == ShaderBuiltin::Position;

    elIdx = m_ModelOut2->secondaryColumn();

    if(elIdx >= 0 && elIdx < out2Config.columns.count())
    {
      m_Out2Secondary = m_Out2Data;
      m_Out2Secondary.vertexByteOffset += out2Config.columns[elIdx].byteOffset;
      m_Out2Secondary.showAlpha = m_ModelOut2->secondaryAlpha();
    }
  }
}

void BufferViewer::configureDrawRange()
{
  const ActionDescription *action = m_Ctx.CurAction();

  int curIndex = ui->drawRange->currentIndex();

  bool instanced = true;

  // don't check the flags, check if there are actually multiple instances
  if(m_Ctx.IsCaptureLoaded())
    instanced = action && action->numInstances > 1;

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

void BufferViewer::ApplyRowAndColumnDims(int numColumns, RDTableView *view, int dataColWidth)
{
  int start = 0;

  QList<int> widths;

  // vertex/element
  widths << m_IdxColWidth;

  // mesh view only - index
  if(m_MeshView)
    widths << m_IdxColWidth;

  for(int i = start; i < numColumns; i++)
    widths << dataColWidth;

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
      m_Config.position = m_InPosition;
      m_Config.second = m_InSecondary;
      break;
    case MeshDataStage::VSOut:
      m_Config.position = m_Out1Position;
      m_Config.second = m_Out1Secondary;
      break;
    case MeshDataStage::GSOut:
    case MeshDataStage::MeshOut:
      m_Config.position = m_Out2Position;
      m_Config.second = m_Out2Secondary;
      break;
    case MeshDataStage::TaskOut:
    default: break;
  }

  camGuess_changed(0.0);

  m_Config.showBBox = false;

  if(m_CurStage == MeshDataStage::TaskOut)
    return;

  BufferItemModel *model = currentBufferModel();
  int stage = currentStageIndex();

  if(model)
  {
    int posEl = model->posColumn();
    if(posEl >= 0 && posEl < model->getConfig().columns.count() &&
       posEl < bbox.bounds[stage].Min.count())
    {
      m_Config.minBounds = bbox.bounds[stage].Min[posEl];
      m_Config.maxBounds = bbox.bounds[stage].Max[posEl];
      m_Config.showBBox = !isCurrentRasterOut();

      int compCount = model->getConfig().columns[posEl].type.columns;

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
    QPointer<BufferViewer> me(this);

    m_Ctx.Replay().AsyncInvoke(lit("PickVertex"), [this, me, curpos](IReplayController *r) {
      if(!me)
        return;

      uint32_t instanceSelected = 0;
      uint32_t vertSelected = 0;

      rdctie(vertSelected, instanceSelected) =
          m_Output->PickVertex((uint32_t)curpos.x(), (uint32_t)curpos.y());

      if(vertSelected != ~0U)
      {
        if(!me)
          return;

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

void BufferViewer::ScrollToColumn(RDTableView *view, int column)
{
  int vs = view->verticalScrollBar()->value();

  view->scrollTo(view->model()->index(0, column), QAbstractItemView::PositionAtTop);

  view->verticalScrollBar()->setValue(vs);
}

void BufferViewer::ShowMeshData(MeshDataStage stage)
{
  const ActionDescription *action = m_Ctx.CurAction();
  if(action && (action->flags & ActionFlags::MeshDispatch) && stage == MeshDataStage::VSIn)
  {
    ToolWindowManager::raiseToolWindow(m_Containers[2]);
    return;
  }

  if(stage == MeshDataStage::VSIn)
    ToolWindowManager::raiseToolWindow(m_Containers[0]);
  else if(stage == MeshDataStage::VSOut)
    ToolWindowManager::raiseToolWindow(m_Containers[1]);
  else if(stage == MeshDataStage::GSOut)
    ToolWindowManager::raiseToolWindow(m_Containers[2]);
  else if(stage == MeshDataStage::TaskOut)
    ToolWindowManager::raiseToolWindow(m_Containers[0]);
  else if(stage == MeshDataStage::MeshOut)
    ToolWindowManager::raiseToolWindow(m_Containers[1]);
}

void BufferViewer::SetCurrentInstance(int32_t instance)
{
  if(ui->instance->isVisible() && ui->instance->isEnabled())
    ui->instance->setValue(instance);
}

void BufferViewer::SetCurrentView(int32_t view)
{
  if(ui->viewIndex->isVisible() && ui->viewIndex->isEnabled())
    ui->viewIndex->setValue(view);
}

void BufferViewer::SetPreviewStage(MeshDataStage stage)
{
  if(m_MeshView)
  {
    if(stage == MeshDataStage::VSIn)
      ui->outputTabs->setCurrentIndex(0);
    else if(stage == MeshDataStage::VSOut)
      ui->outputTabs->setCurrentIndex(1);
    else if(stage == MeshDataStage::GSOut)
      ui->outputTabs->setCurrentIndex(2);
    else if(stage == MeshDataStage::TaskOut)
      ui->outputTabs->setCurrentIndex(1);
    else if(stage == MeshDataStage::MeshOut)
      ui->outputTabs->setCurrentIndex(2);
  }
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
  m_TexSub = {0, 0, 0};

  updateLabelsAndLayout();

  BufferDescription *buf = m_Ctx.GetBuffer(id);
  if(buf)
    m_ObjectByteSize = buf->length;

  m_PagingByteOffset = 0;

  ui->formatSpecifier->setAutoFormat(format);
}

BufferViewer *BufferViewer::HasCBufferView(ShaderStage stage, uint32_t slot, uint32_t idx)
{
  CBufferSlot cbuffer = {stage, slot, idx};

  for(BufferViewer *c : m_CBufferViews)
  {
    if(c->m_CBufferSlot == cbuffer)
      return c;
  }

  return NULL;
}

BufferViewer *BufferViewer::GetFirstCBufferView(BufferViewer *exclude)
{
  for(BufferViewer *b : m_CBufferViews)
  {
    if(b != exclude)
      return b;
  }

  return NULL;
}

void BufferViewer::ViewCBuffer(const ShaderStage stage, uint32_t slot, uint32_t idx)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  m_IsBuffer = true;
  m_ByteOffset = 0;
  m_ByteSize = UINT64_MAX;
  m_BufferID = ResourceId();
  m_CBufferSlot = {stage, slot, idx};
  m_TexSub = {0, 0, 0};

  updateLabelsAndLayout();

  m_ObjectByteSize = 0;
  m_PagingByteOffset = 0;

  // enable the button to toggle on formatting, so we can pre-fill with a sensible format when it's
  // enabled
  ui->setFormat->setVisible(true);

  ui->formatSpecifier->setFormat(QString());
  ui->formatSpecifier->setVisible(false);
  ui->formatSpecifier->setAutoFormat(QString());

  m_CBufferViews.push_back(this);
}

void BufferViewer::ViewTexture(ResourceId id, const Subresource &sub, const rdcstr &format)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  m_IsBuffer = false;
  m_ByteOffset = 0;
  m_ByteSize = UINT64_MAX;
  m_BufferID = id;
  m_TexSub = sub;

  updateLabelsAndLayout();

  TextureDescription *tex = m_Ctx.GetTexture(id);
  if(tex)
  {
    m_ObjectByteSize = tex->byteSize;

    if(m_TexSub.sample == ~0U)
      m_TexSub.sample = tex->msSamp - 1;
  }

  m_PagingByteOffset = 0;

  ui->formatSpecifier->setAutoFormat(format);
}

void BufferViewer::ScrollToRow(int32_t row, MeshDataStage stage)
{
  if(m_MeshView && stage == MeshDataStage::TaskOut)
  {
    ui->fixedVars->scrollToItem(ui->fixedVars->topLevelItem(row));
    ui->fixedVars->setSelectedItem(ui->fixedVars->topLevelItem(row));
    return;
  }

  ScrollToRow(tableForStage(stage), row);

  if(m_MeshView)
    m_Scroll[(int)stage].setY(row);
  else
    // the row scroll is visible and handles paging in the non-mesh view, so use it
    ui->rowOffset->setValue(row);
}

void BufferViewer::ScrollToColumn(int32_t column, MeshDataStage stage)
{
  ScrollToColumn(tableForStage(stage), column);

  m_Scroll[(int)stage].setX(column);
}

bool BufferViewer::eventFilter(QObject *watched, QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    RDTreeWidget *tree = qobject_cast<RDTreeWidget *>(watched);
    if(tree)
    {
      RDTreeWidgetItem *item = tree->itemAt(tree->viewport()->mapFromGlobal(QCursor::pos()));
      if(item)
      {
        FixedVarTag tag = item->tag().value<FixedVarTag>();

        QString tooltip;

        Packing::Rules pack = m_ModelIn->getConfig().packing;

        if(tag.valid && tag.padding)
        {
          tooltip = tr("%1 bytes of padding. Packing rules in effect:\n\n")
                        .arg(Formatter::HumanFormat(tag.byteSize, Formatter::OffsetSize));

          if(pack == Packing::D3DCB)
            tooltip += tr("Standard D3D constant buffer packing.\n\n");
          else if(pack == Packing::std140)
            tooltip += tr("Standard std140 buffer packing.\n\n");
          else if(pack == Packing::std430)
            tooltip += tr("Standard std430 buffer packing.\n\n");
          else if(pack == Packing::C)
            tooltip += tr("Standard C / D3D UAV packing.\n\n");
          else if(pack == Packing::Scalar)
            tooltip += tr("Scalar packing.\n\n");

          if(pack.vector_align_component)
            tooltip +=
                tr("- Vectors are only aligned to their component (float4 to 4-byte boundary)\n");
          else
            tooltip +=
                tr("- 3- and 4-wide vectors must be aligned to a 4-wide boundary\n"
                   "  (vec3 and vec4 to 16-byte boundary)\n");

          if(pack.tight_arrays)
            tooltip += tr("- Arrays are tightly packed to each element\n");
          else
            tooltip += tr("- Arrays have a stride of a 16 bytes\n");

          if(pack.trailing_overlap)
            tooltip += tr("- Variables can overlap the trailing padding in arrays or structs.\n");
          else
            tooltip +=
                tr("- Variables must not overlap the trailing padding in arrays or structs.\n");

          if(pack.vector_straddle_16b)
            tooltip += tr("- Vectors can straddle 16-byte boundaries.\n");
          else
            tooltip += tr("- Vectors must not straddle 16-byte boundaries.\n");
        }
        else if(tag.valid && !tag.padding)
        {
          tooltip = tr("Variable %1 is at byte offset %2")
                        .arg(tag.name)
                        .arg(Formatter::HumanFormat(tag.byteOffset, Formatter::OffsetSize));

          if(!IsCBufferView())
            tooltip += tr(", not including overall base byte offset %1 in buffer")
                           .arg(Formatter::HumanFormat(m_ByteOffset, Formatter::OffsetSize));

          tooltip += lit(".");

          if(tag.matrix)
          {
            tooltip += tr("\n\nMatrix stored ");
            if(tag.rowmajor)
              tooltip += tr("row-major.");
            else
              tooltip += tr("column-major.");
          }
        }

        if(!tooltip.isEmpty())
        {
          QPoint pos = QCursor::pos();
          pos.setX(pos.x() + 10);
          pos.setY(pos.y() + 10);
          QToolTip::showText(pos, tooltip.trimmed());

          return true;
        }
      }
    }
    else if(!m_MeshView && watched == ui->inTable->viewport())
    {
      QModelIndex index =
          ui->inTable->indexAt(ui->inTable->viewport()->mapFromGlobal(QCursor::pos()));

      if(index.isValid())
      {
        const ShaderConstant &c = m_ModelIn->elementForColumn(index.column());

        QModelIndex rowidx = m_ModelIn->index(index.row(), 0, index.parent());
        int row = m_ModelIn->data(rowidx).toInt();

        size_t stride = m_ModelIn->getConfig().buffers[0]->stride;

        QString tooltip;

        tooltip =
            tr("%1 at overall byte offset %2")
                .arg(c.name)
                .arg(Formatter::HumanFormat(stride * row + c.byteOffset, Formatter::OffsetSize));
        tooltip += tr(", not including overall base byte offset %1 in buffer")
                       .arg(Formatter::HumanFormat(m_ByteOffset, Formatter::OffsetSize));

        tooltip += lit(".\n\n");

        tooltip +=
            tr("Row %1 begins at offset %2 (stride of %3 bytes)\n%4 is at offset %5 in each row.")
                .arg(row)
                .arg(Formatter::HumanFormat(stride * row, Formatter::OffsetSize))
                .arg(Formatter::HumanFormat(stride, Formatter::OffsetSize))
                .arg(c.name)
                .arg(Formatter::HumanFormat(c.byteOffset, Formatter::OffsetSize));

        QPoint pos = QCursor::pos();
        pos.setX(pos.x() + 10);
        pos.setY(pos.y() + 10);
        QToolTip::showText(pos, tooltip.trimmed());

        return true;
      }
    }
  }
  else if(!m_MeshView && watched == ui->inTable->viewport())
  {
    if(event->type() == QEvent::MouseMove)
    {
      bool ret = QObject::eventFilter(watched, event);

      QMouseEvent *mouseEvent = (QMouseEvent *)event;

      if(m_delegate->linkHover(mouseEvent, font(),
                               ui->inTable->indexAt(mouseEvent->localPos().toPoint())))
        ui->inTable->setCursor(QCursor(Qt::PointingHandCursor));
      else
        ui->inTable->unsetCursor();

      return ret;
    }
  }

  return QObject::eventFilter(watched, event);
}

void BufferViewer::updateLabelsAndLayout()
{
  if(m_MeshView)
  {
    setWindowTitle(tr("Mesh Viewer"));

    if(m_Ctx.IsCaptureLoaded())
    {
      GraphicsAPI pipeType = m_Ctx.APIProps().pipelineType;

      if(isMeshDraw())
      {
        m_Containers[0]->layout()->addWidget(ui->out1Table);
        m_Containers[0]->layout()->addWidget(ui->fixedVars);
        m_Containers[1]->layout()->addWidget(ui->out2Table);
        m_Containers[2]->layout()->addWidget(ui->inTable);

        ui->instanceLabel->setVisible(false);
        ui->instance->setVisible(false);
        ui->meshFilterLabel->setVisible(true);
        ui->resetMeshFilterButton->setVisible(true);

        ui->fixedVars->setVisible(true);
        ui->out1Table->setVisible(false);
        m_Containers[2]->setWindowTitle(tr("Mesh Input"));
        m_Containers[0]->setWindowTitle(IsD3D(pipeType) ? tr("Amp. Out") : tr("Task Out"));
        m_Containers[1]->setWindowTitle(tr("Mesh Output"));

        if(ui->outputTabs->indexOf(ui->out1Tab) == 1)
          ui->outputTabs->removeTab(1);
        ui->outputTabs->setTabText(0, tr("Mesh Input"));
        ui->outputTabs->setTabText(1, tr("Mesh Out"));

        if(ui->visualisation->itemText(ui->visualisation->count() - 1) != tr("Meshlet"))
          ui->visualisation->addItem(tr("Meshlet"));
        ui->visualisation->adjustSize();
      }
      else
      {
        m_Containers[0]->layout()->addWidget(ui->inTable);
        m_Containers[0]->layout()->addWidget(ui->fixedVars);
        m_Containers[1]->layout()->addWidget(ui->out1Table);
        m_Containers[2]->layout()->addWidget(ui->out2Table);

        ui->instanceLabel->setVisible(true);
        ui->instance->setVisible(true);
        ui->meshFilterLabel->setVisible(false);
        ui->resetMeshFilterButton->setVisible(false);

        ui->fixedVars->setVisible(false);
        ui->out1Table->setVisible(true);
        m_Containers[0]->setWindowTitle(tr("VS Input"));
        m_Containers[1]->setWindowTitle(tr("VS Output"));
        m_Containers[2]->setWindowTitle(tr("GS/DS Output"));

        ui->outputTabs->setTabText(0, tr("VS In"));
        if(ui->outputTabs->indexOf(ui->out1Tab) < 0)
          ui->outputTabs->insertTab(1, ui->out1Tab, tr("VS Out"));
        ui->outputTabs->setTabText(1, tr("VS Out"));
        ui->outputTabs->setTabText(2, tr("GS/DS Out"));

        if(ui->visualisation->itemText(ui->visualisation->count() - 1) == tr("Meshlet"))
          ui->visualisation->removeItem(ui->visualisation->count() - 1);
        ui->visualisation->adjustSize();
      }
    }
    else
    {
      m_Containers[0]->layout()->addWidget(ui->inTable);
      m_Containers[0]->layout()->addWidget(ui->fixedVars);
      m_Containers[1]->layout()->addWidget(ui->out1Table);
      m_Containers[2]->layout()->addWidget(ui->out2Table);

      ui->instanceLabel->setVisible(true);
      ui->instance->setVisible(true);
      ui->meshFilterLabel->setVisible(false);
      ui->resetMeshFilterButton->setVisible(false);

      ui->fixedVars->setVisible(false);
      ui->out1Table->setVisible(true);
      m_Containers[0]->setWindowTitle(tr("VS Input"));
      m_Containers[1]->setWindowTitle(tr("VS Output"));
      m_Containers[2]->setWindowTitle(tr("GS/DS Output"));

      ui->outputTabs->setTabText(0, tr("VS In"));
      if(ui->outputTabs->indexOf(ui->out1Tab) < 0)
        ui->outputTabs->insertTab(1, ui->out1Tab, tr("VS Out"));
      ui->outputTabs->setTabText(1, tr("VS Out"));
      ui->outputTabs->setTabText(2, tr("GS/DS Out"));

      if(ui->visualisation->itemText(ui->visualisation->count() - 1) == tr("Meshlet"))
        ui->visualisation->removeItem(ui->visualisation->count() - 1);
      ui->visualisation->adjustSize();
    }
  }
  else
  {
    if(IsCBufferView())
    {
      QString bufName;

      const ShaderReflection *reflection =
          m_Ctx.CurPipelineState().GetShaderReflection(m_CBufferSlot.stage);

      uint32_t arraySize = ~0U;
      if(reflection != NULL)
      {
        if(m_CBufferSlot.slot < reflection->constantBlocks.size() &&
           !reflection->constantBlocks[m_CBufferSlot.slot].name.isEmpty())
        {
          bufName = QFormatStr("<%1>").arg(reflection->constantBlocks[m_CBufferSlot.slot].name);
          arraySize = reflection->constantBlocks[m_CBufferSlot.slot].bindArraySize;
        }
      }

      if(bufName.isEmpty())
      {
        if(m_BufferID != ResourceId())
          bufName = m_Ctx.GetResourceName(m_BufferID);
        else
          bufName = tr("Unbound");
      }

      GraphicsAPI pipeType = m_Ctx.APIProps().pipelineType;

      QString title = QFormatStr("%1 %2 %3")
                          .arg(ToQStr(m_CBufferSlot.stage, pipeType))
                          .arg(IsD3D(pipeType) ? lit("CB") : lit("UBO"))
                          .arg(m_CBufferSlot.slot);

      if(m_Ctx.CurPipelineState().SupportsResourceArrays() && arraySize > 1)
        title += QFormatStr("[%1]").arg(m_CBufferSlot.arrayIdx);

      title += QFormatStr(" - %1").arg(bufName);

      setWindowTitle(title);
    }
    else
    {
      setWindowTitle(m_Ctx.GetResourceName(m_BufferID) + lit(" - Contents"));
    }
  }
}

void BufferViewer::on_resourceDetails_clicked()
{
  if(m_BufferID == ResourceId())
    return;

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

QPushButton *BufferViewer::MakePreviousPageButton()
{
  QPushButton *b = new QPushButton(tr("Prev Page"), this);
  QObject::connect(b, &QPushButton::clicked, [this] {
    int page = ui->rowOffset->value() / MaxVisibleRows;

    if(page > 0)
      ui->rowOffset->setValue((page - 1) * MaxVisibleRows);
  });
  return b;
}

QPushButton *BufferViewer::MakeNextPageButton()
{
  QPushButton *b = new QPushButton(tr("Next Page"), this);
  QObject::connect(b, &QPushButton::clicked, [this] {
    int page = ui->rowOffset->value() / MaxVisibleRows;

    ui->rowOffset->setValue((page + 1) * MaxVisibleRows);
  });
  return b;
}

RDTableView *BufferViewer::tableForStage(MeshDataStage stage)
{
  if(stage == MeshDataStage::VSIn)
    return ui->inTable;
  else if(stage == MeshDataStage::VSOut)
    return ui->out1Table;
  else if(stage == MeshDataStage::GSOut)
    return ui->out2Table;
  else if(stage == MeshDataStage::TaskOut)
    return ui->out1Table;
  else if(stage == MeshDataStage::MeshOut)
    return ui->out2Table;

  return NULL;
}

BufferItemModel *BufferViewer::modelForStage(MeshDataStage stage)
{
  if(stage == MeshDataStage::VSIn)
    return m_ModelIn;
  else if(stage == MeshDataStage::VSOut)
    return m_ModelOut1;
  else if(stage == MeshDataStage::GSOut)
    return m_ModelOut2;
  else if(stage == MeshDataStage::TaskOut)
    return m_ModelOut1;
  else if(stage == MeshDataStage::MeshOut)
    return m_ModelOut2;

  return NULL;
}

bool BufferViewer::isCurrentRasterOut()
{
  BufferItemModel *model = currentBufferModel();

  // if geometry/tessellation is enabled, only the GS out stage is rasterized output
  if((m_Ctx.CurPipelineState().GetShader(ShaderStage::Tess_Eval) != ResourceId() ||
      m_Ctx.CurPipelineState().GetShader(ShaderStage::Geometry) != ResourceId()) &&
     m_CurStage != MeshDataStage::GSOut)
    return false;

  // task shader outputs are not rasterized by definition
  if(m_CurStage == MeshDataStage::TaskOut)
    return false;

  if(model)
  {
    int posEl = model->posColumn();
    if(posEl >= 0 && posEl < model->getConfig().columns.count())
    {
      return model->getConfig().props[posEl].systemValue == ShaderBuiltin::Position;
    }

    // if the model isn't prepared yet then return a sensible default answer - if no tess/geom,
    // vertex is the output. Otherwise geom is the output. For task/mesh then mesh is the output
    if(model->getConfig().columns.empty())
    {
      if(m_Ctx.CurPipelineState().GetShader(ShaderStage::Tess_Eval) != ResourceId() ||
         m_Ctx.CurPipelineState().GetShader(ShaderStage::Geometry) != ResourceId())
        return m_CurStage == MeshDataStage::GSOut;
      else if(m_CurStage == MeshDataStage::MeshOut)
        return true;
      else if(m_Ctx.CurPipelineState().GetShader(ShaderStage::Tess_Eval) == ResourceId() &&
              m_Ctx.CurPipelineState().GetShader(ShaderStage::Geometry) == ResourceId() &&
              m_CurStage != MeshDataStage::VSOut)
        return true;
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
  else if(m_CurStage == MeshDataStage::TaskOut)
    return 1;
  else if(m_CurStage == MeshDataStage::MeshOut)
    return 2;

  return 0;
}

bool BufferViewer::isMeshDraw()
{
  const ActionDescription *action = m_Ctx.CurAction();

  return action && action->flags & ActionFlags::MeshDispatch;
}

void BufferViewer::Reset()
{
  m_Output = NULL;

  configureDrawRange();

  ClearModels();

  updateLabelsAndLayout();
  SetMeshFilter(MeshFilter::None);

  ui->fixedVars->clear();

  ui->inTable->setColumnWidths({40, 40});
  ui->out1Table->setColumnWidths({40, 40});
  ui->out2Table->setColumnWidths({40, 40});

  m_BBoxes.clear();
}

void BufferViewer::ClearModels()
{
  for(BufferItemModel *m : {m_ModelIn, m_ModelOut1, m_ModelOut2})
  {
    if(!m)
      continue;

    m->beginReset();
    m->endReset(BufferConfiguration());
  }
}

void BufferViewer::CalcColumnWidth(int maxNumRows)
{
  // while the calculated column widths aren't actually isn't quite based on maxNumRows, it can only
  // be affected by a style change so that is good enough for us to cache it and save time
  // recalculating this repeatedly.
  if(m_ColumnWidthRowCount == maxNumRows)
    return;

  m_ColumnWidthRowCount = maxNumRows;

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

  BufferElementProperties floatProp, intProp;
  floatProp.format = floatFmt;
  intProp.format = intFmt;

  ShaderConstant elem;
  elem.name = headerText;
  elem.byteOffset = 0;
  elem.type.rows = maxNumRows;
  elem.type.columns = 1;

  bufconfig.columns.clear();

  bufconfig.columns.push_back(elem);
  bufconfig.props.push_back(floatProp);

  elem.type.rows = 1;
  elem.byteOffset = 4;

  bufconfig.columns.push_back(elem);
  bufconfig.props.push_back(floatProp);

  elem.byteOffset = 8;

  bufconfig.columns.push_back(elem);
  bufconfig.props.push_back(floatProp);

  elem.byteOffset = 12;

  bufconfig.columns.push_back(elem);
  bufconfig.props.push_back(intProp);

  elem.byteOffset = 16;

  bufconfig.columns.push_back(elem);
  bufconfig.props.push_back(intProp);

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

  m_ModelIn->beginReset();

  m_ModelIn->endReset(bufconfig);

  // measure this data so we can use this as column widths
  ui->inTable->resizeColumnsToContents();

  // index/element column
  m_IdxColWidth = ui->inTable->columnWidth(0);

  int col = 1;
  if(m_MeshView)
    col = 2;

  m_DataColWidth = 10;
  for(int c = 0; c < 5; c++)
  {
    int colWidth = ui->inTable->columnWidth(col + c);
    m_DataColWidth = qMax(m_DataColWidth, colWidth);
  }

  ui->inTable->resizeRowsToContents();

  m_DataRowHeight = ui->inTable->rowHeight(0);
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
  m_CurFixed = false;

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

  float vpWidth = qAbs(vp.width);
  float vpHeight = qAbs(vp.height);

  m_Config.aspect = (vpWidth > 0.0f && vpHeight > 0.0f) ? (vpWidth / vpHeight) : 1.0f;

  if(ui->aspectGuess->value() > 0.0)
    m_Config.aspect = ui->aspectGuess->value();

  // use estimates from post vs data (calculated from vertex position data) if the user
  // hasn't overridden the values
  m_Config.position.nearPlane = 0.1f;
  m_Config.position.flipY = false;

  if(m_CurStage == MeshDataStage::VSOut)
  {
    m_Config.position.nearPlane = m_Out1Data.nearPlane;
    m_Config.position.flipY = m_Out1Data.flipY;
  }
  else if(m_CurStage == MeshDataStage::GSOut)
  {
    m_Config.position.nearPlane = m_Out2Data.nearPlane;
    m_Config.position.flipY = m_Out2Data.flipY;
  }
  else if(m_CurStage == MeshDataStage::TaskOut)
  {
    m_Config.position.nearPlane = m_Out1Data.nearPlane;
    m_Config.position.flipY = m_Out1Data.flipY;
  }
  else if(m_CurStage == MeshDataStage::MeshOut)
  {
    m_Config.position.nearPlane = m_Out2Data.nearPlane;
    m_Config.position.flipY = m_Out2Data.flipY;
  }

  if(ui->nearGuess->value() > 0.0)
    m_Config.position.nearPlane = ui->nearGuess->value();

  m_Config.position.farPlane = 100.0f;

  if(m_CurStage == MeshDataStage::VSOut)
    m_Config.position.farPlane = m_Out1Data.farPlane;
  else if(m_CurStage == MeshDataStage::GSOut)
    m_Config.position.farPlane = m_Out2Data.farPlane;
  else if(m_CurStage == MeshDataStage::TaskOut)
    m_Config.position.farPlane = m_Out1Data.farPlane;
  else if(m_CurStage == MeshDataStage::MeshOut)
    m_Config.position.farPlane = m_Out2Data.farPlane;

  if(ui->farGuess->value() > 0.0)
    m_Config.position.farPlane = ui->farGuess->value();

  EnableCameraGuessControls();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_axisMappingCombo_currentIndexChanged(int index)
{
  if(index != 4)
  {
    switch(index)
    {
      case 0:    // Y-up, Left Handed
        m_Config.axisMapping.xAxis = FloatVector(1.0f, 0.0f, 0.0f, 0.0f);
        m_Config.axisMapping.yAxis = FloatVector(0.0f, 1.0f, 0.0f, 0.0f);
        m_Config.axisMapping.zAxis = FloatVector(0.0f, 0.0f, 1.0f, 0.0f);
        break;
      case 1:    // Y-up, Right Handed
        m_Config.axisMapping.xAxis = FloatVector(1.0f, 0.0f, 0.0f, 0.0f);
        m_Config.axisMapping.yAxis = FloatVector(0.0f, 1.0f, 0.0f, 0.0f);
        m_Config.axisMapping.zAxis = FloatVector(0.0f, 0.0f, -1.0f, 0.0f);
        break;
      case 2:    // Z-up, Left Handed
        m_Config.axisMapping.xAxis = FloatVector(1.0f, 0.0f, 0.0f, 0.0f);
        m_Config.axisMapping.yAxis = FloatVector(0.0f, 0.0f, -1.0f, 0.0f);
        m_Config.axisMapping.zAxis = FloatVector(0.0f, 1.0f, 0.0f, 0.0f);
        break;
      case 3:    // Z-up, Right Handed
        m_Config.axisMapping.xAxis = FloatVector(1.0f, 0.0f, 0.0f, 0.0f);
        m_Config.axisMapping.yAxis = FloatVector(0.0f, 0.0f, 1.0f, 0.0f);
        m_Config.axisMapping.zAxis = FloatVector(0.0f, 1.0f, 0.0f, 0.0f);
        break;
      default: break;
    }
    ui->axisMappingButton->setEnabled(false);
    previousAxisMappingIndex = index;
    on_resetCamera_clicked();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
  }
  else
  {
    ui->axisMappingButton->setEnabled(true);
    if(previousAxisMappingIndex != 4)
    {
      bool validConfig = showAxisMappingDialog();

      if(!validConfig)
      {
        ui->axisMappingCombo->setCurrentIndex(previousAxisMappingIndex);
        ui->axisMappingButton->setEnabled(false);
      }
    }
  }
}

bool BufferViewer::showAxisMappingDialog()
{
  AxisMappingDialog dialog(m_Ctx, m_Config, this);
  RDDialog::show(&dialog);

  if(dialog.result() == QDialog::Accepted)
  {
    m_Config.axisMapping = dialog.getAxisMapping();
    on_resetCamera_clicked();
    INVOKE_MEMFN(RT_UpdateAndDisplay);
    return true;
  }
  return false;
}

void BufferViewer::on_axisMappingButton_clicked()
{
  showAxisMappingDialog();
}

void BufferViewer::on_setFormat_toggled(bool checked)
{
  if(!checked)
  {
    ui->formatSpecifier->setVisible(false);

    processFormat(QString());
    return;
  }

  ui->formatSpecifier->setVisible(true);

  const ShaderReflection *reflection =
      m_Ctx.CurPipelineState().GetShaderReflection(m_CBufferSlot.stage);

  if(m_CBufferSlot.slot >= reflection->constantBlocks.size())
  {
    ui->formatSpecifier->setVisible(false);

    processFormat(QString());
    return;
  }

  if(IsD3D(m_Ctx.APIProps().pipelineType))
    ui->formatSpecifier->setAutoFormat(BufferFormatter::DeclareStruct(
        Packing::D3DCB, reflection->resourceId, reflection->constantBlocks[m_CBufferSlot.slot].name,
        reflection->constantBlocks[m_CBufferSlot.slot].variables, 0));
  else
    ui->formatSpecifier->setAutoFormat(BufferFormatter::DeclareStruct(
        BufferFormatter::EstimatePackingRules(
            reflection->resourceId, reflection->constantBlocks[m_CBufferSlot.slot].variables),
        reflection->resourceId, reflection->constantBlocks[m_CBufferSlot.slot].name,
        reflection->constantBlocks[m_CBufferSlot.slot].variables, 0));
}

void BufferViewer::on_resetMeshFilterButton_clicked()
{
  SetMeshFilter(MeshFilter::None);
}

void BufferViewer::processFormat(const QString &format)
{
  // save scroll values now before we reset all the models
  m_Scrolls = new PopulateBufferData;
  FillScrolls(m_Scrolls);

  Reset();

  BufferConfiguration bufconfig;

  ParsedFormat parsed;

  if(IsCBufferView() && format.isEmpty())
  {
    // insert a dummy member so we get identified as plain fixed vars - we will automatically
    // evaluate ignoring the format
    parsed.fixed.type.members.push_back(ShaderConstant());
  }
  else
  {
    parsed = BufferFormatter::ParseFormatString(format, m_ByteSize, IsCBufferView());
  }

  const bool repeatedVars = parsed.repeating.type.baseType != VarType::Unknown;
  const bool fixedVars = !parsed.fixed.type.members.empty();

  if(fixedVars && repeatedVars)
  {
    if(m_OuterSplitter->widget(0) != m_InnerSplitter)
      m_OuterSplitter->replaceWidget(0, m_InnerSplitter);

    m_FixedGroup->layout()->addWidget(ui->fixedVars);
    m_RepeatedGroup->layout()->addWidget(ui->inTable);

    // row offset should be shown in the repeated control bar, but no separator line is needed
    ui->offsetLine->setVisible(false);
    ui->rowOffsetLabel->setVisible(true);
    ui->rowOffset->setVisible(true);
    if(ui->rowOffset->parentWidget() != m_RepeatedControlBar)
    {
      QHBoxLayout *hbox = qobject_cast<QHBoxLayout *>(m_RepeatedControlBar->layout());
      hbox->insertWidget(0, ui->rowOffsetLabel);
      hbox->insertWidget(1, ui->rowOffset);
    }
    ui->fixedVars->setVisible(true);
    ui->inTable->setVisible(true);

    ui->showPadding->setVisible(true);

    m_InnerSplitter->setVisible(true);

    if(m_CurView == NULL && !m_CurFixed)
      m_CurView = ui->inTable;
  }
  else if(fixedVars)
  {
    if(m_OuterSplitter->widget(0) != ui->fixedVars)
      m_OuterSplitter->replaceWidget(0, ui->fixedVars);

    // row offset should not be shown
    ui->offsetLine->setVisible(false);
    ui->rowOffsetLabel->setVisible(false);
    ui->rowOffset->setVisible(false);

    ui->fixedVars->setVisible(true);
    ui->inTable->setVisible(false);

    ui->showPadding->setVisible(true);

    m_InnerSplitter->setVisible(false);

    m_CurView = NULL;
    m_CurFixed = true;
  }
  else if(repeatedVars)
  {
    if(m_OuterSplitter->widget(0) != ui->inTable)
      m_OuterSplitter->replaceWidget(0, ui->inTable);

    // row offset should be shown with the other controls
    ui->offsetLine->setVisible(true);
    ui->rowOffsetLabel->setVisible(true);
    ui->rowOffset->setVisible(true);
    // insert after the offsetLine
    if(ui->rowOffset->parentWidget() != ui->meshToolbar)
    {
      QHBoxLayout *hbox = qobject_cast<QHBoxLayout *>(ui->meshToolbar->layout());

      int i = 0;
      for(; i < hbox->count(); i++)
      {
        if(hbox->itemAt(i)->widget() == ui->offsetLine)
          break;
      }
      i++;
      if(i < hbox->count())
      {
        hbox->insertWidget(i, ui->rowOffset);
        hbox->insertWidget(i, ui->rowOffsetLabel);
      }
    }

    ui->fixedVars->setVisible(false);
    ui->inTable->setVisible(true);

    ui->showPadding->setVisible(false);

    m_InnerSplitter->setVisible(false);

    m_CurView = ui->inTable;
    m_CurFixed = false;
  }

  CalcColumnWidth(MaxNumRows(parsed.repeating));

  ClearModels();

  m_Format = format;

  if(IsCBufferView())
  {
    ui->byteRangeLine->setVisible(false);
    ui->byteRangeStartLabel->setVisible(false);
    byteRangeStart->setVisible(false);
    ui->byteRangeLengthLabel->setVisible(false);
    byteRangeLength->setVisible(false);
    GraphicsAPI pipeType = m_Ctx.APIProps().pipelineType;

    if(IsD3D(pipeType))
      ui->formatSpecifier->setTitle(tr("Constant Buffer Custom Format"));
    else
      ui->formatSpecifier->setTitle(tr("Uniform Buffer Custom Format"));
  }
  else
  {
    qulonglong stride = qMax(1U, parsed.repeating.type.arrayByteStride);

    byteRangeStart->setSingleStep(stride);
    byteRangeLength->setSingleStep(stride);

    byteRangeStart->setMaximum((qulonglong)m_ObjectByteSize);
    byteRangeLength->setMaximum((qulonglong)m_ObjectByteSize);

    byteRangeStart->setValue(m_ByteOffset);
    byteRangeLength->setValue(m_ByteSize);

    if(!m_IsBuffer)
    {
      byteRangeStart->setVisible(false);

      TextureDescription *tex = m_Ctx.GetTexture(m_BufferID);
      if(tex)
      {
        if(tex->arraysize == 1 && tex->mips == 1 && tex->msSamp == 1 && tex->depth == 1)
        {
          ui->byteRangeStartLabel->setVisible(false);
        }
        else
        {
          QString text;

          if(tex->arraysize > 1 || tex->depth > 1)
            text = tr("Slice %1").arg(m_TexSub.slice);

          if(tex->mips > 1)
          {
            if(!text.isEmpty())
              text += lit(", ");
            text += tr("Mip %1").arg(m_TexSub.mip);
          }

          if(tex->msSamp > 1)
          {
            if(!text.isEmpty())
              text += lit(", ");
            text += tr("Sample %1").arg(m_TexSub.sample);
          }

          text += lit(". ");

          ui->byteRangeStartLabel->setText(text);
        }
      }
      byteRangeLength->setEnabled(false);
    }
  }

  ui->formatSpecifier->setErrors(parsed.errors);

  OnEventChanged(m_Ctx.CurEvent());
}

void BufferViewer::on_byteRangeStart_valueChanged(double value)
{
  m_ByteOffset = RDSpinBox64::getUValue(value);

  m_PagingByteOffset = 0;

  processFormat(m_Format);
}

void BufferViewer::on_byteRangeLength_valueChanged(double value)
{
  m_ByteSize = RDSpinBox64::getUValue(value);

  m_PagingByteOffset = 0;

  processFormat(m_Format);
}

void BufferViewer::updateExportActionNames()
{
  QString csv = tr("Export%1 to &CSV");
  QString bytes = tr("Export%1 to &Bytes");

  bool valid = m_Ctx.IsCaptureLoaded() && m_Ctx.CurAction();

  if(m_MeshView)
  {
    valid = valid && m_CurView != NULL;
  }
  else
  {
    valid = valid && (m_CurView != NULL || m_CurFixed);
  }

  if(!valid)
  {
    m_ExportCSV->setText(csv.arg(QString()));
    m_ExportBytes->setText(bytes.arg(QString()));
    m_ExportCSV->setEnabled(false);
    m_ExportBytes->setEnabled(false);
    return;
  }

  m_ExportCSV->setEnabled(true);
  m_ExportBytes->setEnabled(m_BufferID != ResourceId());

  if(m_MeshView)
  {
    m_ExportCSV->setText(csv.arg(lit(" ") + m_CurView->windowTitle()));
    m_ExportBytes->setText(bytes.arg(lit(" ") + m_CurView->windowTitle()));
    m_ExportBytes->setEnabled(true);
  }
  else
  {
    // if only one type of data is visible, the export is unambiguous
    if(!ui->inTable->isVisible() || !ui->fixedVars->isVisible())
    {
      m_ExportCSV->setText(csv.arg(QString()));
      m_ExportBytes->setText(bytes.arg(QString()));
    }
    // otherwise go by which is selected
    else if(m_CurFixed)
    {
      m_ExportCSV->setText(csv.arg(lit(" ") + m_FixedGroup->title()));
      m_ExportBytes->setText(bytes.arg(lit(" ") + m_FixedGroup->title()));
    }
    else
    {
      m_ExportCSV->setText(csv.arg(lit(" ") + m_RepeatedGroup->title()));
      m_ExportBytes->setText(bytes.arg(lit(" ") + m_RepeatedGroup->title()));
    }
  }
}

void BufferViewer::exportCSV(QTextStream &ts, const QString &prefix, RDTreeWidgetItem *item)
{
  if(item->childCount() == 0)
  {
    ts << QFormatStr("%1,\"%2\",%3,%4\n")
              .arg(item->text(0))
              .arg(item->text(1))
              .arg(item->text(2))
              .arg(item->text(3));
  }
  else
  {
    ts << QFormatStr("%1,,%2,%3\n").arg(item->text(0)).arg(item->text(2)).arg(item->text(3));
    for(int i = 0; i < item->childCount(); i++)
      exportCSV(ts, item->text(0) + lit("."), item->child(i));
  }
}

void BufferViewer::exportData(const BufferExport &params)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(!m_Ctx.CurAction())
    return;

  if(!m_CurView && !m_CurFixed)
    return;

  QString filter;
  QString title;
  if(params.format == BufferExport::CSV)
  {
    filter = tr("CSV Files (*.csv)");
    title = tr("Export buffer to CSV");
  }
  else if(params.format == BufferExport::RawBytes)
  {
    filter = tr("Binary Files (*.bin)");
    title = tr("Export buffer to bytes");
  }

  QString filename =
      RDDialog::getSaveFileName(this, title, QString(), tr("%1;;All files (*)").arg(filter));

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

  if(m_CurView)
  {
    BufferItemModel *model = (BufferItemModel *)m_CurView->model();

    LambdaThread *exportThread = new LambdaThread([this, params, model, f]() {
      if(params.format == BufferExport::RawBytes)
      {
        const BufferConfiguration &config = model->getConfig();

        if(!m_MeshView)
        {
          // this is the simplest possible case, we just dump the contents of the first buffer.
          if(!m_IsBuffer || config.buffers[0]->size() >= m_ByteSize)
          {
            f->write((const char *)config.buffers[0]->data(), int(config.buffers[0]->size()));
          }
          else
          {
            // For buffers we have to handle reading in pages though as we might not have everything
            // in memory.
            ResourceId buff = m_BufferID;

            static const uint64_t maxChunkSize = 4 * 1024 * 1024;
            for(uint64_t byteOffset = m_ByteOffset; byteOffset < m_ByteSize;
                byteOffset += maxChunkSize)
            {
              uint64_t chunkSize = qMin(m_ByteSize - byteOffset, maxChunkSize);

              // it's fine to block invoke, because this is on the export thread
              m_Ctx.Replay().BlockInvoke([buff, f, byteOffset, chunkSize](IReplayController *r) {
                bytebuf chunk = r->GetBufferData(buff, byteOffset, chunkSize);
                f->write((const char *)chunk.data(), (qint64)chunk.size());
              });
            }
          }
        }
        else
        {
          // cache column data for the inner loop
          QVector<CachedElData> cache;

          CacheDataForIteration(cache, config.columns, config.props, config.buffers,
                                config.curInstance);

          // go row by row, finding the start of the row and dumping out the elements using their
          // offset and sizes
          for(int i = 0; i < model->rowCount(); i++)
          {
            // manually calculate the index so that we get the real offset (not the displayed
            // offset)
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
              const ShaderConstant *el = d.el;
              const BufferElementProperties *prop = d.prop;

              if(d.data)
              {
                const char *bytes = (const char *)d.data;

                if(!prop->perinstance)
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
        // otherwise we need to iterate over all the data ourselves
        const BufferConfiguration &config = model->getConfig();

        QTextStream s(f);

        for(int i = 0; i < model->columnCount(); i++)
        {
          s << model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();

          if(i + 1 < model->columnCount())
            s << ", ";
        }

        s << "\n";

        if(m_MeshView || !m_IsBuffer || config.buffers[0]->size() >= m_ByteSize)
        {
          // if there's no pagination to worry about, dump using the model's data()
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
        else
        {
          // write 64k rows at a time
          ResourceId buff = m_BufferID;
          const uint64_t maxChunkSize = 64 * 1024 * config.buffers[0]->stride;
          for(uint64_t byteOffset = m_ByteOffset; byteOffset < m_ByteSize; byteOffset += maxChunkSize)
          {
            uint64_t chunkSize = qMin(m_ByteSize - byteOffset, maxChunkSize);

            // it's fine to block invoke, because this is on the export thread
            m_Ctx.Replay().BlockInvoke(
                [buff, &s, &config, byteOffset, chunkSize](IReplayController *r) {
                  // cache column data for the inner loop
                  QVector<CachedElData> cache;

                  BufferData bufferData;

                  bufferData.storage = r->GetBufferData(buff, byteOffset, chunkSize);
                  bufferData.stride = config.buffers[0]->stride;

                  size_t numRows =
                      (bufferData.storage.size() + bufferData.stride - 1) / bufferData.stride;
                  size_t rowOffset = byteOffset / bufferData.stride;

                  CacheDataForIteration(cache, config.columns, config.props, {&bufferData}, 0);

                  // go row by row, finding the start of the row and dumping out the elements using
                  // their
                  // offset and sizes
                  for(size_t idx = 0; idx < numRows; idx++)
                  {
                    s << (rowOffset + idx) << ", ";

                    for(int col = 0; col < cache.count(); col++)
                    {
                      const CachedElData &d = cache[col];
                      const ShaderConstant *el = d.el;
                      const BufferElementProperties *prop = d.prop;

                      if(d.data)
                      {
                        const byte *data = d.data;
                        const byte *end = d.end;

                        data += d.stride * idx;

                        // only slightly wasteful, we need to fetch all variants together
                        // since some formats are packed and can't be read individually
                        QVariantList list = GetVariants(prop->format, *el, data, end);

                        for(int v = 0; v < list.count(); v++)
                        {
                          s << interpretVariant(list[v], *el, *prop);

                          if(v + 1 < list.count())
                            s << ", ";
                        }

                        if(list.empty())
                        {
                          for(int v = 0; v < d.numColumns; v++)
                          {
                            s << "---";

                            if(v + 1 < d.numColumns)
                              s << ", ";
                          }
                        }

                        if(col + 1 < cache.count())
                          s << ", ";
                      }
                    }

                    s << "\n";
                  }
                });
          }
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
  else if(m_CurFixed)
  {
    if(params.format == BufferExport::RawBytes)
    {
      BufferItemModel *model = (BufferItemModel *)ui->inTable->model();
      const BufferConfiguration &config = model->getConfig();

      size_t byteSize = 0;

      if(!config.fixedVars.type.members.empty())
        byteSize = BufferFormatter::GetVarAdvance(config.packing, config.fixedVars);

      const bytebuf &bufdata = config.buffers[0]->storage;

      f->write((const char *)bufdata.data(), qMin(bufdata.size(), byteSize));

      // if the buffer wasn't large enough for the variables, fill with 0s
      if(byteSize > bufdata.size())
      {
        QByteArray nulls;
        nulls.resize(int(byteSize - config.buffers[0]->storage.size()));
        f->write(nulls);
      }
    }
    else if(params.format == BufferExport::CSV)
    {
      QTextStream ts(f);

      ts << tr("Name,Value,Byte Offset,Type\n");

      for(int i = 0; i < ui->fixedVars->topLevelItemCount(); i++)
        exportCSV(ts, QString(), ui->fixedVars->topLevelItem(i));
    }

    f->close();

    delete f;
  }
}

void BufferViewer::debugVertex()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(!m_Ctx.CurAction())
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
  uint32_t view = m_Config.curView;

  bool done = false;
  ShaderDebugTrace *trace = NULL;

  m_Ctx.Replay().AsyncInvoke([this, &done, &trace, vertid, index, view](IReplayController *r) {
    trace = r->DebugVertex(vertid, m_Config.curInstance, index, view);

    if(trace->debugger == NULL)
    {
      r->FreeTrace(trace);
      trace = NULL;
    }

    done = true;
  });

  QString debugContext = tr("Vertex %1").arg(vertid);

  if(m_Ctx.CurAction()->numInstances > 1)
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
  ResourceId pipeline = m_Ctx.CurPipelineState().GetGraphicsPipelineObject();

  // viewer takes ownership of the trace
  IShaderViewer *s = m_Ctx.DebugShader(shaderDetails, pipeline, trace, debugContext);

  m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
}

void BufferViewer::SyncViews(RDTableView *primary, bool selection, bool scroll)
{
  if(!ui->syncViews->isChecked())
    return;

  RDTableView *views[] = {ui->inTable, ui->out1Table, ui->out2Table};

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

  if(ui->highlightVerts->isHidden() || !ui->highlightVerts->isChecked())
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
  ui->matrixType->setEnabled(isCurrentRasterOut());
  ui->aspectGuess->setEnabled(isCurrentRasterOut());
  ui->nearGuess->setEnabled(isCurrentRasterOut());
  ui->farGuess->setEnabled(isCurrentRasterOut());

  // FOV is only available in perspective mode
  ui->fovGuess->setEnabled(isCurrentRasterOut() && ui->matrixType->currentIndex() == 0);
}

void BufferViewer::on_outputTabs_currentChanged(int index)
{
  ui->renderContainer->parentWidget()->layout()->removeWidget(ui->renderContainer);
  ui->outputTabs->widget(index)->layout()->addWidget(ui->renderContainer);

  if(index == 0)
    m_CurStage = MeshDataStage::VSIn;
  else if(index == 1)
    m_CurStage = isMeshDraw() ? MeshDataStage::MeshOut : MeshDataStage::VSOut;
  else if(index == 2)
    m_CurStage = MeshDataStage::GSOut;

  configureDrawRange();

  on_resetCamera_clicked();
  ui->autofitCamera->setEnabled(!isCurrentRasterOut());

  EnableCameraGuessControls();
  ui->axisMappingCombo->setEnabled(!isCurrentRasterOut());
  ui->axisMappingButton->setEnabled(!isCurrentRasterOut() &&
                                    ui->axisMappingCombo->currentIndex() == 4);

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

void BufferViewer::on_showPadding_toggled(bool checked)
{
  OnEventChanged(m_Ctx.CurEvent());
}

void BufferViewer::on_highlightVerts_toggled(bool checked)
{
  UpdateHighlightVerts();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_vtxExploderSlider_valueChanged(int value)
{
  m_Config.vtxExploderSliderSNorm = (float)value / 100.0f;

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_exploderReset_clicked()
{
  ui->vtxExploderSlider->setSliderPosition(0);
}

void BufferViewer::on_exploderScale_valueChanged(double value)
{
  m_Config.exploderScale = (float)value;

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_wireframeRender_toggled(bool checked)
{
  m_Config.wireframeDraw = checked;

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::on_visualisation_currentIndexChanged(int index)
{
  ui->wireframeRender->setEnabled(index > 0);

  if(!ui->wireframeRender->isEnabled())
  {
    ui->wireframeRender->setChecked(true);
    m_Config.wireframeDraw = true;
  }

  bool explodeHidden = (index != (int)Visualisation::Explode);
  ui->vtxExploderLabel->setHidden(explodeHidden);
  ui->vtxExploderSlider->setHidden(explodeHidden);
  ui->exploderReset->setHidden(explodeHidden);
  ui->exploderScaleLabel->setHidden(explodeHidden);
  ui->exploderScale->setHidden(explodeHidden);
  // Because the vertex/prim highlights draw from a new, temporary vertex buffer,
  // those vertex IDs (which determine the explode displacement) won't necessarily
  // match the original mesh's IDs and exploded vertices.  Because of this, it seems
  // cleanest to just avoid drawing the highlighted vert/prim with the explode
  // visualisation (while also getting back a little room on the toolbar used by
  // the extra exploder controls).
  ui->highlightVerts->setHidden(!explodeHidden);
  UpdateHighlightVerts();

  m_Config.visualisationMode = (Visualisation)qMax(0, index);

  m_ModelIn->setSecondaryColumn(m_ModelIn->secondaryColumn(),
                                m_Config.visualisationMode == Visualisation::Secondary,
                                m_ModelIn->secondaryAlpha());
  m_ModelOut1->setSecondaryColumn(m_ModelOut1->secondaryColumn(),
                                  m_Config.visualisationMode == Visualisation::Secondary,
                                  m_ModelOut1->secondaryAlpha());
  m_ModelOut2->setSecondaryColumn(m_ModelOut2->secondaryColumn(),
                                  m_Config.visualisationMode == Visualisation::Secondary,
                                  m_ModelOut2->secondaryAlpha());

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

void BufferViewer::SetMeshFilter(MeshFilter filter, uint32_t taskGroup, uint32_t meshGroup)
{
  // calculate new scrolls manually to keep the same logical item selected
  m_Scrolls = new PopulateBufferData;
  FillScrolls(m_Scrolls);

  {
    const BufferConfiguration &config1 = m_ModelOut1->getConfig();
    const BufferConfiguration &config2 = m_ModelOut2->getConfig();

    // baseTaskRow is the first row in the mesh view for the start of the task with no mesh filter,
    // and baseMeshRow is the offset to the filtered mesh (if relevant). They could be identical
    const uint32_t prevBaseTaskRow = m_TaskFilterRowOffset;
    const uint32_t prevBaseMeshRow = m_MeshFilterRowOffset;

    // if we're filtering directly to a task from none, we also have the prefix count we just have
    // to determine the base mesh
    uint32_t taskBaseMesh = 0;
    for(uint32_t i = 0; i < taskGroup && i < config1.taskSizes.size(); i++)
      taskBaseMesh += config1.taskSizes[i].x * config1.taskSizes[i].y * config1.taskSizes[i].z;

    uint32_t newBaseTaskRow = 0, newBaseMeshRow = 0;
    if(filter == MeshFilter::None || config2.meshletVertexPrefixCounts.empty())
    {
      // if the new filter is none, then our new base row for both is 0
      newBaseTaskRow = newBaseMeshRow = 0;
    }
    else if(m_CurMeshFilter == MeshFilter::None && filter == MeshFilter::Mesh)
    {
      newBaseTaskRow = config2.meshletVertexPrefixCounts[taskBaseMesh];
      newBaseMeshRow = config2.meshletVertexPrefixCounts[meshGroup];
    }
    else if(m_CurMeshFilter == MeshFilter::None && filter == MeshFilter::TaskGroup)
    {
      newBaseTaskRow = newBaseMeshRow = config2.meshletVertexPrefixCounts[taskBaseMesh];
    }
    else if(m_CurMeshFilter == MeshFilter::TaskGroup && filter == MeshFilter::Mesh)
    {
      // the first complex case - if we're already filtered to a task and now we're filtering to a
      // mesh, we only have prefix counts relatively so look it up
      newBaseTaskRow = prevBaseTaskRow;
      newBaseMeshRow = prevBaseTaskRow + config2.meshletVertexPrefixCounts[meshGroup - taskBaseMesh];
    }
    else if(m_CurMeshFilter == MeshFilter::Mesh && filter == MeshFilter::TaskGroup)
    {
      // the second complex case - if we're already filtered to a *mesh* and now we're filtering
      // back to the task, we undo the previous per-mesh filter
      newBaseTaskRow = newBaseMeshRow = prevBaseTaskRow;

      // only support filtering within the same group, not arbitrarily from one mesh in one task
      // group to a different task group
      Q_ASSERT(m_FilteredTaskGroup == taskGroup);
    }

    const uint32_t prevBaseRow = prevBaseMeshRow;
    const uint32_t newBaseRow = newBaseMeshRow;

    // when going to/from no filter, we just rebase by the base row and set the task row that we know directly
    if(m_CurMeshFilter == MeshFilter::None)
    {
      m_Scrolls->out1Vert = 0;
      m_Scrolls->out2Vert -= newBaseRow;
    }
    else if(filter == MeshFilter::None)
    {
      m_Scrolls->out1Vert = config1.taskOrMeshletOffset;
      m_Scrolls->out2Vert += prevBaseRow;
    }

    // otherwise changing between task and mesh filter, we rebase based on the difference between
    // the number of meshes shown. The task filter doesn't have to change
    else if(m_CurMeshFilter == MeshFilter::TaskGroup && filter == MeshFilter::Mesh)
    {
      m_Scrolls->out1Vert = 0;
      m_Scrolls->out2Vert -= (newBaseRow - prevBaseRow);
    }
    else if(m_CurMeshFilter == MeshFilter::Mesh && filter == MeshFilter::TaskGroup)
    {
      m_Scrolls->out1Vert = 0;
      m_Scrolls->out2Vert += (prevBaseRow - newBaseRow);
    }

    m_TaskFilterRowOffset = newBaseTaskRow;
    m_MeshFilterRowOffset = newBaseMeshRow;
  }

  m_CurMeshFilter = filter;
  m_FilteredTaskGroup = taskGroup;
  m_FilteredMeshGroup = meshGroup;

  switch(m_CurMeshFilter)
  {
    case MeshFilter::None:
      ui->meshFilterLabel->setText(tr("Current Range filter: None"));
      ui->resetMeshFilterButton->setEnabled(false);
      break;
    case MeshFilter::TaskGroup:
      if(IsD3D(m_Ctx.APIProps().pipelineType))
        ui->meshFilterLabel->setText(tr("Current Range filter: Single Amplification Threadgroup"));
      else
        ui->meshFilterLabel->setText(tr("Current Range filter: Single Task"));
      ui->resetMeshFilterButton->setEnabled(true);
      break;
    case MeshFilter::Mesh:
      ui->meshFilterLabel->setText(tr("Current Range filter: Single Meshlet"));
      ui->resetMeshFilterButton->setEnabled(true);
      break;
  }

  if(m_Ctx.IsCaptureLoaded())
    OnEventChanged(m_Ctx.CurEvent());
}

void BufferViewer::on_rowOffset_valueChanged(int value)
{
  if(!m_MeshView && m_ModelIn->getConfig().unclampedNumRows > 0)
  {
    int page = value / MaxVisibleRows;
    value %= MaxVisibleRows;

    uint64_t pageOffset = page * MaxVisibleRows * m_ModelIn->getConfig().buffers[0]->stride;

    // account for the extra row at the top with previous/next buttons
    if(pageOffset > 0)
      value++;

    if(pageOffset != m_PagingByteOffset)
    {
      m_PagingByteOffset = pageOffset;

      processFormat(m_Format);

      return;
    }
  }

  ScrollToRow(ui->inTable, value);
  ScrollToRow(ui->out1Table, value);
  ScrollToRow(ui->out2Table, value);

  // when we're paging and we select the first row, actually scroll up to include the previous/next
  // buttons.
  if(!m_MeshView && value == 1 && m_PagingByteOffset > 0)
    ui->inTable->verticalScrollBar()->setValue(0);
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

  BufferItemModel *model = m_ModelIn;
  int stage = 0;

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

    if(!isCurrentRasterOut())
    {
      // apply axis mapping to midpoint
      FloatVector transformedMid;
      transformedMid.x = m_Config.axisMapping.xAxis.x * mid.x +
                         m_Config.axisMapping.yAxis.x * mid.y + m_Config.axisMapping.zAxis.x * mid.z;
      transformedMid.y = m_Config.axisMapping.xAxis.y * mid.x +
                         m_Config.axisMapping.yAxis.y * mid.y + m_Config.axisMapping.zAxis.y * mid.z;
      transformedMid.z = m_Config.axisMapping.xAxis.z * mid.x +
                         m_Config.axisMapping.yAxis.z * mid.y + m_Config.axisMapping.zAxis.z * mid.z;
      mid = transformedMid;
    }

    mid.z -= len;

    m_Flycam->Reset(mid);
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}
