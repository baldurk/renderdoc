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
#include "ui_BufferViewer.h"

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

  m_ModelVSIn = new BufferItemModel(ui->vsinData, this);
  m_ModelVSOut = new BufferItemModel(ui->vsoutData, this);
  // m_ModelGSOut = new BufferItemModel(ui->gsoutData, this);

  m_ConfigVSIn.type = eMeshDataStage_VSIn;
  m_ConfigVSIn.cam = Camera_InitArcball();
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

  m_CamDist = 10.0f;

  Camera_SetArcballDistance(m_ConfigVSIn.cam, m_CamDist);

  m_ConfigVSIn.position.showAlpha = false;

  m_ConfigVSOut = m_ConfigVSIn;

  m_ConfigVSOut.cam = Camera_InitFPSLook();

  m_curConfig = &m_ConfigVSIn;
  ui->outputTabs->setCurrentIndex(0);

  QObject::connect(ui->vsinData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);
  QObject::connect(ui->vsoutData->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &BufferViewer::data_selected);

  Reset();
}

BufferViewer::~BufferViewer()
{
  delete[] m_ModelVSIn->indices.data;

  for(auto vb : m_ModelVSIn->buffers)
    delete[] vb.data;

  Camera_Shutdown(m_ConfigVSIn.cam);
  Camera_Shutdown(m_ConfigVSOut.cam);

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
  // m_ModelGSOut->beginReset();

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
  // m_ModelGSOut->componentWidth = m_ModelVSIn->componentWidth;

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

  m_CamDist = 10.0f;

  Camera_ResetArcball(m_ConfigVSIn.cam);
  Camera_SetArcballDistance(m_ConfigVSIn.cam, m_CamDist);

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

  Camera_SetPosition(m_ConfigVSOut.cam, 0.0f, 0.0f, 0.0f);
  Camera_SetFPSRotation(m_ConfigVSOut.cam, 0.0f, 0.0f, 0.0f);
  m_pos = FloatVector();
  m_rot = FloatVector();

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
      ui->vsinData->resizeColumnsToContents();
      ui->vsoutData->resizeColumnsToContents();
    });
  });
}

void BufferViewer::on_outputTabs_currentChanged(int index)
{
  ui->render->parentWidget()->layout()->removeWidget(ui->render);
  ui->outputTabs->widget(index)->layout()->addWidget(ui->render);

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

  QPoint curpos = e->pos();

  if(e->buttons() & Qt::LeftButton)
  {
    if(m_PrevPos != QPoint())
    {
      if(m_curConfig == &m_ConfigVSIn)
      {
        float ax = ((float)m_PrevPos.x() / (float)ui->render->rect().width()) * 2.0f - 1.0f;
        float ay = ((float)m_PrevPos.y() / (float)ui->render->rect().height()) * 2.0f - 1.0f;
        float bx = ((float)curpos.x() / (float)ui->render->rect().width()) * 2.0f - 1.0f;
        float by = ((float)curpos.y() / (float)ui->render->rect().height()) * 2.0f - 1.0f;

        // this isn't a 'true arcball' but it handles extreme aspect ratios
        // better. We basically 'centre' around the from point always being
        // 0,0 (straight out of the screen) as if you're always dragging
        // the arcball from the middle, and just use the relative movement
        int minDimension = qMin(ui->render->rect().width(), ui->render->rect().height());

        ax = ay = 0;
        bx = ((float)(curpos.x() - m_PrevPos.x()) / (float)minDimension) * 2.0f;
        by = ((float)(curpos.y() - m_PrevPos.y()) / (float)minDimension) * 2.0f;

        ay = -ay;
        by = -by;

        Camera_RotateArcball(m_curConfig->cam, ax, ay, bx, by);
      }
      else
      {
        m_rot.y -= (float)(curpos.x() - m_PrevPos.x()) / 300.0f;
        m_rot.x -= (float)(curpos.y() - m_PrevPos.y()) / 300.0f;

        Camera_SetFPSRotation(m_curConfig->cam, m_rot.x, m_rot.y, m_rot.z);
      }
    }

    m_PrevPos = curpos;
  }
  else
  {
    m_PrevPos = QPoint();
  }

  if(e->buttons() & Qt::RightButton)
  {
    render_clicked(e);
    return;
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_clicked(QMouseEvent *e)
{
  if(!m_Ctx->LogLoaded())
    return;

  ui->render->setFocus();

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

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_mouseWheel(QWheelEvent *e)
{
  float mod = (1.0f - e->delta() / 2500.0f);

  m_CamDist = qMax(1e-6f, m_CamDist * mod);

  Camera_SetArcballDistance(m_ConfigVSIn.cam, m_CamDist);

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::render_keyPress(QKeyEvent *e)
{
  if(m_curConfig != &m_ConfigVSIn)
  {
    FloatVector pos, fwd, right, up;
    Camera_GetBasis(m_curConfig->cam, &pos, &fwd, &right, &up);

    if(e->key() == Qt::Key_W)
    {
      m_pos.x += fwd.x * 0.1f;
      m_pos.y += fwd.y * 0.1f;
      m_pos.z += fwd.z * 0.1f;
    }
    if(e->key() == Qt::Key_S)
    {
      m_pos.x -= fwd.x * 0.1f;
      m_pos.y -= fwd.y * 0.1f;
      m_pos.z -= fwd.z * 0.1f;
    }
    if(e->key() == Qt::Key_A)
    {
      m_pos.x -= right.x * 0.1f;
      m_pos.y -= right.y * 0.1f;
      m_pos.z -= right.z * 0.1f;
    }
    if(e->key() == Qt::Key_D)
    {
      m_pos.x += right.x * 0.1f;
      m_pos.y += right.y * 0.1f;
      m_pos.z += right.z * 0.1f;
    }

    Camera_SetPosition(m_curConfig->cam, m_pos.x, m_pos.y, m_pos.z);
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void BufferViewer::RT_UpdateAndDisplay(IReplayRenderer *)
{
  if(m_Output)
  {
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
    ui->outputTabs->currentWidget()->layout()->addWidget(ui->render);
  }

  QObject::connect(ui->render, &CustomPaintWidget::mouseMove, this, &BufferViewer::render_mouseMove);
  QObject::connect(ui->render, &CustomPaintWidget::clicked, this, &BufferViewer::render_clicked);
  QObject::connect(ui->render, &CustomPaintWidget::keyPress, this, &BufferViewer::render_keyPress);
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
