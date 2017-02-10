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
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Windows/MainWindow.h"
#include "Windows/ShaderViewer.h"
#include "D3D11PipelineStateViewer.h"
#include "D3D12PipelineStateViewer.h"
#include "GLPipelineStateViewer.h"
#include "VulkanPipelineStateViewer.h"
#include "ui_PipelineStateViewer.h"

PipelineStateViewer::PipelineStateViewer(CaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;

  m_Ctx.AddLogViewer(this);

  setToD3D11();
}

PipelineStateViewer::~PipelineStateViewer()
{
  reset();

  m_Ctx.windowClosed(this);
  m_Ctx.RemoveLogViewer(this);

  delete ui;
}

void PipelineStateViewer::OnLogfileLoaded()
{
  if(m_Ctx.APIProps().pipelineType == eGraphicsAPI_D3D11)
    setToD3D11();
  else if(m_Ctx.APIProps().pipelineType == eGraphicsAPI_D3D12)
    setToD3D12();
  else if(m_Ctx.APIProps().pipelineType == eGraphicsAPI_OpenGL)
    setToGL();
  else if(m_Ctx.APIProps().pipelineType == eGraphicsAPI_Vulkan)
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
  if(m_Ctx.CurPipelineState.DefaultType != m_Ctx.APIProps().pipelineType)
    OnLogfileLoaded();

  if(m_Current)
    m_Current->OnEventChanged(eventID);
}

QVariant PipelineStateViewer::persistData()
{
  QVariantMap state;

  if(m_Current == m_D3D11)
    state["type"] = "D3D11";
  else if(m_Current == m_D3D12)
    state["type"] = "D3D12";
  else if(m_Current == m_GL)
    state["type"] = "GL";
  else if(m_Current == m_Vulkan)
    state["type"] = "Vulkan";
  else
    state["type"] = "";

  return state;
}

void PipelineStateViewer::setPersistData(const QVariant &persistData)
{
  QString str = persistData.toMap()["type"].toString();

  if(str == "D3D11")
    setToD3D11();
  else if(str == "D3D12")
    setToD3D12();
  else if(str == "GL")
    setToGL();
  else if(str == "Vulkan")
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
  m_Ctx.CurPipelineState.DefaultType = eGraphicsAPI_D3D11;
}

void PipelineStateViewer::setToD3D12()
{
  if(m_D3D12)
    return;

  reset();

  m_D3D12 = new D3D12PipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_D3D12);
  m_Current = m_D3D12;
  m_Ctx.CurPipelineState.DefaultType = eGraphicsAPI_D3D12;
}

void PipelineStateViewer::setToGL()
{
  if(m_GL)
    return;

  reset();

  m_GL = new GLPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_GL);
  m_Current = m_GL;
  m_Ctx.CurPipelineState.DefaultType = eGraphicsAPI_OpenGL;
}

void PipelineStateViewer::setToVulkan()
{
  if(m_Vulkan)
    return;

  reset();

  m_Vulkan = new VulkanPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_Vulkan);
  m_Current = m_Vulkan;
  m_Ctx.CurPipelineState.DefaultType = eGraphicsAPI_Vulkan;
}

bool PipelineStateViewer::PrepareShaderEditing(const ShaderReflection *shaderDetails,
                                               QString &entryFunc, QStringMap &files,
                                               QString &mainfile)
{
  if(!shaderDetails->DebugInfo.entryFunc.empty() && !shaderDetails->DebugInfo.files.empty())
  {
    entryFunc = ToQStr(shaderDetails->DebugInfo.entryFunc);

    QStringList uniqueFiles;

    for(auto &s : shaderDetails->DebugInfo.files)
    {
      QString filename = ToQStr(s.first);
      if(uniqueFiles.contains(filename.toLower()))
      {
        qWarning() << "Duplicate full filename" << ToQStr(s.first);
        continue;
      }
      uniqueFiles.push_back(filename.toLower());

      files[filename] = ToQStr(s.second);
    }

    int entryFile = shaderDetails->DebugInfo.entryFile;
    if(entryFile < 0 || entryFile >= shaderDetails->DebugInfo.files.count)
      entryFile = 0;

    mainfile = ToQStr(shaderDetails->DebugInfo.files[entryFile].first);

    return true;
  }

  return false;
}

void PipelineStateViewer::EditShader(ShaderStageType shaderType, ResourceId id,
                                     const ShaderReflection *shaderDetails, const QString &entryFunc,
                                     const QStringMap &files, const QString &mainfile)
{
  ShaderViewer *sv = ShaderViewer::editShader(
      m_Ctx, false, entryFunc, files,
      // save callback
      [entryFunc, mainfile, shaderType, id, shaderDetails](
          CaptureContext *ctx, ShaderViewer *viewer, const QStringMap &updatedfiles) {
        QString compileSource = updatedfiles[mainfile];

        // try and match up #includes against the files that we have. This isn't always
        // possible as fxc only seems to include the source for files if something in
        // that file was included in the compiled output. So you might end up with
        // dangling #includes - we just have to ignore them
        int offs = compileSource.indexOf("#include");

        while(offs >= 0)
        {
          // search back to ensure this is a valid #include (ie. not in a comment).
          // Must only see whitespace before, then a newline.
          int ws = qMax(0, offs - 1);
          while(ws >= 0 && (compileSource[ws] == ' ' || compileSource[ws] == '\t'))
            ws--;

          // not valid? jump to next.
          if(ws > 0 && compileSource[ws] != '\n')
          {
            offs = compileSource.indexOf("#include", offs + 1);
            continue;
          }

          int start = ws + 1;

          bool tail = true;

          int lineEnd = compileSource.indexOf("\n", start + 1);
          if(lineEnd == -1)
          {
            lineEnd = compileSource.length();
            tail = false;
          }

          ws = offs + sizeof("#include") - 1;
          while(compileSource[ws] == ' ' || compileSource[ws] == '\t')
            ws++;

          QString line = compileSource.mid(offs, lineEnd - offs + 1);

          if(compileSource[ws] != '<' && compileSource[ws] != '"')
          {
            viewer->showErrors("Invalid #include directive found:\r\n" + line);
            return;
          }

          // find matching char, either <> or "";
          int end = compileSource.indexOf(compileSource[ws] == '"' ? '"' : '>', ws + 1);

          if(end == -1)
          {
            viewer->showErrors("Invalid #include directive found:\r\n" + line);
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

            if(fileText == "")
              fileText = "// Can't find file " + fname + "\n";
          }

          compileSource = compileSource.left(offs) + "\n\n" + fileText + "\n\n" +
                          (tail ? compileSource.mid(lineEnd + 1) : "");

          // need to start searching from the beginning - wasteful but allows nested includes to
          // work
          offs = compileSource.indexOf("#include");
        }

        if(updatedfiles.contains("@cmdline"))
          compileSource = updatedfiles["@cmdline"] + "\n\n" + compileSource;

        // invoke off to the ReplayRenderer to replace the log's shader
        // with our edited one
        ctx->Renderer().AsyncInvoke([ctx, entryFunc, compileSource, shaderType, id, shaderDetails,
                                     viewer](IReplayRenderer *r) {
          rdctype::str errs;

          uint flags = shaderDetails->DebugInfo.compileFlags;

          ResourceId from = id;
          ResourceId to = r->BuildTargetShader(
              entryFunc.toUtf8().data(), compileSource.toUtf8().data(), flags, shaderType, &errs);

          GUIInvoke::call([viewer, errs]() { viewer->showErrors(ToQStr(errs)); });
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
      [id](CaptureContext *ctx) {
        // remove the replacement on close (we could make this more sophisticated if there
        // was a place to control replaced resources/shaders).
        ctx->Renderer().AsyncInvoke([ctx, id](IReplayRenderer *r) {
          r->RemoveReplacement(id);
          GUIInvoke::call([ctx] { ctx->RefreshStatus(); });
        });
      },
      m_Ctx.mainWindow());

  m_Ctx.setupDockWindow(sv);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
  manager->addToolWindow(sv, ref);
}

bool PipelineStateViewer::SaveShaderFile(const ShaderReflection *shader)
{
  if(!shader)
    return false;

  QString filter;

  if(m_Ctx.CurPipelineState.IsLogD3D11() || m_Ctx.CurPipelineState.IsLogD3D12())
  {
    filter = tr("DXBC Shader files (*.dxbc)");
  }
  else if(m_Ctx.CurPipelineState.IsLogGL())
  {
    filter = tr("GLSL files (*.glsl)");
  }
  else if(m_Ctx.CurPipelineState.IsLogVK())
  {
    filter = tr("SPIR-V files (*.spv)");
  }

  QString filename = RDDialog::getSaveFileName(this, tr("Save Shader As"), QString(), filter);

  if(filename != "")
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