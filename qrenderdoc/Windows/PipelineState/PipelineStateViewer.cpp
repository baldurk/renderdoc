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

#include "PipelineStateViewer.h"
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSvgRenderer>
#include <QToolButton>
#include <QXmlStreamWriter>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDLabel.h"
#include "D3D11PipelineStateViewer.h"
#include "D3D12PipelineStateViewer.h"
#include "GLPipelineStateViewer.h"
#include "VulkanPipelineStateViewer.h"
#include "ui_PipelineStateViewer.h"

static uint32_t byteSize(const ResourceFormat &fmt)
{
  if(fmt.Special())
  {
    switch(fmt.type)
    {
      default:
      case ResourceFormatType::R9G9B9E5:
      case ResourceFormatType::R5G6B5:
      case ResourceFormatType::R5G5B5A1:
      case ResourceFormatType::R4G4B4A4:
      case ResourceFormatType::R4G4:
      case ResourceFormatType::BC1:
      case ResourceFormatType::BC2:
      case ResourceFormatType::BC3:
      case ResourceFormatType::BC4:
      case ResourceFormatType::BC5:
      case ResourceFormatType::BC6:
      case ResourceFormatType::BC7:
      case ResourceFormatType::ETC2:
      case ResourceFormatType::EAC:
      case ResourceFormatType::ASTC:
      case ResourceFormatType::D16S8:
      case ResourceFormatType::D24S8:
      case ResourceFormatType::D32S8:
      case ResourceFormatType::S8:
      case ResourceFormatType::YUV8:
      case ResourceFormatType::YUV10:
      case ResourceFormatType::YUV12:
      case ResourceFormatType::YUV16:
      case ResourceFormatType::PVRTC: return ~0U;
      case ResourceFormatType::R10G10B10A2:
      case ResourceFormatType::R11G11B10: return 4;
    }
  }

  return fmt.compByteWidth * fmt.compCount;
}

static QString padding(uint32_t bytes)
{
  if(bytes == 0)
    return QString();

  QString ret;

  if(bytes > 4)
  {
    ret += lit("xint pad[%1];").arg(bytes / 4);

    bytes = bytes % 4;
  }

  if(bytes == 4)
    ret += lit("xint pad;");
  else if(bytes == 3)
    ret += lit("xshort pad; xbyte pad;");
  else if(bytes == 2)
    ret += lit("xshort pad;");
  else if(bytes == 1)
    ret += lit("xbyte pad;");

  return ret + lit("\n");
}

PipelineStateViewer::PipelineStateViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;

  for(size_t i = 0; i < ARRAY_COUNT(editMenus); i++)
    editMenus[i] = new QMenu(this);

  setToD3D11();

  m_Ctx.AddCaptureViewer(this);
}

PipelineStateViewer::~PipelineStateViewer()
{
  reset();

  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);

  delete ui;
}

void PipelineStateViewer::OnCaptureLoaded()
{
  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D11)
    setToD3D11();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D12)
    setToD3D12();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
    setToGL();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan)
    setToVulkan();

  if(m_Current)
    m_Current->OnCaptureLoaded();
}

void PipelineStateViewer::OnCaptureClosed()
{
  if(m_Current)
    m_Current->OnCaptureClosed();
}

void PipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D11)
    setToD3D11();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D12)
    setToD3D12();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
    setToGL();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan)
    setToVulkan();

  if(m_Current)
    m_Current->OnEventChanged(eventId);
}

QString PipelineStateViewer::GetCurrentAPI()
{
  if(m_Current == m_D3D11)
    return lit("D3D11");
  else if(m_Current == m_D3D12)
    return lit("D3D12");
  else if(m_Current == m_GL)
    return lit("OpenGL");
  else if(m_Current == m_Vulkan)
    return lit("Vulkan");

  return lit("");
}

QVariant PipelineStateViewer::persistData()
{
  QVariantMap state;

  state[lit("type")] = GetCurrentAPI();

  return state;
}

void PipelineStateViewer::setPersistData(const QVariant &persistData)
{
  QString str = persistData.toMap()[lit("type")].toString();

  if(str == lit("D3D11"))
    setToD3D11();
  else if(str == lit("D3D12"))
    setToD3D12();
  else if(str == lit("GL"))
    setToGL();
  else if(str == lit("Vulkan"))
    setToVulkan();
}

void PipelineStateViewer::reset()
{
  delete m_D3D11;
  delete m_D3D12;
  delete m_GL;
  delete m_Vulkan;

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;
}

void PipelineStateViewer::setToD3D11()
{
  if(m_D3D11)
    return;

  reset();

  m_D3D11 = new D3D11PipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_D3D11);
  m_Current = m_D3D11;
}

void PipelineStateViewer::setToD3D12()
{
  if(m_D3D12)
    return;

  reset();

  m_D3D12 = new D3D12PipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_D3D12);
  m_Current = m_D3D12;
}

void PipelineStateViewer::setToGL()
{
  if(m_GL)
    return;

  reset();

  m_GL = new GLPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_GL);
  m_Current = m_GL;
}

void PipelineStateViewer::setToVulkan()
{
  if(m_Vulkan)
    return;

  reset();

  m_Vulkan = new VulkanPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_Vulkan);
  m_Current = m_Vulkan;
}

QXmlStreamWriter *PipelineStateViewer::beginHTMLExport()
{
  if(!m_Ctx.IsCaptureLoaded())
    return NULL;

  QString filename = RDDialog::getSaveFileName(this, tr("Export pipeline state as HTML"), QString(),
                                               tr("HTML files (*.html)"));

  if(!filename.isEmpty())
  {
    ANALYTIC_SET(Export.PipelineState, true);

    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile *f = new QFile(filename, this);
      if(f->open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        QXmlStreamWriter *xmlptr = new QXmlStreamWriter(f);

        QXmlStreamWriter &xml = *xmlptr;

        xml.setAutoFormatting(true);
        xml.setAutoFormattingIndent(4);
        xml.writeStartDocument();
        xml.writeDTD(lit("<!DOCTYPE html>"));

        xml.writeStartElement(lit("html"));
        xml.writeAttribute(lit("lang"), lit("en"));

        QString title = tr("%1 EID %2 - %3 Pipeline export")
                            .arg(QFileInfo(m_Ctx.GetCaptureFilename()).fileName())
                            .arg(m_Ctx.CurEvent())
                            .arg(GetCurrentAPI());

        {
          xml.writeStartElement(lit("head"));

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("charset"), lit("utf-8"));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("http-equiv"), lit("X-UA-Compatible"));
          xml.writeAttribute(lit("content"), lit("IE=edge"));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("name"), lit("viewport"));
          xml.writeAttribute(lit("content"), lit("width=device-width, initial-scale=1"));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("name"), lit("description"));
          xml.writeAttribute(lit("content"), lit(""));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("name"), lit("author"));
          xml.writeAttribute(lit("content"), lit(""));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("http-equiv"), lit("Content-Type"));
          xml.writeAttribute(lit("content"), lit("text/html;charset=utf-8"));
          xml.writeEndElement();

          xml.writeStartElement(lit("title"));
          xml.writeCharacters(title);
          xml.writeEndElement();

          xml.writeStartElement(lit("style"));
          xml.writeComment(lit(R"(

/* If you think this css is ugly/bad, open a pull request! */
body { margin: 20px; }
div.stage { border: 1px solid #BBBBBB; border-radius: 5px; padding: 16px; margin-bottom: 32px; }
div.stage h1 { text-decoration: underline; margin-top: 0px; }
div.stage table { border: 1px solid #AAAAAA; border-collapse: collapse; }
div.stage table thead tr { border-bottom: 1px solid #AAAAAA; background-color: #EEEEFF; }
div.stage table tr th { border-right: 1px solid #AAAAAA; padding: 6px; }
div.stage table tr td { border-right: 1px solid #AAAAAA; background-color: #EEEEEE; padding: 3px; }

)"));
          xml.writeEndElement();    // </style>

          xml.writeEndElement();    // </head>
        }

        {
          xml.writeStartElement(lit("body"));

          xml.writeStartElement(lit("h1"));
          xml.writeCharacters(title);
          xml.writeEndElement();

          xml.writeStartElement(lit("h3"));
          {
            QString context = tr("Frame %1").arg(m_Ctx.FrameInfo().frameNumber);

            const DrawcallDescription *draw = m_Ctx.CurDrawcall();

            QList<const DrawcallDescription *> drawstack;
            const DrawcallDescription *parent = draw->parent;
            while(parent)
            {
              drawstack.push_front(parent);
              parent = parent->parent;
            }

            for(const DrawcallDescription *d : drawstack)
            {
              context += QFormatStr(" > %1").arg(d->name);
            }

            context += QFormatStr(" => %1").arg(draw->name);

            xml.writeCharacters(context);
          }
          xml.writeEndElement();    // </h3>
        }

        // body is open

        return xmlptr;
      }

      RDDialog::critical(
          this, tr("Error exporting pipeline state"),
          tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f->errorString()));

      delete f;

      return NULL;
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
      return NULL;
    }
  }

  return NULL;
}

void PipelineStateViewer::exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols,
                                          const QList<QVariantList> &rows)
{
  xml.writeStartElement(lit("table"));

  {
    xml.writeStartElement(lit("thead"));
    xml.writeStartElement(lit("tr"));

    for(const QString &col : cols)
    {
      xml.writeStartElement(lit("th"));
      xml.writeCharacters(col);
      xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndElement();
  }

  {
    xml.writeStartElement(lit("tbody"));

    if(rows.isEmpty())
    {
      xml.writeStartElement(lit("tr"));

      for(int i = 0; i < cols.count(); i++)
      {
        xml.writeStartElement(lit("td"));
        xml.writeCharacters(lit("-"));
        xml.writeEndElement();
      }

      xml.writeEndElement();
    }
    else
    {
      for(const QVariantList &row : rows)
      {
        xml.writeStartElement(lit("tr"));

        for(const QVariant &el : row)
        {
          xml.writeStartElement(lit("td"));

          if(el.type() == QVariant::Bool)
            xml.writeCharacters(el.toBool() ? tr("True") : tr("False"));
          else
            xml.writeCharacters(el.toString());

          xml.writeEndElement();
        }

        xml.writeEndElement();
      }
    }

    xml.writeEndElement();
  }

  xml.writeEndElement();
}

void PipelineStateViewer::exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols,
                                          const QVariantList &row)
{
  exportHTMLTable(xml, cols, QList<QVariantList>({row}));
}

void PipelineStateViewer::endHTMLExport(QXmlStreamWriter *xml)
{
  xml->writeEndElement();    // </body>

  xml->writeEndElement();    // </html>

  xml->writeEndDocument();

  // delete the file the writer was writing to
  QFile *f = qobject_cast<QFile *>(xml->device());
  delete f;

  delete xml;
}

void PipelineStateViewer::setTopologyDiagram(QLabel *diagram, Topology topo)
{
  int idx = qMin((int)topo, (int)Topology::PatchList);

  if(m_TopoPixmaps[idx].isNull())
  {
    QSvgRenderer svg;
    switch(topo)
    {
      case Topology::PointList: svg.load(lit(":/topologies/topo_pointlist.svg")); break;
      case Topology::LineList: svg.load(lit(":/topologies/topo_linelist.svg")); break;
      case Topology::LineStrip: svg.load(lit(":/topologies/topo_linestrip.svg")); break;
      case Topology::TriangleList: svg.load(lit(":/topologies/topo_trilist.svg")); break;
      case Topology::TriangleStrip: svg.load(lit(":/topologies/topo_tristrip.svg")); break;
      case Topology::LineList_Adj: svg.load(lit(":/topologies/topo_linelist_adj.svg")); break;
      case Topology::LineStrip_Adj: svg.load(lit(":/topologies/topo_linestrip_adj.svg")); break;
      case Topology::TriangleList_Adj: svg.load(lit(":/topologies/topo_trilist_adj.svg")); break;
      case Topology::TriangleStrip_Adj: svg.load(lit(":/topologies/topo_tristrip_adj.svg")); break;
      default: svg.load(lit(":/topologies/topo_patch.svg")); break;
    }

    QRect rect = svg.viewBox();

    QImage im(rect.size() * diagram->devicePixelRatio(), QImage::Format_ARGB32);

    im.fill(QColor(0, 0, 0, 0));

    QPainter p(&im);

    svg.render(&p);

    // convert the colors - black maps to Text (foreground) and white maps to Base (background)
    QColor white = diagram->palette().color(QPalette::Active, QPalette::Base);
    QColor black = diagram->palette().color(QPalette::Active, QPalette::Text);

    const float br = black.redF();
    const float bg = black.greenF();
    const float bb = black.blueF();

    const float wr = white.redF();
    const float wg = white.greenF();
    const float wb = white.blueF();

    for(int y = 0; y < im.height(); y++)
    {
      QRgb *line = (QRgb *)im.scanLine(y);

      for(int x = 0; x < im.width(); x++)
      {
        // delta of 0 is black, delta of 255 is white
        const float delta = float(qRed(*line));
        const float bd = 255.0f - delta;
        const float wd = delta;

        const int r = int(br * bd + wr * wd);
        const int g = int(bg * bd + wg * wd);
        const int b = int(bb * bd + wb * wd);

        *line = qRgba(r, g, b, qAlpha(*line));

        line++;
      }
    }

    m_TopoPixmaps[idx] = QPixmap::fromImage(im);
    m_TopoPixmaps[idx].setDevicePixelRatio(diagram->devicePixelRatioF());
  }

  diagram->setPixmap(m_TopoPixmaps[idx]);
}

void PipelineStateViewer::setMeshViewPixmap(RDLabel *meshView)
{
  QImage meshIcon = Pixmaps::wireframe_mesh(meshView->devicePixelRatio()).toImage();
  QImage colSwapped(meshIcon.size(), QImage::Format_ARGB32);
  colSwapped.fill(meshView->palette().color(QPalette::WindowText));

  for(int y = 0; y < meshIcon.height(); y++)
  {
    const QRgb *in = (const QRgb *)meshIcon.constScanLine(y);
    QRgb *out = (QRgb *)colSwapped.scanLine(y);

    for(int x = 0; x < meshIcon.width(); x++)
    {
      *out = qRgba(qRed(*out), qGreen(*out), qBlue(*out), qAlpha(*in));

      in++;
      out++;
    }
  }

  QPixmap p = QPixmap::fromImage(colSwapped);
  p.setDevicePixelRatio(meshView->devicePixelRatioF());

  meshView->setPixmap(p);
  meshView->setPreserveAspectRatio(true);

  QPalette pal = meshView->palette();
  pal.setColor(QPalette::Shadow, pal.color(QPalette::Window).darker(120));
  meshView->setPalette(pal);
  meshView->setBackgroundRole(QPalette::Window);
  meshView->setMouseTracking(true);

  QObject::connect(meshView, &RDLabel::mouseMoved, [meshView](QMouseEvent *) {
    meshView->setBackgroundRole(QPalette::Shadow);
    meshView->setAutoFillBackground(true);
  });
  QObject::connect(meshView, &RDLabel::leave, [meshView]() {
    meshView->setBackgroundRole(QPalette::Window);
    meshView->setAutoFillBackground(false);
  });
}

QString PipelineStateViewer::declareStruct(QList<QString> &declaredStructs, const QString &name,
                                           const rdcarray<ShaderConstant> &members,
                                           uint32_t requiredByteStride)
{
  QString ret;

  ret = lit("struct %1\n{\n").arg(name);

  for(int i = 0; i < members.count(); i++)
  {
    QString arraySize;
    if(members[i].type.descriptor.elements > 1)
      arraySize = QFormatStr("[%1]").arg(members[i].type.descriptor.elements);

    QString varTypeName = members[i].type.descriptor.name;

    if(!members[i].type.members.isEmpty())
    {
      // GL structs don't give us typenames (boo!) so give them unique names. This will mean some
      // structs get duplicated if they're used in multiple places, but not much we can do about
      // that.
      if(varTypeName.isEmpty() || varTypeName == lit("struct"))
        varTypeName = lit("anon%1").arg(declaredStructs.size());

      if(!declaredStructs.contains(varTypeName))
      {
        declaredStructs.push_back(varTypeName);
        ret = declareStruct(declaredStructs, varTypeName, members[i].type.members,
                            members[i].type.descriptor.arrayByteStride) +
              lit("\n") + ret;
      }
    }

    ret += QFormatStr("    %1 %2%3;\n").arg(varTypeName).arg(members[i].name).arg(arraySize);
  }

  if(requiredByteStride > 0)
  {
    uint32_t structStart = 0;

    const ShaderConstant *lastChild = &members.back();

    structStart += lastChild->byteOffset;
    while(!lastChild->type.members.isEmpty())
    {
      lastChild = &lastChild->type.members.back();
      structStart += lastChild->byteOffset;
    }

    uint32_t size = lastChild->type.descriptor.rows * lastChild->type.descriptor.columns;
    if(lastChild->type.descriptor.type == VarType::Double ||
       lastChild->type.descriptor.type == VarType::ULong ||
       lastChild->type.descriptor.type == VarType::SLong)
      size *= 8;
    else if(lastChild->type.descriptor.type == VarType::Float ||
            lastChild->type.descriptor.type == VarType::UInt ||
            lastChild->type.descriptor.type == VarType::SInt)
      size *= 4;
    else if(lastChild->type.descriptor.type == VarType::Half ||
            lastChild->type.descriptor.type == VarType::UShort ||
            lastChild->type.descriptor.type == VarType::SShort)
      size *= 2;

    if(lastChild->type.descriptor.elements > 1)
      size *= lastChild->type.descriptor.elements;

    uint32_t padBytes = requiredByteStride - (lastChild->byteOffset + size);

    if(padBytes > 0)
      ret += lit("    ") + padding(padBytes);
  }

  ret += lit("}\n");

  return ret;
}

QString PipelineStateViewer::GenerateBufferFormatter(const ShaderResource &res,
                                                     const ResourceFormat &viewFormat,
                                                     uint64_t &baseByteOffset)
{
  QString format;

  if(!res.variableType.members.empty())
  {
    if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan ||
       m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
    {
      const rdcarray<ShaderConstant> &members = res.variableType.members;

      format += QFormatStr("struct %1\n{\n").arg(res.name);

      // GL/Vulkan allow fixed-sized members before the array-of-structs. This can't be
      // represented in a buffer format so we skip it
      if(members.count() > 1)
      {
        format += tr("    // members skipped as they are fixed size:\n");
        baseByteOffset += members.back().byteOffset;
      }

      QString varTypeName;
      QString comment = lit("// ");
      for(int i = 0; i < members.count(); i++)
      {
        QString arraySize;
        if(members[i].type.descriptor.elements > 1)
          arraySize = QFormatStr("[%1]").arg(members[i].type.descriptor.elements);

        varTypeName = members[i].type.descriptor.name;

        if(i + 1 == members.count())
        {
          comment = arraySize = QString();

          if(members.count() > 1)
            format +=
                lit("    // final array struct @ byte offset %1\n").arg(members.back().byteOffset);

          // give GL nameless structs a better name
          if(varTypeName.isEmpty() || varTypeName == lit("struct"))
            varTypeName = lit("root_struct");
        }

        format +=
            QFormatStr("    %1%2 %3%4;\n").arg(comment).arg(varTypeName).arg(members[i].name).arg(arraySize);
      }

      format += lit("}");

      // if the last member is a struct, declare it
      if(!members.back().type.members.isEmpty())
      {
        QList<QString> declaredStructs;
        format = declareStruct(declaredStructs, varTypeName, members.back().type.members,
                               members.back().type.descriptor.arrayByteStride) +
                 lit("\n") + format;
      }
    }
    else
    {
      QList<QString> declaredStructs;
      format = declareStruct(declaredStructs, res.variableType.descriptor.name,
                             res.variableType.members, 0);
    }
  }
  else
  {
    const auto &desc = res.variableType.descriptor;

    if(viewFormat.type == ResourceFormatType::Undefined)
    {
      if(desc.type == VarType::Unknown)
      {
        format = desc.name;
      }
      else
      {
        if(desc.rowMajorStorage)
          format += lit("row_major ");

        format += ToQStr(desc.type);
        if(desc.rows > 1 && desc.columns > 1)
          format += QFormatStr("%1x%2").arg(desc.rows).arg(desc.columns);
        else if(desc.columns > 1)
          format += QString::number(desc.columns);

        if(!desc.name.empty())
          format += lit(" ") + desc.name;

        if(desc.elements > 1)
          format += QFormatStr("[%1]").arg(desc.elements);
      }
    }
    else
    {
      if(viewFormat.type == ResourceFormatType::R10G10B10A2)
      {
        if(viewFormat.compType == CompType::UInt)
          format = lit("uintten");
        if(viewFormat.compType == CompType::UNorm)
          format = lit("unormten");
      }
      else if(viewFormat.type == ResourceFormatType::R11G11B10)
      {
        format = lit("floateleven");
      }
      else
      {
        switch(viewFormat.compByteWidth)
        {
          case 1:
          {
            if(viewFormat.compType == CompType::UNorm || viewFormat.compType == CompType::UNormSRGB)
              format = lit("unormb");
            if(viewFormat.compType == CompType::SNorm)
              format = lit("snormb");
            if(viewFormat.compType == CompType::UInt)
              format = lit("ubyte");
            if(viewFormat.compType == CompType::SInt)
              format = lit("byte");
            break;
          }
          case 2:
          {
            if(viewFormat.compType == CompType::UNorm || viewFormat.compType == CompType::UNormSRGB)
              format = lit("unormh");
            if(viewFormat.compType == CompType::SNorm)
              format = lit("snormh");
            if(viewFormat.compType == CompType::UInt)
              format = lit("ushort");
            if(viewFormat.compType == CompType::SInt)
              format = lit("short");
            if(viewFormat.compType == CompType::Float)
              format = lit("half");
            break;
          }
          case 4:
          {
            if(viewFormat.compType == CompType::UNorm || viewFormat.compType == CompType::UNormSRGB)
              format = lit("unormf");
            if(viewFormat.compType == CompType::SNorm)
              format = lit("snormf");
            if(viewFormat.compType == CompType::UInt)
              format = lit("uint");
            if(viewFormat.compType == CompType::SInt)
              format = lit("int");
            if(viewFormat.compType == CompType::Float)
              format = lit("float");
            break;
          }
        }

        format += QString::number(viewFormat.compCount);
      }
    }
  }

  return format;
}

void PipelineStateViewer::MakeShaderVariablesHLSL(bool cbufferContents,
                                                  const rdcarray<ShaderConstant> &vars,
                                                  QString &struct_contents, QString &struct_defs)
{
  for(const ShaderConstant &v : vars)
  {
    if(!v.type.members.isEmpty())
    {
      QString def = lit("struct %1 {\n").arg(v.type.descriptor.name);

      if(!struct_defs.contains(def))
      {
        QString contents;
        MakeShaderVariablesHLSL(false, v.type.members, contents, struct_defs);

        struct_defs += def + contents + lit("};\n\n");
      }
    }

    struct_contents += lit("\t%1 %2").arg(v.type.descriptor.name).arg(v.name);

    if((v.byteOffset % 4) != 0)
      qWarning() << "Variable " << QString(v.name) << " is not DWORD aligned";

    uint32_t dwordOffset = v.byteOffset / 4;

    uint32_t vectorIndex = dwordOffset / 4;
    uint32_t vectorComponent = dwordOffset % 4;

    char comp = 'x';
    if(vectorComponent == 1)
      comp = 'y';
    if(vectorComponent == 2)
      comp = 'z';
    if(vectorComponent == 3)
      comp = 'w';

    if(cbufferContents)
      struct_contents += lit(" : packoffset(c%1.%2);").arg(vectorIndex).arg(QLatin1Char(comp));
    else
      struct_contents += lit(";");

    struct_contents += lit("\n");
  }
}

QString PipelineStateViewer::GenerateHLSLStub(const ShaderReflection *shaderDetails,
                                              const QString &entryFunc)
{
  QString hlsl = lit("// HLSL function stub generated\n\n");

  const QString textureDim[ENUM_ARRAY_SIZE(TextureType)] = {
      lit("Unknown"),          lit("Buffer"),      lit("Texture1D"),      lit("Texture1DArray"),
      lit("Texture2D"),        lit("TextureRect"), lit("Texture2DArray"), lit("Texture2DMS"),
      lit("Texture2DMSArray"), lit("Texture3D"),   lit("TextureCube"),    lit("TextureCubeArray"),
  };

  for(const ShaderSampler &samp : shaderDetails->samplers)
  {
    hlsl += lit("//SamplerComparisonState %1 : register(s%2); // can't disambiguate\n"
                "SamplerState %1 : register(s%2); // can't disambiguate\n")
                .arg(samp.name)
                .arg(samp.bindPoint);
  }

  for(int i = 0; i < 2; i++)
  {
    const rdcarray<ShaderResource> &resources =
        (i == 0 ? shaderDetails->readOnlyResources : shaderDetails->readWriteResources);
    for(const ShaderResource &res : resources)
    {
      char regChar = 't';

      if(i == 1)
      {
        hlsl += lit("RW");
        regChar = 'u';
      }

      if(res.isTexture)
      {
        hlsl += lit("%1<%2> %3 : register(%4%5);\n")
                    .arg(textureDim[(size_t)res.resType])
                    .arg(res.variableType.descriptor.name)
                    .arg(res.name)
                    .arg(QLatin1Char(regChar))
                    .arg(res.bindPoint);
      }
      else
      {
        if(res.variableType.descriptor.rows > 1)
          hlsl += lit("Structured");

        hlsl += lit("Buffer<%1> %2 : register(%3%4);\n")
                    .arg(res.variableType.descriptor.name)
                    .arg(res.name)
                    .arg(QLatin1Char(regChar))
                    .arg(res.bindPoint);
      }
    }
  }

  hlsl += lit("\n\n");

  QString cbuffers;

  int cbufIdx = 0;
  for(const ConstantBlock &cbuf : shaderDetails->constantBlocks)
  {
    if(!cbuf.name.isEmpty() && !cbuf.variables.isEmpty())
    {
      QString cbufName = cbuf.name;
      if(cbufName == lit("$Globals"))
        cbufName = lit("_Globals");
      cbuffers += lit("cbuffer %1 : register(b%2) {\n").arg(cbufName).arg(cbuf.bindPoint);
      MakeShaderVariablesHLSL(true, cbuf.variables, cbuffers, hlsl);
      cbuffers += lit("};\n\n");
    }
    cbufIdx++;
  }

  hlsl += cbuffers;

  hlsl += lit("\n\n");

  hlsl += lit("struct InputStruct {\n");
  for(const SigParameter &sig : shaderDetails->inputSignature)
    hlsl += lit("\t%1 %2 : %3;\n")
                .arg(TypeString(sig))
                .arg(!sig.varName.isEmpty() ? QString(sig.varName) : lit("param%1").arg(sig.regIndex))
                .arg(D3DSemanticString(sig));
  hlsl += lit("};\n\n");

  hlsl += lit("struct OutputStruct {\n");
  for(const SigParameter &sig : shaderDetails->outputSignature)
    hlsl += lit("\t%1 %2 : %3;\n")
                .arg(TypeString(sig))
                .arg(!sig.varName.isEmpty() ? QString(sig.varName) : lit("param%1").arg(sig.regIndex))
                .arg(D3DSemanticString(sig));
  hlsl += lit("};\n\n");

  hlsl += lit("OutputStruct %1(in InputStruct IN)\n"
              "{\n"
              "\tOutputStruct OUT = (OutputStruct)0;\n"
              "\n"
              "\t// ...\n"
              "\n"
              "\treturn OUT;\n"
              "}\n")
              .arg(entryFunc);

  return hlsl;
}

void PipelineStateViewer::shaderEdit_clicked()
{
  QToolButton *sender = qobject_cast<QToolButton *>(QObject::sender());
  if(!sender)
    return;

  // activate the first item in the menu, if there are any items, as the default action.
  QMenu *menu = sender->menu();
  if(menu && !menu->actions().isEmpty())
    menu->actions()[0]->trigger();
}

IShaderViewer *PipelineStateViewer::EditShader(ResourceId id, ShaderStage shaderType,
                                               const rdcstr &entry, ShaderCompileFlags compileFlags,
                                               ShaderEncoding encoding, const rdcstrpairs &files)
{
  auto saveCallback = [shaderType, id](ICaptureContext *ctx, IShaderViewer *viewer,
                                       ShaderEncoding shaderEncoding, ShaderCompileFlags flags,
                                       rdcstr entryFunc, bytebuf shaderBytes) {
    if(shaderBytes.isEmpty())
      return;

    ANALYTIC_SET(UIFeatures.ShaderEditing, true);

    // invoke off to the ReplayController to replace the capture's shader
    // with our edited one
    ctx->Replay().AsyncInvoke([ctx, entryFunc, shaderBytes, shaderEncoding, flags, shaderType, id,
                               viewer](IReplayController *r) {
      rdcstr errs;

      ResourceId from = id;
      ResourceId to;

      std::tie(to, errs) =
          r->BuildTargetShader(entryFunc.c_str(), shaderEncoding, shaderBytes, flags, shaderType);

      GUIInvoke::call(viewer->Widget(), [viewer, errs]() { viewer->ShowErrors(errs); });
      if(to == ResourceId())
      {
        r->RemoveReplacement(from);
        GUIInvoke::call(viewer->Widget(), [ctx]() { ctx->RefreshStatus(); });
      }
      else
      {
        r->ReplaceResource(from, to);
        GUIInvoke::call(viewer->Widget(), [ctx]() { ctx->RefreshStatus(); });
      }
    });
  };

  auto closeCallback = [id](ICaptureContext *ctx) {
    // remove the replacement on close (we could make this more sophisticated if there
    // was a place to control replaced resources/shaders).
    ctx->Replay().AsyncInvoke([ctx, id](IReplayController *r) {
      r->RemoveReplacement(id);
      GUIInvoke::call(ctx->GetMainWindow()->Widget(), [ctx] { ctx->RefreshStatus(); });
    });
  };

  IShaderViewer *sv = m_Ctx.EditShader(false, shaderType, entry, files, encoding, compileFlags,
                                       saveCallback, closeCallback);

  m_Ctx.AddDockWindow(sv->Widget(), DockReference::AddTo, this);

  return sv;
}

IShaderViewer *PipelineStateViewer::EditOriginalShaderSource(ResourceId id,
                                                             const ShaderReflection *shaderDetails)
{
  QSet<uint> uniqueFiles;
  rdcstrpairs files;

  for(const ShaderSourceFile &s : shaderDetails->debugInfo.files)
  {
    QString filename = s.filename;

    uint filenameHash = qHash(filename.toLower());

    if(uniqueFiles.contains(filenameHash))
    {
      qWarning() << lit("Duplicate full filename") << filename;
      continue;
    }
    uniqueFiles.insert(filenameHash);

    files.push_back(make_rdcpair(s.filename, s.contents));
  }

  return EditShader(id, shaderDetails->stage, shaderDetails->entryPoint,
                    shaderDetails->debugInfo.compileFlags, shaderDetails->debugInfo.encoding, files);
}

IShaderViewer *PipelineStateViewer::EditDecompiledSource(const ShaderProcessingTool &tool,
                                                         ResourceId id,
                                                         const ShaderReflection *shaderDetails)
{
  ShaderToolOutput out = tool.DisassembleShader(this, shaderDetails, "");

  rdcstr source;
  source.assign((const char *)out.result.data(), out.result.size());

  rdcstrpairs files;
  files.push_back(make_rdcpair<rdcstr, rdcstr>("decompiled", source));

  IShaderViewer *sv = EditShader(id, shaderDetails->stage, shaderDetails->entryPoint,
                                 shaderDetails->debugInfo.compileFlags, tool.output, files);

  sv->ShowErrors(out.log);

  return sv;
}

void PipelineStateViewer::SetupShaderEditButton(QToolButton *button, ResourceId pipelineId,
                                                ResourceId shaderId,
                                                const ShaderReflection *shaderDetails)
{
  if(!shaderDetails || !button->isEnabled() || button->popupMode() != QToolButton::MenuButtonPopup)
    return;

  QMenu *menu = editMenus[(int)shaderDetails->stage];

  menu->clear();

  rdcarray<ShaderEncoding> accepted = m_Ctx.TargetShaderEncodings();

  // if we have original source and it's in a known format, display it as the first most preferred
  // option
  if(!shaderDetails->debugInfo.files.empty() &&
     shaderDetails->debugInfo.encoding != ShaderEncoding::Unknown)
  {
    QAction *action =
        new QAction(tr("Edit Source - %1").arg(shaderDetails->debugInfo.files[0].filename), menu);
    action->setIcon(Icons::page_white_edit());

    QObject::connect(action, &QAction::triggered, [this, shaderId, shaderDetails]() {
      EditOriginalShaderSource(shaderId, shaderDetails);
    });

    menu->addAction(action);
  }

  // next up, try the shader processing tools in order - all the ones that will decompile from our
  // native representation. We don't check here yet if we have a valid compiler to compile from the
  // output to what we want.
  for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
  {
    // skip tools that can't decode our shader, or doesn't produce a textual output
    if(tool.input != shaderDetails->encoding || !IsTextRepresentation(tool.output))
      continue;

    QAction *action = new QAction(tr("Decompile with %1").arg(tool.name), menu);
    action->setIcon(Icons::page_white_edit());

    QObject::connect(action, &QAction::triggered, [this, tool, shaderId, shaderDetails]() {
      EditDecompiledSource(tool, shaderId, shaderDetails);
    });

    menu->addAction(action);
  }

  // if all else fails we can generate a stub for editing. Skip this for GLSL as it always has
  // source above which is preferred.
  if(shaderDetails->encoding != ShaderEncoding::GLSL)
  {
    QString label = tr("Edit Generated Stub");

    if(shaderDetails->encoding == ShaderEncoding::SPIRV)
      label = tr("Edit Pseudocode");

    QAction *action = new QAction(label, menu);
    action->setIcon(Icons::page_white_edit());

    QObject::connect(action, &QAction::triggered, [this, pipelineId, shaderId, shaderDetails]() {
      QString entry;
      QString src;

      if(shaderDetails->encoding == ShaderEncoding::SPIRV)
      {
        m_Ctx.Replay().AsyncInvoke([this, pipelineId, shaderId, shaderDetails](IReplayController *r) {
          rdcstr disasm = r->DisassembleShader(pipelineId, shaderDetails, "");

          QString editeddisasm =
              tr("####          PSEUDOCODE SPIR-V DISASSEMBLY            ###\n") +
              tr("#### Use a SPIR-V decompiler to get compileable source ###\n\n");

          editeddisasm += disasm;

          GUIInvoke::call(this, [this, shaderId, shaderDetails, editeddisasm]() {
            rdcstrpairs files;
            files.push_back(make_rdcpair<rdcstr, rdcstr>("pseudocode", editeddisasm));

            EditShader(shaderId, shaderDetails->stage, shaderDetails->entryPoint,
                       ShaderCompileFlags(), ShaderEncoding::Unknown, files);
          });
        });
      }
      else if(shaderDetails->encoding == ShaderEncoding::DXBC)
      {
        entry = lit("EditedShader%1S").arg(ToQStr(shaderDetails->stage, GraphicsAPI::D3D11)[0]);

        rdcstrpairs files;
        files.push_back(make_rdcpair<rdcstr, rdcstr>("decompiled_stub.hlsl",
                                                     GenerateHLSLStub(shaderDetails, entry)));

        EditShader(shaderId, shaderDetails->stage, entry, ShaderCompileFlags(),
                   ShaderEncoding::HLSL, files);
      }

    });

    menu->addAction(action);
  }

  button->setMenu(menu);
}

QString PipelineStateViewer::GetVBufferFormatString(uint32_t slot)
{
  rdcarray<BoundVBuffer> vbs = m_Ctx.CurPipelineState().GetVBuffers();
  rdcarray<VertexInputAttribute> attrs = m_Ctx.CurPipelineState().GetVertexInputs();

  if(slot >= vbs.size())
    return tr("// Unbound vertex buffer slot %1").arg(slot);

  uint32_t stride = vbs[slot].byteStride;

  // filter attributes to only the ones enabled and using this slot
  for(size_t i = 0; i < attrs.size();)
  {
    if(!attrs[i].used || attrs[i].vertexBuffer != (int)slot)
    {
      attrs.erase(i);
      // continue with same i
    }
    else
    {
      // move to next i
      i++;
    }
  }

  // we now have all attributes in this buffer. Sort by offset
  std::sort(attrs.begin(), attrs.end(),
            [](const VertexInputAttribute &a, const VertexInputAttribute &b) {
              return a.byteOffset < b.byteOffset;
            });

  // ensure we don't have any overlap between attributes or with the stride
  for(size_t i = 0; i < attrs.size(); i++)
  {
    uint32_t cursz = byteSize(attrs[i].format);
    if(cursz == 0)
      return tr("// Unhandled vertex attribute type '%1'").arg(ToQStr(attrs[i].format.type));

    // for all but the first attribute, ensure no overlaps with previous. We allow identical
    // elements
    if(i > 0)
    {
      uint32_t prevsz = byteSize(attrs[i - 1].format);

      if((attrs[i - 1].byteOffset != attrs[i].byteOffset || cursz != prevsz) &&
         attrs[i - 1].byteOffset + prevsz > attrs[i].byteOffset)
        return tr("// vertex attributes overlapping, no automatic format available");
    }

    if(i + 1 == attrs.size())
    {
      // for the last attribute, ensure the total size doesn't overlap stride
      if(attrs[i].byteOffset + cursz > stride && stride > 0)
        return tr("// vertex stride %1 less than total data fetched %2")
            .arg(stride)
            .arg(attrs[i].byteOffset + cursz);
    }
  }

  QString format;

  uint32_t offset = 0;

  for(size_t i = 0; i < attrs.size(); i++)
  {
    // we disallowed overlaps above, but we do allow *duplicates*. So if our offset has already
    // passed, silently skip this element.
    if(attrs[i].byteOffset < offset)
      continue;

    // declare any padding from previous element to this one
    format += padding(attrs[i].byteOffset - offset);

    const ResourceFormat &fmt = attrs[i].format;

    offset += byteSize(fmt);

    if(fmt.Special())
    {
      switch(fmt.type)
      {
        case ResourceFormatType::R10G10B10A2:
          if(fmt.compType == CompType::UInt)
            format += lit("uintten");
          if(fmt.compType == CompType::UNorm)
            format += lit("unormten");
          break;
        case ResourceFormatType::R11G11B10: format += lit("floateleven"); break;
        default: format += tr("// unknown type "); break;
      }
    }
    else
    {
      QChar widthchar[] = {
          QLatin1Char('?'), QLatin1Char('b'), QLatin1Char('h'), QLatin1Char('?'), QLatin1Char('f'),
      };

      if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
      {
        format += lit("unorm%1").arg(widthchar[fmt.compByteWidth]);
      }
      else if(fmt.compType == CompType::SNorm)
      {
        format += lit("snorm%1").arg(widthchar[fmt.compByteWidth]);
      }
      else
      {
        if(fmt.compType == CompType::UInt)
          format += lit("u");

        if(fmt.compByteWidth == 1)
        {
          format += lit("byte");
        }
        else if(fmt.compByteWidth == 2)
        {
          if(fmt.compType == CompType::Float)
            format += lit("half");
          else
            format += lit("short");
        }
        else if(fmt.compByteWidth == 4)
        {
          if(fmt.compType == CompType::Float)
            format += lit("float");
          else
            format += lit("int");
        }
        else if(fmt.compByteWidth == 8)
        {
          format += lit("double");
        }
      }

      format += QString::number(fmt.compCount);
    }

    QString real_name = QString(attrs[i].name);
    QString sanitised_name = real_name;

    sanitised_name.replace(QLatin1Char('.'), QLatin1Char('_'))
        .replace(QLatin1Char('['), QLatin1Char('_'))
        .replace(QLatin1Char(']'), QLatin1Char('_'));

    if(real_name == sanitised_name)
      format += QFormatStr(" %1;\n").arg(sanitised_name);
    else
      format += QFormatStr(" %1; // %2\n").arg(sanitised_name).arg(real_name);
  }

  if(stride > 0)
    format += padding(stride - offset);

  return format;
}

bool PipelineStateViewer::SaveShaderFile(const ShaderReflection *shader)
{
  if(!shader)
    return false;

  QString filter;

  if(m_Ctx.CurPipelineState().IsCaptureD3D11() || m_Ctx.CurPipelineState().IsCaptureD3D12())
  {
    filter = tr("DXBC Shader files (*.dxbc)");
  }
  else if(m_Ctx.CurPipelineState().IsCaptureGL())
  {
    filter = tr("GLSL files (*.glsl)");
  }
  else if(m_Ctx.CurPipelineState().IsCaptureVK())
  {
    filter = tr("SPIR-V files (*.spv)");
  }

  QString filename = RDDialog::getSaveFileName(this, tr("Save Shader As"), QString(), filter);

  if(!filename.isEmpty())
  {
    ANALYTIC_SET(Export.Shader, true);

    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        f.write((const char *)shader->rawBytes.data(), (qint64)shader->rawBytes.size());
      }
      else
      {
        RDDialog::critical(
            this, tr("Error saving shader"),
            tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
        return false;
      }
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
      return false;
    }
  }

  return true;
}
