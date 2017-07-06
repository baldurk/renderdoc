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

#include "PipelineStateViewer.h"
#include <QMouseEvent>
#include <QXmlStreamWriter>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDLabel.h"
#include "D3D11PipelineStateViewer.h"
#include "D3D12PipelineStateViewer.h"
#include "GLPipelineStateViewer.h"
#include "VulkanPipelineStateViewer.h"
#include "ui_PipelineStateViewer.h"

PipelineStateViewer::PipelineStateViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;

  setToD3D11();

  m_Ctx.AddLogViewer(this);
}

PipelineStateViewer::~PipelineStateViewer()
{
  reset();

  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveLogViewer(this);

  delete ui;
}

void PipelineStateViewer::OnLogfileLoaded()
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
    m_Current->OnLogfileLoaded();
}

void PipelineStateViewer::OnLogfileClosed()
{
  if(m_Current)
    m_Current->OnLogfileClosed();
}

void PipelineStateViewer::OnEventChanged(uint32_t eventID)
{
  if(m_Ctx.CurPipelineState().DefaultType != m_Ctx.APIProps().pipelineType)
    OnLogfileLoaded();

  if(m_Current)
    m_Current->OnEventChanged(eventID);
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
  m_Ctx.CurPipelineState().DefaultType = GraphicsAPI::D3D11;
}

void PipelineStateViewer::setToD3D12()
{
  if(m_D3D12)
    return;

  reset();

  m_D3D12 = new D3D12PipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_D3D12);
  m_Current = m_D3D12;
  m_Ctx.CurPipelineState().DefaultType = GraphicsAPI::D3D12;
}

void PipelineStateViewer::setToGL()
{
  if(m_GL)
    return;

  reset();

  m_GL = new GLPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_GL);
  m_Current = m_GL;
  m_Ctx.CurPipelineState().DefaultType = GraphicsAPI::OpenGL;
}

void PipelineStateViewer::setToVulkan()
{
  if(m_Vulkan)
    return;

  reset();

  m_Vulkan = new VulkanPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_Vulkan);
  m_Current = m_Vulkan;
  m_Ctx.CurPipelineState().DefaultType = GraphicsAPI::Vulkan;
}

QXmlStreamWriter *PipelineStateViewer::beginHTMLExport()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Export pipeline state as HTML"), QString(),
                                               tr("HTML files (*.html)"));

  if(!filename.isEmpty())
  {
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
                            .arg(QFileInfo(m_Ctx.LogFilename()).fileName())
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
            const DrawcallDescription *parent = m_Ctx.GetDrawcall(draw->parent);
            while(parent)
            {
              drawstack.push_front(parent);
              parent = m_Ctx.GetDrawcall(parent->parent);
            }

            for(const DrawcallDescription *d : drawstack)
            {
              context += QFormatStr(" > %1").arg(ToQStr(d->name));
            }

            context += QFormatStr(" => %1").arg(ToQStr(draw->name));

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

          QMetaType::Type type = (QMetaType::Type)el.type();

          if(type == QMetaType::Bool)
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
    QImage im;
    switch(topo)
    {
      case Topology::PointList: im = Pixmaps::topo_pointlist(diagram).toImage(); break;
      case Topology::LineList: im = Pixmaps::topo_linelist(diagram).toImage(); break;
      case Topology::LineStrip: im = Pixmaps::topo_linestrip(diagram).toImage(); break;
      case Topology::TriangleList: im = Pixmaps::topo_trilist(diagram).toImage(); break;
      case Topology::TriangleStrip: im = Pixmaps::topo_tristrip(diagram).toImage(); break;
      case Topology::LineList_Adj: im = Pixmaps::topo_linelist_adj(diagram).toImage(); break;
      case Topology::LineStrip_Adj: im = Pixmaps::topo_linestrip_adj(diagram).toImage(); break;
      case Topology::TriangleList_Adj: im = Pixmaps::topo_trilist_adj(diagram).toImage(); break;
      case Topology::TriangleStrip_Adj: im = Pixmaps::topo_tristrip_adj(diagram).toImage(); break;
      default: im = Pixmaps::topo_patch(diagram).toImage(); break;
    }

    im = im.convertToFormat(QImage::Format_ARGB32);

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

bool PipelineStateViewer::PrepareShaderEditing(const ShaderReflection *shaderDetails,
                                               QString &entryFunc, QStringMap &files,
                                               QString &mainfile)
{
  if(!shaderDetails->DebugInfo.files.empty())
  {
    entryFunc = ToQStr(shaderDetails->EntryPoint);

    QStringList uniqueFiles;

    for(auto &s : shaderDetails->DebugInfo.files)
    {
      QString filename = ToQStr(s.first);
      if(uniqueFiles.contains(filename.toLower()))
      {
        qWarning() << lit("Duplicate full filename") << ToQStr(s.first);
        continue;
      }
      uniqueFiles.push_back(filename.toLower());

      files[filename] = ToQStr(s.second);
    }

    mainfile = ToQStr(shaderDetails->DebugInfo.files[0].first);

    return true;
  }

  return false;
}

void PipelineStateViewer::MakeShaderVariablesHLSL(bool cbufferContents,
                                                  const rdctype::array<ShaderConstant> &vars,
                                                  QString &struct_contents, QString &struct_defs)
{
  for(const ShaderConstant &v : vars)
  {
    if(v.type.members.count > 0)
    {
      QString def = lit("struct %1 {\n").arg(ToQStr(v.type.descriptor.name));

      if(!struct_defs.contains(def))
      {
        QString contents;
        MakeShaderVariablesHLSL(false, v.type.members, contents, struct_defs);

        struct_defs += def + contents + lit("};\n\n");
      }
    }

    struct_contents += lit("\t%1 %2").arg(ToQStr(v.type.descriptor.name)).arg(ToQStr(v.name));

    char comp = 'x';
    if(v.reg.comp == 1)
      comp = 'y';
    if(v.reg.comp == 2)
      comp = 'z';
    if(v.reg.comp == 3)
      comp = 'w';

    if(cbufferContents)
      struct_contents += lit(" : packoffset(c%1.%2);").arg(v.reg.vec).arg(QLatin1Char(comp));
    else
      struct_contents += lit(";");

    struct_contents += lit("\n");
  }
}

QString PipelineStateViewer::GenerateHLSLStub(const ShaderReflection *shaderDetails,
                                              const QString &entryFunc)
{
  QString hlsl = lit("// No HLSL available - function stub generated\n\n");

  const QString textureDim[ENUM_ARRAY_SIZE(TextureDim)] = {
      lit("Unknown"),          lit("Buffer"),      lit("Texture1D"),      lit("Texture1DArray"),
      lit("Texture2D"),        lit("TextureRect"), lit("Texture2DArray"), lit("Texture2DMS"),
      lit("Texture2DMSArray"), lit("Texture3D"),   lit("TextureCube"),    lit("TextureCubeArray"),
  };

  for(int i = 0; i < 2; i++)
  {
    const rdctype::array<ShaderResource> &resources =
        (i == 0 ? shaderDetails->ReadOnlyResources : shaderDetails->ReadWriteResources);
    for(const ShaderResource &res : resources)
    {
      if(res.IsSampler)
      {
        hlsl += lit("//SamplerComparisonState %1 : register(s%2); // can't disambiguate\n"
                    "SamplerState %1 : register(s%2); // can't disambiguate\n")
                    .arg(ToQStr(res.name))
                    .arg(res.bindPoint);
      }
      else
      {
        char regChar = 't';

        if(i == 1)
        {
          hlsl += lit("RW");
          regChar = 'u';
        }

        if(res.IsTexture)
        {
          hlsl += lit("%1<%2> %3 : register(%4%5);\n")
                      .arg(textureDim[(size_t)res.resType])
                      .arg(ToQStr(res.variableType.descriptor.name))
                      .arg(ToQStr(res.name))
                      .arg(QLatin1Char(regChar))
                      .arg(res.bindPoint);
        }
        else
        {
          if(res.variableType.descriptor.rows > 1)
            hlsl += lit("Structured");

          hlsl += lit("Buffer<%1> %2 : register(%3%4);\n")
                      .arg(ToQStr(res.variableType.descriptor.name))
                      .arg(ToQStr(res.name))
                      .arg(QLatin1Char(regChar))
                      .arg(res.bindPoint);
        }
      }
    }
  }

  hlsl += lit("\n\n");

  QString cbuffers;

  int cbufIdx = 0;
  for(const ConstantBlock &cbuf : shaderDetails->ConstantBlocks)
  {
    if(cbuf.name.count > 0 && cbuf.variables.count > 0)
    {
      QString cbufName = ToQStr(cbuf.name);
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
  for(const SigParameter &sig : shaderDetails->InputSig)
    hlsl += lit("\t%1 %2 : %3;\n")
                .arg(TypeString(sig))
                .arg(sig.varName.count > 0 ? ToQStr(sig.varName) : lit("param%1").arg(sig.regIndex))
                .arg(D3DSemanticString(sig));
  hlsl += lit("};\n\n");

  hlsl += lit("struct OutputStruct {\n");
  for(const SigParameter &sig : shaderDetails->OutputSig)
    hlsl += lit("\t%1 %2 : %3;\n")
                .arg(TypeString(sig))
                .arg(sig.varName.count > 0 ? ToQStr(sig.varName) : lit("param%1").arg(sig.regIndex))
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

void PipelineStateViewer::EditShader(ShaderStage shaderType, ResourceId id,
                                     const ShaderReflection *shaderDetails, const QString &entryFunc,
                                     const QStringMap &files, const QString &mainfile)
{
  IShaderViewer *sv = m_Ctx.EditShader(
      false, entryFunc, files,
      // save callback
      [entryFunc, mainfile, shaderType, id, shaderDetails](
          ICaptureContext *ctx, IShaderViewer *viewer, const QStringMap &updatedfiles) {
        QString compileSource = updatedfiles[mainfile];

        // try and match up #includes against the files that we have. This isn't always
        // possible as fxc only seems to include the source for files if something in
        // that file was included in the compiled output. So you might end up with
        // dangling #includes - we just have to ignore them
        int offs = compileSource.indexOf(lit("#include"));

        while(offs >= 0)
        {
          // search back to ensure this is a valid #include (ie. not in a comment).
          // Must only see whitespace before, then a newline.
          int ws = qMax(0, offs - 1);
          while(ws >= 0 &&
                (compileSource[ws] == QLatin1Char(' ') || compileSource[ws] == QLatin1Char('\t')))
            ws--;

          // not valid? jump to next.
          if(ws > 0 && compileSource[ws] != QLatin1Char('\n'))
          {
            offs = compileSource.indexOf(lit("#include"), offs + 1);
            continue;
          }

          int start = ws + 1;

          bool tail = true;

          int lineEnd = compileSource.indexOf(QLatin1Char('\n'), start + 1);
          if(lineEnd == -1)
          {
            lineEnd = compileSource.length();
            tail = false;
          }

          ws = offs + sizeof("#include") - 1;
          while(compileSource[ws] == QLatin1Char(' ') || compileSource[ws] == QLatin1Char('\t'))
            ws++;

          QString line = compileSource.mid(offs, lineEnd - offs + 1);

          if(compileSource[ws] != QLatin1Char('<') && compileSource[ws] != QLatin1Char('"'))
          {
            viewer->ShowErrors(lit("Invalid #include directive found:\r\n") + line);
            return;
          }

          // find matching char, either <> or "";
          int end = compileSource.indexOf(
              compileSource[ws] == QLatin1Char('"') ? QLatin1Char('"') : QLatin1Char('>'), ws + 1);

          if(end == -1)
          {
            viewer->ShowErrors(lit("Invalid #include directive found:\r\n") + line);
            return;
          }

          QString fname = compileSource.mid(ws + 1, end - ws - 1);

          QString fileText;

          // look for exact match first
          if(updatedfiles.contains(fname))
          {
            fileText = updatedfiles[fname];
          }
          else
          {
            QString search = QFileInfo(fname).fileName();
            // if not, try and find the same filename (this is not proper include handling!)
            for(const QString &k : updatedfiles.keys())
            {
              if(QFileInfo(k).fileName().compare(search, Qt::CaseInsensitive) == 0)
              {
                fileText = updatedfiles[k];
                break;
              }
            }

            if(fileText.isEmpty())
              fileText = QFormatStr("// Can't find file %1\n").arg(fname);
          }

          compileSource = compileSource.left(offs) + lit("\n\n") + fileText + lit("\n\n") +
                          (tail ? compileSource.mid(lineEnd + 1) : QString());

          // need to start searching from the beginning - wasteful but allows nested includes to
          // work
          offs = compileSource.indexOf(lit("#include"));
        }

        if(updatedfiles.contains(lit("@cmdline")))
          compileSource = updatedfiles[lit("@cmdline")] + lit("\n\n") + compileSource;

        // invoke off to the ReplayController to replace the log's shader
        // with our edited one
        ctx->Replay().AsyncInvoke([ctx, entryFunc, compileSource, shaderType, id, shaderDetails,
                                   viewer](IReplayController *r) {
          rdctype::str errs;

          uint flags = shaderDetails->DebugInfo.compileFlags;

          ResourceId from = id;
          ResourceId to;

          std::tie(to, errs) = r->BuildTargetShader(
              entryFunc.toUtf8().data(), compileSource.toUtf8().data(), flags, shaderType);

          GUIInvoke::call([viewer, errs]() { viewer->ShowErrors(ToQStr(errs)); });
          if(to == ResourceId())
          {
            r->RemoveReplacement(from);
            GUIInvoke::call([ctx]() { ctx->RefreshStatus(); });
          }
          else
          {
            r->ReplaceResource(from, to);
            GUIInvoke::call([ctx]() { ctx->RefreshStatus(); });
          }
        });
      },

      // Close Callback
      [id](ICaptureContext *ctx) {
        // remove the replacement on close (we could make this more sophisticated if there
        // was a place to control replaced resources/shaders).
        ctx->Replay().AsyncInvoke([ctx, id](IReplayController *r) {
          r->RemoveReplacement(id);
          GUIInvoke::call([ctx] { ctx->RefreshStatus(); });
        });
      });

  m_Ctx.AddDockWindow(sv->Widget(), DockReference::AddTo, this);
}

bool PipelineStateViewer::SaveShaderFile(const ShaderReflection *shader)
{
  if(!shader)
    return false;

  QString filter;

  if(m_Ctx.CurPipelineState().IsLogD3D11() || m_Ctx.CurPipelineState().IsLogD3D12())
  {
    filter = tr("DXBC Shader files (*.dxbc)");
  }
  else if(m_Ctx.CurPipelineState().IsLogGL())
  {
    filter = tr("GLSL files (*.glsl)");
  }
  else if(m_Ctx.CurPipelineState().IsLogVK())
  {
    filter = tr("SPIR-V files (*.spv)");
  }

  QString filename = RDDialog::getSaveFileName(this, tr("Save Shader As"), QString(), filter);

  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        f.write((const char *)shader->RawBytes.elems, (qint64)shader->RawBytes.count);
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
