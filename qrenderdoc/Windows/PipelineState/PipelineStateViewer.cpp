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

#include "PipelineStateViewer.h"
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStylePainter>
#include <QSvgRenderer>
#include <QToolButton>
#include <QXmlStreamWriter>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDTreeWidget.h"
#include "toolwindowmanager/ToolWindowManager.h"
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
      case ResourceFormatType::A8: return 1;
      case ResourceFormatType::R10G10B10A2:
      case ResourceFormatType::R11G11B10: return 4;
    }
  }

  return fmt.compByteWidth * fmt.compCount;
}

RDPreviewTooltip::RDPreviewTooltip(PipelineStateViewer *parent, CustomPaintWidget *thumbnail,
                                   ICaptureContext &ctx)
    : QFrame(parent), m_Ctx(ctx)
{
  int margin = style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, NULL, this);
  int opacity = style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, NULL, this);

  pipe = parent;

  setWindowFlags(Qt::ToolTip | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setForegroundRole(QPalette::ToolTipText);
  setBackgroundRole(QPalette::ToolTipBase);
  setFrameStyle(QFrame::NoFrame);
  setWindowOpacity(opacity / 255.0);
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  QHBoxLayout *hbox = new QHBoxLayout;
  QVBoxLayout *vbox = new QVBoxLayout;
  hbox->setSpacing(0);
  hbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(2);
  vbox->setContentsMargins(6, 3, 6, 3);

  label = new QLabel(this);
  label->setAlignment(Qt::AlignLeft);

  title = new QLabel(this);
  title->setAlignment(Qt::AlignLeft);

  setLayout(vbox);
  vbox->addWidget(title);
  vbox->addLayout(hbox);

  hbox->addWidget(thumbnail);
  hbox->addStretch();

  vbox->addWidget(label);
}

void RDPreviewTooltip::hideTip()
{
  hide();
}

QSize RDPreviewTooltip::configureTip(QWidget *widget, QModelIndex idx, QString text)
{
  ResourceId id = pipe->updateThumbnail(widget, idx);
  if(id != ResourceId())
  {
    title->setText(m_Ctx.GetResourceName(id));
    title->show();
  }
  else
  {
    title->hide();
  }
  label->setText(text);
  label->setVisible(!text.isEmpty());
  layout()->update();
  layout()->activate();
  return minimumSizeHint();
}

void RDPreviewTooltip::showTip(QPoint pos)
{
  move(pos);
  resize(minimumSize());
  show();
}

bool RDPreviewTooltip::forceTip(QWidget *widget, QModelIndex idx)
{
  return pipe->hasThumbnail(widget, idx);
}

void RDPreviewTooltip::paintEvent(QPaintEvent *ev)
{
  QStylePainter p(this);
  QStyleOptionFrame opt;
  opt.init(this);
  p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
  p.end();

  QWidget::paintEvent(ev);
}

void RDPreviewTooltip::resizeEvent(QResizeEvent *e)
{
  QStyleHintReturnMask frameMask;
  QStyleOption option;
  option.init(this);
  if(style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &frameMask))
    setMask(frameMask.region);

  QWidget::resizeEvent(e);
}

PipelineStateViewer::PipelineStateViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->thumbnail->SetContext(m_Ctx);
  ui->layout->removeWidget(ui->thumbnail);

  m_Tooltip = new RDPreviewTooltip(this, ui->thumbnail, m_Ctx);

  QColor c = palette().color(QPalette::ToolTipBase).toRgb();

  m_TexDisplay.backgroundColor = FloatVector();
  m_TexDisplay.backgroundColor.w = 1.0f;

  // auto-fit and center scale
  m_TexDisplay.scale = -1.0f;

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;

  for(size_t i = 0; i < ARRAY_COUNT(editMenus); i++)
    editMenus[i] = new QMenu(this);

  m_Ctx.AddCaptureViewer(this);
}

PipelineStateViewer::~PipelineStateViewer()
{
  reset();

  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);

  delete m_Tooltip;

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

  if(!m_Ctx.APIProps().remoteReplay)
  {
    WindowingData thumbData = ui->thumbnail->GetWidgetWindowingData();

    m_Ctx.Replay().BlockInvoke([thumbData, this](IReplayController *r) {
      m_Output = r->CreateOutput(thumbData, ReplayOutputType::Texture);

      ui->thumbnail->SetOutput(m_Output);

      RT_UpdateAndDisplay(r);
    });
  }
  else
  {
    m_Output = NULL;
  }
}

void PipelineStateViewer::RT_UpdateAndDisplay(IReplayController *r)
{
  if(m_Output != NULL)
  {
    m_Output->SetTextureDisplay(m_TexDisplay);

    GUIInvoke::call(this, [this]() { ui->thumbnail->update(); });
  }
}

void PipelineStateViewer::OnCaptureClosed()
{
  if(m_Current)
    m_Current->OnCaptureClosed();
}

void PipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  RENDERDOC_PROFILEFUNCTION();

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
            uint32_t frameNumber = m_Ctx.FrameInfo().frameNumber;

            QString context = frameNumber == ~0U ? tr("Capture") : tr("Frame %1").arg(frameNumber);

            const ActionDescription *action = m_Ctx.CurAction();

            QList<const ActionDescription *> actionstack;
            const ActionDescription *parent = action ? action->parent : NULL;
            while(parent)
            {
              actionstack.push_front(parent);
              parent = parent->parent;
            }

            for(const ActionDescription *d : actionstack)
            {
              context += QFormatStr(" > %1").arg(d->customName);
            }

            if(action)
              context +=
                  QFormatStr(" => %1").arg(m_Ctx.GetEventBrowser()->GetEventName(action->eventId));
            else
              context += tr(" => Capture Start");

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

void PipelineStateViewer::MakeShaderVariablesHLSL(bool cbufferContents,
                                                  const rdcarray<ShaderConstant> &vars,
                                                  QString &struct_contents, QString &struct_defs)
{
  for(const ShaderConstant &v : vars)
  {
    if(v.type.baseType == VarType::Struct)
    {
      QString def = lit("struct %1 {\n").arg(v.type.name);

      if(!struct_defs.contains(def))
      {
        QString contents;
        MakeShaderVariablesHLSL(false, v.type.members, contents, struct_defs);

        struct_defs += def + contents + lit("};\n\n");
      }
    }

    if(v.type.elements > 1)
    {
      struct_contents += lit("\t%1 %2[%3]").arg(v.type.name).arg(v.name).arg(v.type.elements);
    }
    else
    {
      struct_contents += lit("\t%1 %2").arg(v.type.name).arg(v.name);
    }

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

void PipelineStateViewer::showEvent(QShowEvent *event)
{
  // we didn't set any default pipeline state in case it would be overridden by the persist data.
  // But if we don't have any persist data and we're about to show, default to D3D11.
  if(m_Current == NULL)
  {
    setToD3D11();
  }
}

void PipelineStateViewer::SetStencilLabelValue(QLabel *label, uint8_t value)
{
  label->setText(Formatter::Format(value, true));
  label->setToolTip(tr("%1 / 0x%2 / 0b%3")
                        .arg(value, 3, 10, QLatin1Char(' '))
                        .arg(Formatter::Format(value, true))
                        .arg(value, 8, 2, QLatin1Char('0')));
}

void PipelineStateViewer::SetStencilTreeItemValue(RDTreeWidgetItem *item, int column, uint8_t value)
{
  item->setText(column, Formatter::Format(value, true));
  item->setToolTip(column, tr("%1 / 0x%2 / 0b%3")
                               .arg(value, 3, 10, QLatin1Char(' '))
                               .arg(Formatter::Format(value, true))
                               .arg(value, 8, 2, QLatin1Char('0')));
}

QString PipelineStateViewer::GenerateHLSLStub(const ShaderReflection *shaderDetails,
                                              const QString &entryFunc)
{
  QString hlsl = lit("// HLSL function stub generated\n\n");

  const QString textureDim[arraydim<TextureType>()] = {
      lit("Unknown"),          lit("Buffer"),      lit("Texture1D"),      lit("Texture1DArray"),
      lit("Texture2D"),        lit("TextureRect"), lit("Texture2DArray"), lit("Texture2DMS"),
      lit("Texture2DMSArray"), lit("Texture3D"),   lit("TextureCube"),    lit("TextureCubeArray"),
  };

  for(const ShaderSampler &samp : shaderDetails->samplers)
  {
    hlsl += lit("SamplerState %1 : register(s%2); // can't disambiguate\n"
                "//SamplerComparisonState %1 : register(s%2); // can't disambiguate\n")
                .arg(samp.name)
                .arg(samp.fixedBindNumber);
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
                    .arg(textureDim[(size_t)res.textureType])
                    .arg(res.variableType.name)
                    .arg(res.name)
                    .arg(QLatin1Char(regChar))
                    .arg(res.fixedBindNumber);
      }
      else
      {
        if(res.variableType.rows > 1)
          hlsl += lit("Structured");

        hlsl += lit("Buffer<%1> %2 : register(%3%4);\n")
                    .arg(res.variableType.name)
                    .arg(res.name)
                    .arg(QLatin1Char(regChar))
                    .arg(res.fixedBindNumber);
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
      cbuffers += lit("cbuffer %1 : register(b%2) {\n").arg(cbufName).arg(cbuf.fixedBindNumber);
      MakeShaderVariablesHLSL(true, cbuf.variables, cbuffers, hlsl);
      cbuffers += lit("};\n\n");
    }
    cbufIdx++;
  }

  hlsl += cbuffers;

  hlsl += lit("\n\n");

  hlsl += lit("struct InputStruct {\n");
  for(const SigParameter &sig : shaderDetails->inputSignature)
  {
    QString name = !sig.varName.isEmpty() ? QString(sig.varName) : lit("param%1").arg(sig.regIndex);

    if(sig.varName.isEmpty() && sig.systemValue != ShaderBuiltin::Undefined)
      name = D3DSemanticString(sig).replace(lit("SV_"), QString());

    hlsl += lit("\t%1 %2 : %3;\n").arg(TypeString(sig)).arg(name).arg(D3DSemanticString(sig));
  }
  hlsl += lit("};\n\n");

  hlsl += lit("struct OutputStruct {\n");
  for(const SigParameter &sig : shaderDetails->outputSignature)
  {
    QString name = !sig.varName.isEmpty() ? QString(sig.varName) : lit("param%1").arg(sig.regIndex);

    if(sig.varName.isEmpty() && sig.systemValue != ShaderBuiltin::Undefined)
      name = D3DSemanticString(sig).replace(lit("SV_"), QString());

    hlsl += lit("\t%1 %2 : %3;\n").arg(TypeString(sig)).arg(name).arg(D3DSemanticString(sig));
  }

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
                                               KnownShaderTool knownTool,
                                               ShaderEncoding shaderEncoding,
                                               const rdcstrpairs &files)
{
  IShaderViewer *sv = m_Ctx.EditShader(id, shaderType, entry, files, knownTool, shaderEncoding,
                                       compileFlags, NULL, NULL);

  m_Ctx.AddDockWindow(sv->Widget(), DockReference::AddTo, this);

  return sv;
}

IShaderViewer *PipelineStateViewer::EditOriginalShaderSource(ResourceId id,
                                                             const ShaderReflection *shaderDetails)
{
  QSet<uint> uniqueFiles;
  rdcstrpairs files;

  // add the entry point file first, if we have one
  const int entryFile = shaderDetails->debugInfo.editBaseFile;

  for(int i = -1; i < shaderDetails->debugInfo.files.count(); i++)
  {
    int idx = i;
    if(idx < 0)
      idx = entryFile;
    else if(idx == entryFile)
      continue;

    if(idx < 0)
      continue;

    const ShaderSourceFile &s = shaderDetails->debugInfo.files[idx];

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

  return EditShader(id, shaderDetails->stage, shaderDetails->debugInfo.entrySourceName,
                    shaderDetails->debugInfo.compileFlags, shaderDetails->debugInfo.compiler,
                    shaderDetails->debugInfo.encoding, files);
}

IShaderViewer *PipelineStateViewer::EditDecompiledSource(const ShaderProcessingTool &tool,
                                                         ResourceId id,
                                                         const ShaderReflection *shaderDetails)
{
  ShaderToolOutput out = tool.DisassembleShader(this, shaderDetails, "");

  rdcstr source;
  source.assign((const char *)out.result.data(), out.result.size());

  rdcstrpairs files;
  files.push_back(rdcpair<rdcstr, rdcstr>("decompiled", source));

  ShaderCompileFlags flags;

  for(const ShaderCompileFlag &flag : shaderDetails->debugInfo.compileFlags.flags)
    if(flag.name == "@spirver")
      flags.flags.push_back(flag);

  IShaderViewer *sv = EditShader(id, shaderDetails->stage, shaderDetails->debugInfo.entrySourceName,
                                 flags, KnownShaderTool::Unknown, tool.output, files);

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

  const ShaderDebugInfo &dbg = shaderDetails->debugInfo;

  // if we have original source and it's in a known format, display it as the first most preferred
  // option
  if(!dbg.files.empty() && dbg.encoding != ShaderEncoding::Unknown)
  {
    int entryFile = qMax(0, dbg.entryLocation.fileIndex);
    if(dbg.editBaseFile >= 0 && dbg.editBaseFile < dbg.files.count())
      entryFile = dbg.editBaseFile;
    QAction *action = new QAction(tr("Edit Source - %1").arg(dbg.files[entryFile].filename), menu);
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

    if(shaderDetails->encoding == ShaderEncoding::SPIRV ||
       shaderDetails->encoding == ShaderEncoding::OpenGLSPIRV)
      label = tr("Edit Pseudocode");

    QAction *action = new QAction(label, menu);
    action->setIcon(Icons::page_white_edit());

    QObject::connect(action, &QAction::triggered, [this, pipelineId, shaderId, shaderDetails]() {
      QString entry;
      QString src;

      if(shaderDetails->encoding == ShaderEncoding::SPIRV ||
         shaderDetails->encoding == ShaderEncoding::OpenGLSPIRV)
      {
        m_Ctx.Replay().AsyncInvoke([this, pipelineId, shaderId, shaderDetails](IReplayController *r) {
          rdcstr disasm = r->DisassembleShader(pipelineId, shaderDetails, "");

          QString editeddisasm =
              tr("####          PSEUDOCODE SPIR-V DISASSEMBLY            ###\n") +
              tr("#### Use a SPIR-V decompiler to get compileable source ###\n\n");

          editeddisasm += disasm;

          GUIInvoke::call(this, [this, shaderId, shaderDetails, editeddisasm]() {
            rdcstrpairs files;
            files.push_back(rdcpair<rdcstr, rdcstr>("pseudocode", editeddisasm));

            EditShader(shaderId, shaderDetails->stage, shaderDetails->entryPoint,
                       shaderDetails->debugInfo.compileFlags, KnownShaderTool::Unknown,
                       ShaderEncoding::Unknown, files);
          });
        });
      }
      else if(shaderDetails->encoding == ShaderEncoding::DXBC ||
              shaderDetails->encoding == ShaderEncoding::DXIL)
      {
        entry = lit("EditedShader%1S").arg(ToQStr(shaderDetails->stage, GraphicsAPI::D3D11)[0]);

        rdcstrpairs files;
        files.push_back(rdcpair<rdcstr, rdcstr>("decompiled_stub.hlsl",
                                                GenerateHLSLStub(shaderDetails, entry)));

        EditShader(shaderId, shaderDetails->stage, entry, shaderDetails->debugInfo.compileFlags,
                   KnownShaderTool::Unknown, ShaderEncoding::HLSL, files);
      }
    });

    menu->addAction(action);
  }

  button->setMenu(menu);
}

void PipelineStateViewer::AddResourceUsageEntry(QMenu &menu, uint32_t start, uint32_t end,
                                                ResourceUsage usage)
{
  QAction *item = NULL;

  if(start == end)
    item = new QAction(
        QFormatStr("EID %1: %2").arg(start).arg(ToQStr(usage, m_Ctx.APIProps().pipelineType)), this);
  else
    item = new QAction(
        QFormatStr("EID %1-%2: %3").arg(start).arg(end).arg(ToQStr(usage, m_Ctx.APIProps().pipelineType)),
        this);

  QObject::connect(item, &QAction::triggered, [this, end]() { m_Ctx.SetEventID({}, end, end); });

  menu.addAction(item);
}

void PipelineStateViewer::ShowResourceContextMenu(RDTreeWidget *widget, const QPoint &pos,
                                                  ResourceId id, const rdcarray<EventUsage> &usage)
{
  RDTreeWidgetItem *item = widget->itemAt(pos);

  QMenu contextMenu(this);

  QAction copy(tr("&Copy"), this);

  contextMenu.addAction(&copy);

  copy.setIcon(Icons::copy());

  QObject::connect(&copy, &QAction::triggered,
                   [widget, pos, item]() { widget->copyItem(pos, item); });

  QAction usageTitle(tr("Used:"), this);
  QAction openResourceInspector(tr("Open in Resource Inspector"), this);

  openResourceInspector.setIcon(Icons::link());

  if(id != ResourceId())
  {
    contextMenu.addSeparator();
    contextMenu.addAction(&openResourceInspector);
    contextMenu.addAction(&usageTitle);

    QObject::connect(&openResourceInspector, &QAction::triggered, [this, id]() {
      m_Ctx.ShowResourceInspector();

      m_Ctx.GetResourceInspector()->Inspect(id);
    });

    CombineUsageEvents(m_Ctx, usage,
                       [this, &contextMenu](uint32_t start, uint32_t end, ResourceUsage use) {
                         AddResourceUsageEntry(contextMenu, start, end, use);
                       });
  }

  RDDialog::show(&contextMenu, widget->viewport()->mapToGlobal(pos));
}

ResourceId PipelineStateViewer::updateThumbnail(QWidget *widget, QModelIndex idx)
{
  ResourceId id;

  if(!m_Output)
    return id;

  RDTreeWidget *treeWidget = qobject_cast<RDTreeWidget *>(widget);
  if(treeWidget)
  {
    RDTreeWidgetItem *item = treeWidget->itemForIndex(idx);

    if(item)
    {
      if(m_D3D11)
        id = m_D3D11->GetResource(item);
      else if(m_D3D12)
        id = m_D3D12->GetResource(item);
      else if(m_GL)
        id = m_GL->GetResource(item);
      else if(m_Vulkan)
        id = m_Vulkan->GetResource(item);
    }

    TextureDescription *tex = m_Ctx.GetTexture(id);

    if(tex)
    {
      m_TexDisplay.resourceId = id;
      INVOKE_MEMFN(RT_UpdateAndDisplay);

      float aspect = (float)tex->width / (float)qMax(1U, tex->height);

      // keep height fixed at 100, and make width match the aspect ratio of the texture - up to 21:9
      // ratio
      ui->thumbnail->setFixedSize((int)qBound(100.0f, aspect * 100.0f, (21.0f / 9.0f) * 100.0f), 100);
      ui->thumbnail->show();
    }
    else
    {
      ui->thumbnail->hide();
    }
  }

  return id;
}

bool PipelineStateViewer::hasThumbnail(QWidget *widget, QModelIndex idx)
{
  if(!m_Output)
    return false;

  RDTreeWidget *treeWidget = qobject_cast<RDTreeWidget *>(widget);
  if(treeWidget)
  {
    ResourceId id;

    RDTreeWidgetItem *item = treeWidget->itemForIndex(idx);

    if(item)
    {
      if(m_D3D11)
        id = m_D3D11->GetResource(item);
      else if(m_D3D12)
        id = m_D3D12->GetResource(item);
      else if(m_GL)
        id = m_GL->GetResource(item);
      else if(m_Vulkan)
        id = m_Vulkan->GetResource(item);
    }

    if(id != ResourceId() && m_Ctx.GetTexture(id))
      return true;
  }

  return false;
}

void PipelineStateViewer::SetupResourceView(RDTreeWidget *widget)
{
  auto handler = [this, widget](const QPoint &pos) {
    RDTreeWidgetItem *item = widget->itemAt(pos);

    ResourceId id;

    if(m_D3D11)
      id = m_D3D11->GetResource(item);
    else if(m_D3D12)
      id = m_D3D12->GetResource(item);
    else if(m_GL)
      id = m_GL->GetResource(item);
    else if(m_Vulkan)
      id = m_Vulkan->GetResource(item);

    if(id != ResourceId())
    {
      m_Ctx.Replay().AsyncInvoke([this, widget, pos, id](IReplayController *r) {
        rdcarray<EventUsage> usage = r->GetUsage(id);

        GUIInvoke::call(this, [this, widget, pos, id, usage]() {
          ShowResourceContextMenu(widget, pos, id, usage);
        });
      });
    }
    else
    {
      ShowResourceContextMenu(widget, pos, id, {});
    }
  };

  widget->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(widget, &RDTreeWidget::customContextMenuRequested, handler);

  widget->setCustomTooltip(m_Tooltip);
}

QString PipelineStateViewer::GetVBufferFormatString(uint32_t slot)
{
  rdcarray<BoundVBuffer> vbs = m_Ctx.CurPipelineState().GetVBuffers();
  rdcarray<VertexInputAttribute> attrs = m_Ctx.CurPipelineState().GetVertexInputs();

  if(slot >= vbs.size())
    return tr("// Unbound vertex buffer slot %1").arg(slot);

  uint32_t stride = vbs[slot].byteStride;

  // filter attributes to only the ones enabled and using this slot
  attrs.removeIf([slot](const VertexInputAttribute &attr) {
    return (!attr.used || attr.vertexBuffer != (int)slot);
  });

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

  format = lit("struct vbuffer {\n");

  for(size_t i = 0; i < attrs.size(); i++)
  {
    // we disallowed overlaps above, but we do allow *duplicates*. So if our offset has already
    // passed, silently skip this element.
    if(attrs[i].byteOffset < offset)
      continue;

    // declare an explicit offset if there's a gap from previous element to this one
    if(attrs[i].byteOffset > offset)
      format += lit("  [[offset(%1)]]\n").arg(attrs[i].byteOffset);

    format += lit("  ");

    const ResourceFormat &fmt = attrs[i].format;

    offset = attrs[i].byteOffset + byteSize(fmt);

    if(fmt.Special())
    {
      switch(fmt.type)
      {
        case ResourceFormatType::R10G10B10A2:
          if(fmt.compType == CompType::UNorm)
            format += lit("[[packed(r10g10b10a2)]] [[unorm]] uint4");
          else
            format += lit("[[packed(r10g10b10a2)]] uint4");
          break;
        case ResourceFormatType::R11G11B10: format += lit("[[packed(r11g11b10)]] float3"); break;
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
        format += lit("[[unorm]]");
      }
      else if(fmt.compType == CompType::SNorm)
      {
        format += lit("[[snorm]]");
      }

      if(fmt.compType == CompType::UInt || fmt.compType == CompType::UNorm ||
         fmt.compType == CompType::UNormSRGB)
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
        if(fmt.compType == CompType::Float)
          format += lit("double");
        else
          format += lit("long");
      }

      format += QString::number(fmt.compCount);
    }

    QString real_name = QString(attrs[i].name);
    QString sanitised_name = real_name;

    sanitised_name.replace(QLatin1Char('.'), QLatin1Char('_'))
        .replace(QLatin1Char(':'), QLatin1Char('_'))
        .replace(QLatin1Char('['), QLatin1Char('_'))
        .replace(QLatin1Char(']'), QLatin1Char('_'));

    if(real_name == sanitised_name)
      format += QFormatStr(" %1;\n").arg(sanitised_name);
    else
      format += QFormatStr(" %1; // %2\n").arg(sanitised_name).arg(real_name);
  }

  format += lit("}\n\nvbuffer vertex[];");

  if(stride > offset)
    format = lit("[[size(%1)]]\n").arg(stride) + format;

  format = lit("#pack(scalar) // vertex buffers can be tightly packed\n"
               "\n") +
           format;

  return format;
}

QColor PipelineStateViewer::GetViewDetailsColor()
{
  return QColor::fromHslF(0.45f, 1.0f,
                          qBound(0.25, palette().color(QPalette::Base).lightnessF(), 0.75));
}

bool PipelineStateViewer::SaveShaderFile(const ShaderReflection *shader)
{
  if(!shader)
    return false;

  QString filter;

  switch(shader->encoding)
  {
    case ShaderEncoding::DXBC: filter = tr("DXBC Shader files (*.dxbc)"); break;
    case ShaderEncoding::HLSL: filter = tr("HLSL files (*.hlsl)"); break;
    case ShaderEncoding::GLSL: filter = tr("GLSL files (*.glsl)"); break;
    case ShaderEncoding::SPIRV:
    case ShaderEncoding::OpenGLSPIRV: filter = tr("SPIR-V files (*.spv)"); break;
    case ShaderEncoding::SPIRVAsm:
    case ShaderEncoding::OpenGLSPIRVAsm: filter = tr("SPIR-V assembly files (*.spvasm)"); break;
    case ShaderEncoding::DXIL: filter = tr("DXIL Shader files (*.dxbc)"); break;
    case ShaderEncoding::Slang: filter = tr("Slang Shader files (*.slang)"); break;
    case ShaderEncoding::Unknown:
    case ShaderEncoding::Count: filter = tr("All files (*.*)"); break;
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

void PipelineStateViewer::SelectPipelineStage(PipelineStage stage)
{
  if(m_D3D11)
    m_D3D11->SelectPipelineStage(stage);
  else if(m_D3D12)
    m_D3D12->SelectPipelineStage(stage);
  else if(m_GL)
    m_GL->SelectPipelineStage(stage);
  else if(m_Vulkan)
    m_Vulkan->SelectPipelineStage(stage);
}

ScopedTreeUpdater::ScopedTreeUpdater(RDTreeWidget *widget) : m_Widget(widget)
{
  vs = m_Widget->verticalScrollBar()->value();
  m_Widget->beginUpdate();
  m_Widget->clear();
}

ScopedTreeUpdater::~ScopedTreeUpdater()
{
  m_Widget->clearSelection();
  m_Widget->endUpdate();
  m_Widget->verticalScrollBar()->setValue(vs);
}
