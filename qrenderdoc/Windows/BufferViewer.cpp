/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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
#include "ui_BufferViewer.h"

struct VBData
{
  byte *data;
  byte *end;
  size_t stride;
};

class BufferItemModel : public QAbstractItemModel
{
public:
  BufferItemModel(QObject *parent) : QAbstractItemModel(parent) {}
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
    return columns.count();
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(section < columns.count() && orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return columns[section].name;

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      if(role == Qt::DisplayRole)
      {
        int row = index.row();
        int col = index.column();

        if(col >= 0 && col < columns.count() && row >= 0 && row < numRows)
        {
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
              if(v.type() == QMetaType::Double)
                ret += Formatter::Format(v.toDouble());
              else if(v.type() == QMetaType::Float)
                ret += Formatter::Format(v.toFloat());
              else if(v.type() == QMetaType::UInt || v.type() == QMetaType::UShort ||
                      v.type() == QMetaType::UChar)
                ret += Formatter::Format(v.toUInt());
              else if(v.type() == QMetaType::Int || v.type() == QMetaType::Short ||
                      v.type() == QMetaType::SChar)
                ret += Formatter::Format(v.toInt());
              else
                ret += v.toString();

              ret += " ";
            }

            return ret.trimmed();
          }
        }
      }
    }

    return QVariant();
  }

  int numRows;
  QList<FormatElement> columns;
  QList<VBData> buffers;
};

BufferViewer::BufferViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::BufferViewer), m_Ctx(ctx)
{
  ui->setupUi(this);
  m_Ctx->AddLogViewer(this);

  ui->vsinData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  Reset();
}

BufferViewer::~BufferViewer()
{
  m_Ctx->windowClosed(this);
  m_Ctx->RemoveLogViewer(this);
  delete ui;
}

BufferItemModel *model = NULL;

void BufferViewer::OnLogfileLoaded()
{
  Reset();

  model = new BufferItemModel(this);

  ui->vsinData->setModel(model);

  WId renderID = ui->render->winId();

  m_Ctx->Renderer()->BlockInvoke([renderID, this](IReplayRenderer *r) {
    m_Output = r->CreateOutput(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(renderID),
                               eOutputType_MeshDisplay);

    ui->render->setOutput(m_Output);

    OutputConfig c = {eOutputType_MeshDisplay};
    m_Output->SetOutputConfig(c);

    GUIInvoke::call([this]() { OnEventSelected(m_Ctx->CurEvent()); });
  });
}

void BufferViewer::OnLogfileClosed()
{
  Reset();
}

void BufferViewer::OnEventSelected(uint32_t eventID)
{
  if(model)
  {
    model->beginReset();

    QVector<VertexInputAttribute> vinputs = m_Ctx->CurPipelineState.GetVertexInputs();

    model->columns.clear();
    model->columns.reserve(vinputs.count());

    for(const VertexInputAttribute &a : vinputs)
    {
      if(!a.Used)
        continue;

      FormatElement f(a.Name, a.VertexBuffer, a.RelativeByteOffset, a.PerInstance, a.InstanceRate,
                      false,    // row major matrix
                      1,        // matrix dimension
                      a.Format, false);

      model->columns.push_back(f);
    }

    for(auto vb : model->buffers)
      delete[] vb.data;

    model->buffers.clear();

    const FetchDrawcall *draw = m_Ctx->CurDrawcall();

    if(draw == NULL)
      model->numRows = 0;
    else
      model->numRows = draw->numIndices;

    QVector<BoundVBuffer> vbs = m_Ctx->CurPipelineState.GetVBuffers();

    m_Ctx->Renderer()->AsyncInvoke([this, vbs](IReplayRenderer *r) {

      for(BoundVBuffer vb : vbs)
      {
        rdctype::array<byte> data;
        r->GetBufferData(vb.Buffer, vb.ByteOffset, 0, &data);

        VBData buf;
        buf.data = new byte[data.count];
        memcpy(buf.data, data.elems, data.count);
        buf.end = buf.data + data.count;
        buf.stride = vb.ByteStride;
        model->buffers.push_back(buf);
      }

      GUIInvoke::call([this] {
        model->endReset();
        ui->vsinData->resizeColumnsToContents();
      });
    });
  }
}

void BufferViewer::Reset()
{
  m_Output = NULL;

  if(model)
  {
    for(auto vb : model->buffers)
      delete[] vb.data;

    model->buffers.clear();

    model->columns.clear();

    model->numRows = 0;
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
    ui->horizontalLayout->addWidget(ui->render);
  }

  ui->render->setColours(QColor::fromRgbF(0.57f, 0.57f, 0.57f, 1.0f),
                         QColor::fromRgbF(0.81f, 0.81f, 0.81f, 1.0f));
}
