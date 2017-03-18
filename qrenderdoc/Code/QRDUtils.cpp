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

#include "QRDUtils.h"
#include <QApplication>
#include <QFileSystemModel>
#include <QGridLayout>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMetaMethod>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QtMath>

QString ToQStr(const ResourceUsage usage, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(usage)
    {
      case eUsage_VertexBuffer: return "Vertex Buffer";
      case eUsage_IndexBuffer: return "Index Buffer";

      case eUsage_VS_Constants: return "VS - Constant Buffer";
      case eUsage_GS_Constants: return "GS - Constant Buffer";
      case eUsage_HS_Constants: return "HS - Constant Buffer";
      case eUsage_DS_Constants: return "DS - Constant Buffer";
      case eUsage_CS_Constants: return "CS - Constant Buffer";
      case eUsage_PS_Constants: return "PS - Constant Buffer";
      case eUsage_All_Constants: return "All - Constant Buffer";

      case eUsage_SO: return "Stream Out";

      case eUsage_VS_Resource: return "VS - Resource";
      case eUsage_GS_Resource: return "GS - Resource";
      case eUsage_HS_Resource: return "HS - Resource";
      case eUsage_DS_Resource: return "DS - Resource";
      case eUsage_CS_Resource: return "CS - Resource";
      case eUsage_PS_Resource: return "PS - Resource";
      case eUsage_All_Resource: return "All - Resource";

      case eUsage_VS_RWResource: return "VS - UAV";
      case eUsage_HS_RWResource: return "HS - UAV";
      case eUsage_DS_RWResource: return "DS - UAV";
      case eUsage_GS_RWResource: return "GS - UAV";
      case eUsage_PS_RWResource: return "PS - UAV";
      case eUsage_CS_RWResource: return "CS - UAV";
      case eUsage_All_RWResource: return "All - UAV";

      case eUsage_InputTarget: return "Colour Input";
      case eUsage_ColourTarget: return "Rendertarget";
      case eUsage_DepthStencilTarget: return "Depthstencil";

      case eUsage_Indirect: return "Indirect argument";

      case eUsage_Clear: return "Clear";

      case eUsage_GenMips: return "Generate Mips";
      case eUsage_Resolve: return "Resolve";
      case eUsage_ResolveSrc: return "Resolve - Source";
      case eUsage_ResolveDst: return "Resolve - Dest";
      case eUsage_Copy: return "Copy";
      case eUsage_CopySrc: return "Copy - Source";
      case eUsage_CopyDst: return "Copy - Dest";

      case eUsage_Barrier: return "Barrier";
      default: break;
    }
  }
  else if(apitype == eGraphicsAPI_OpenGL || apitype == eGraphicsAPI_Vulkan)
  {
    const bool vk = (apitype == eGraphicsAPI_Vulkan);

    switch(usage)
    {
      case eUsage_VertexBuffer: return "Vertex Buffer";
      case eUsage_IndexBuffer: return "Index Buffer";

      case eUsage_VS_Constants: return "VS - Uniform Buffer";
      case eUsage_GS_Constants: return "GS - Uniform Buffer";
      case eUsage_HS_Constants: return "HS - Uniform Buffer";
      case eUsage_DS_Constants: return "DS - Uniform Buffer";
      case eUsage_CS_Constants: return "CS - Uniform Buffer";
      case eUsage_PS_Constants: return "PS - Uniform Buffer";
      case eUsage_All_Constants: return "All - Uniform Buffer";

      case eUsage_SO: return "Transform Feedback";

      case eUsage_VS_Resource: return "VS - Texture";
      case eUsage_GS_Resource: return "GS - Texture";
      case eUsage_HS_Resource: return "HS - Texture";
      case eUsage_DS_Resource: return "DS - Texture";
      case eUsage_CS_Resource: return "CS - Texture";
      case eUsage_PS_Resource: return "PS - Texture";
      case eUsage_All_Resource: return "All - Texture";

      case eUsage_VS_RWResource: return "VS - Image/SSBO";
      case eUsage_HS_RWResource: return "HS - Image/SSBO";
      case eUsage_DS_RWResource: return "DS - Image/SSBO";
      case eUsage_GS_RWResource: return "GS - Image/SSBO";
      case eUsage_PS_RWResource: return "PS - Image/SSBO";
      case eUsage_CS_RWResource: return "CS - Image/SSBO";
      case eUsage_All_RWResource: return "All - Image/SSBO";

      case eUsage_InputTarget: return "FBO Input";
      case eUsage_ColourTarget: return "FBO Colour";
      case eUsage_DepthStencilTarget: return "FBO Depthstencil";

      case eUsage_Indirect: return "Indirect argument";

      case eUsage_Clear: return "Clear";

      case eUsage_GenMips: return "Generate Mips";
      case eUsage_Resolve: return vk ? "Resolve" : "Framebuffer blit";
      case eUsage_ResolveSrc: return vk ? "Resolve - Source" : "Framebuffer blit - Source";
      case eUsage_ResolveDst: return vk ? "Resolve - Dest" : "Framebuffer blit - Dest";
      case eUsage_Copy: return "Copy";
      case eUsage_CopySrc: return "Copy - Source";
      case eUsage_CopyDst: return "Copy - Dest";

      case eUsage_Barrier: return "Barrier";
      default: break;
    }
  }

  return "Unknown";
}

QString ToQStr(const ShaderStageType stage, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(stage)
    {
      case eShaderStage_Vertex: return "Vertex";
      case eShaderStage_Hull: return "Hull";
      case eShaderStage_Domain: return "Domain";
      case eShaderStage_Geometry: return "Geometry";
      case eShaderStage_Pixel: return "Pixel";
      case eShaderStage_Compute: return "Compute";
      default: break;
    }
  }
  else if(apitype == eGraphicsAPI_OpenGL || apitype == eGraphicsAPI_Vulkan)
  {
    switch(stage)
    {
      case eShaderStage_Vertex: return "Vertex";
      case eShaderStage_Tess_Control: return "Tess. Control";
      case eShaderStage_Tess_Eval: return "Tess. Eval";
      case eShaderStage_Geometry: return "Geometry";
      case eShaderStage_Fragment: return "Fragment";
      case eShaderStage_Compute: return "Compute";
      default: break;
    }
  }

  return "Unknown";
}

QString TypeString(const SigParameter &sig)
{
  QString ret = "";

  if(sig.compType == eCompType_Float)
    ret += "float";
  else if(sig.compType == eCompType_UInt || sig.compType == eCompType_UScaled)
    ret += "uint";
  else if(sig.compType == eCompType_SInt || sig.compType == eCompType_SScaled)
    ret += "int";
  else if(sig.compType == eCompType_UNorm)
    ret += "unorm float";
  else if(sig.compType == eCompType_SNorm)
    ret += "snorm float";
  else if(sig.compType == eCompType_Depth)
    ret += "float";

  if(sig.compCount > 1)
    ret += QString::number(sig.compCount);

  return ret;
}

QString D3DSemanticString(const SigParameter &sig)
{
  if(sig.systemValue == eAttr_None)
    return ToQStr(sig.semanticIdxName);

  QString ret = ToQStr(sig.systemValue);

  // need to include the index if it's a system value semantic that's numbered
  if(sig.systemValue == eAttr_ColourOutput || sig.systemValue == eAttr_CullDistance ||
     sig.systemValue == eAttr_ClipDistance)
    ret += QString::number(sig.semanticIndex);

  return ret;
}

QString GetComponentString(byte mask)
{
  QString ret = "";

  if((mask & 0x1) > 0)
    ret += "R";
  if((mask & 0x2) > 0)
    ret += "G";
  if((mask & 0x4) > 0)
    ret += "B";
  if((mask & 0x8) > 0)
    ret += "A";

  return ret;
}

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  // marker that this data is valid
  data[magicIdentifier] = magicVersion;

  QJsonDocument doc = QJsonDocument::fromVariant(data);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert data to JSON document";
    return false;
  }

  QByteArray jsontext = doc.toJson(QJsonDocument::Indented);

  qint64 ret = f.write(jsontext);

  if(ret != jsontext.size())
  {
    qCritical() << "Failed to write JSON data: " << ret << " " << f.errorString();
    return false;
  }

  return true;
}

bool LoadFromJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  QByteArray json = f.readAll();

  if(json.isEmpty())
  {
    qCritical() << "Read invalid empty JSON data from file " << f.errorString();
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(json);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert file to JSON document";
    return false;
  }

  data = doc.toVariant().toMap();

  if(data.isEmpty() || !data.contains(magicIdentifier))
  {
    qCritical() << "Converted config data is invalid or unrecognised";
    return false;
  }

  if(data[magicIdentifier].toUInt() != magicVersion)
  {
    qCritical() << "Converted config data is not the right version";
    return false;
  }

  return true;
}

int GUIInvoke::methodIndex = -1;

void GUIInvoke::init()
{
  GUIInvoke *invoke = new GUIInvoke();
  methodIndex = invoke->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("doInvoke()"));
}

void GUIInvoke::call(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  invoke->metaObject()->method(methodIndex).invoke(invoke, Qt::QueuedConnection);
}

void GUIInvoke::blockcall(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  invoke->metaObject()->method(methodIndex).invoke(invoke, Qt::BlockingQueuedConnection);
}

const QMessageBox::StandardButtons RDDialog::YesNoCancel =
    QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

void RDDialog::show(QMenu *menu, QPoint pos)
{
  menu->setWindowModality(Qt::ApplicationModal);
  menu->popup(pos);
  QEventLoop loop;
  while(menu->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }
}

int RDDialog::show(QDialog *dialog)
{
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->show();
  QEventLoop loop;
  while(dialog->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }

  return dialog->result();
}

QMessageBox::StandardButton RDDialog::messageBox(QMessageBox::Icon icon, QWidget *parent,
                                                 const QString &title, const QString &text,
                                                 QMessageBox::StandardButtons buttons,
                                                 QMessageBox::StandardButton defaultButton)
{
  QMessageBox::StandardButton ret = defaultButton;

  // if we're already on the right thread, this boils down to a function call
  GUIInvoke::blockcall([&]() {
    QMessageBox mb(icon, title, text, buttons, parent);
    mb.setDefaultButton(defaultButton);
    show(&mb);
    ret = mb.standardButton(mb.clickedButton());
  });
  return ret;
}

QString RDDialog::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir,
                                       QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, QString());
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::DirectoryOnly);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setFileMode(QFileDialog::ExistingFile);
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getExecutableFileName(QWidget *parent, const QString &caption, const QString &dir,
                                        QFileDialog::Options options)
{
  QString filter;

#if defined(Q_OS_WIN32)
  // can't filter by executable bit on windows, but we have extensions
  filter = "Executables (*.exe);;All Files (*.*)";
#endif

  QFileDialog fd(parent, caption, dir, filter);
  fd.setOptions(options);
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::ExistingFile);
  {
    QFileFilterModel *fileProxy = new QFileFilterModel(parent);
    fileProxy->setRequirePermissions(QDir::Executable);
    fd.setProxyModel(fileProxy);
  }
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setAcceptMode(QFileDialog::AcceptSave);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

bool QFileFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
  QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);

  QFileSystemModel *fs = qobject_cast<QFileSystemModel *>(sourceModel());

  if(!fs)
  {
    qCritical() << "Expected a QFileSystemModel as the source model!";
    return true;
  }

  if(fs->isDir(idx))
    return true;

  QFile::Permissions permissions =
      (QFile::Permissions)sourceModel()->data(idx, QFileSystemModel::FilePermissions).toInt();

  if((m_requireMask & QDir::Readable) && !(permissions & QFile::ReadUser))
    return false;
  if((m_requireMask & QDir::Writable) && !(permissions & QFile::WriteUser))
    return false;
  if((m_requireMask & QDir::Executable) && !(permissions & QFile::ExeUser))
    return false;

  if((m_excludeMask & QDir::Readable) && (permissions & QFile::ReadUser))
    return false;
  if((m_excludeMask & QDir::Writable) && (permissions & QFile::WriteUser))
    return false;
  if((m_excludeMask & QDir::Executable) && (permissions & QFile::ExeUser))
    return false;

  return true;
}

void addGridLines(QGridLayout *grid)
{
  for(int y = 0; y < grid->rowCount(); y++)
  {
    for(int x = 0; x < grid->columnCount(); x++)
    {
      QLayoutItem *item = grid->itemAtPosition(y, x);

      if(item == NULL)
        continue;

      QWidget *w = item->widget();

      if(w == NULL)
        continue;

      QString style = "border: solid black; border-bottom-width: 1px; border-right-width: 1px;";

      if(x == 0)
        style += "border-left-width: 1px;";
      if(y == 0)
        style += "border-top-width: 1px;";

      w->setStyleSheet(style);
    }
  }
}

QTreeWidgetItem *makeTreeNode(const std::initializer_list<QVariant> &values)
{
  QTreeWidgetItem *ret = new QTreeWidgetItem();

  int i = 0;
  for(const QVariant &v : values)
    ret->setData(i++, Qt::DisplayRole, v);

  return ret;
}

QTreeWidgetItem *makeTreeNode(const QVariantList &values)
{
  QTreeWidgetItem *ret = new QTreeWidgetItem();

  int i = 0;
  for(const QVariant &v : values)
    ret->setData(i++, Qt::DisplayRole, v);

  return ret;
}

void deleteChildren(QTreeWidgetItem *item)
{
  while(item->childCount() > 0)
  {
    QTreeWidgetItem *child = item->takeChild(0);
    deleteChildren(child);
    delete child;
  }
}

int Formatter::m_minFigures = 2, Formatter::m_maxFigures = 5, Formatter::m_expNegCutoff = 5,
    Formatter::m_expPosCutoff = 7;
double Formatter::m_expNegValue = 0.00001;       // 10^(-5)
double Formatter::m_expPosValue = 10000000.0;    // 10^7

void Formatter::setParams(int minFigures, int maxFigures, int expNegCutoff, int expPosCutoff)
{
  m_minFigures = qMax(0, minFigures);
  m_maxFigures = qMax(2, maxFigures);
  m_expNegCutoff = qMax(0, expNegCutoff);
  m_expPosCutoff = qMax(0, expPosCutoff);

  m_expNegValue = qPow(10.0, -m_expNegCutoff);
  m_expPosValue = qPow(10.0, m_expPosCutoff);
}

QString Formatter::Format(double f, bool)
{
  if(f != 0.0 && (qAbs(f) < m_expNegValue || qAbs(f) > m_expPosValue))
    return QString("%1").arg(f, -m_minFigures, 'E', m_maxFigures);

  QString ret = QString("%1").arg(f, 0, 'f', m_maxFigures);

  // trim excess trailing 0s
  int decimal = ret.lastIndexOf(QChar('.'));
  if(decimal > 0)
  {
    decimal += m_minFigures;

    const int len = ret.count();

    int remove = 0;
    while(len - remove - 1 > decimal && ret.at(len - remove - 1) == QChar('0'))
      remove++;

    if(remove > 0)
      ret.chop(remove);
  }

  return ret;
}

class RDProgressDialog : public QProgressDialog
{
public:
  RDProgressDialog(const QString &labelText, QWidget *parent)
      // we add 1 so that the progress value never hits maximum until we are actually finished
      : QProgressDialog(labelText, QString(), 0, maxProgress + 1, parent),
        m_Label(this)
  {
    setWindowTitle(tr("Please Wait"));
    setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog | Qt::WindowTitleHint);
    setWindowIcon(QIcon());
    setMinimumSize(QSize(250, 0));
    setMaximumSize(QSize(250, 10000));
    setCancelButton(NULL);
    setMinimumDuration(0);
    setWindowModality(Qt::ApplicationModal);
    setValue(0);

    m_Label.setText(labelText);
    m_Label.setAlignment(Qt::AlignCenter);
    m_Label.setWordWrap(true);

    setLabel(&m_Label);
  }

  void setPercentage(float percent) { setValue(int(maxProgress * percent)); }
  void setInfinite(bool infinite)
  {
    if(infinite)
    {
      setMinimum(0);
      setMaximum(0);
      setValue(0);
    }
    else
    {
      setMinimum(0);
      setMaximum(maxProgress + 1);
      setValue(0);
    }
  }

  void closeAndReset()
  {
    setValue(maxProgress);
    hide();
    reset();
  }

protected:
  void keyPressEvent(QKeyEvent *e) override
  {
    if(e->key() == Qt::Key_Escape)
      return;

    QProgressDialog::keyPressEvent(e);
  }

  QLabel m_Label;

  static const int maxProgress = 1000;
};

#if defined(Q_OS_WIN32)
#include <windows.h>

#include <shellapi.h>
#endif

bool RunProcessAsAdmin(const QString &fullExecutablePath, const QStringList &params,
                       std::function<void()> finishedCallback)
{
#if defined(Q_OS_WIN32)

  std::wstring wideExe = fullExecutablePath.toStdWString();
  std::wstring wideParams = params.join(QChar(' ')).toStdWString();

  SHELLEXECUTEINFOW info = {};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"runas";
  info.lpFile = wideExe.c_str();
  info.lpParameters = wideParams.c_str();
  info.nShow = SW_SHOWNORMAL;

  ShellExecuteExW(&info);

  if((uintptr_t)info.hInstApp > 32 && info.hProcess != NULL)
  {
    if(finishedCallback)
    {
      HANDLE h = info.hProcess;

      // do the wait on another thread
      LambdaThread *thread = new LambdaThread([h, finishedCallback]() {
        WaitForSingleObject(h, 30000);
        CloseHandle(h);
        GUIInvoke::call(finishedCallback);
      });
      thread->selfDelete(true);
      thread->start();
    }
    else
    {
      CloseHandle(info.hProcess);
    }

    return true;
  }

  return false;

#else
  // try to find a way to run the application elevated.
  const QString graphicalSudo[] = {
      "pkexec", "kdesudo", "gksudo", "beesu",
  };

  // if none of the graphical options, then look for sudo and either
  const QString termEmulator[] = {
      "x-terminal-emulator", "gnome-terminal", "knosole", "xterm",
  };

  for(const QString &sudo : graphicalSudo)
  {
    QString inPath = QStandardPaths::findExecutable(sudo);

    // can't find in path
    if(inPath.isEmpty())
      continue;

    QProcess *process = new QProcess;

    QStringList sudoParams;
    sudoParams << fullExecutablePath;
    for(const QString &p : params)
      sudoParams << p;

    qInfo() << "Running" << sudo << "with params" << sudoParams;

    // run with sudo
    process->start(sudo, sudoParams);

    // when the process exits, call the callback and delete
    QObject::connect(process, OverloadedSlot<int>::of(&QProcess::finished),
                     [process, finishedCallback](int exitCode) {
                       process->deleteLater();
                       GUIInvoke::call(finishedCallback);
                     });

    return true;
  }

  QString sudo = QStandardPaths::findExecutable("sudo");

  if(sudo.isEmpty())
  {
    qCritical() << "Couldn't find graphical or terminal sudo program!\n"
                << "Please run " << fullExecutablePath << "with args" << params << "manually.";
    return false;
  }

  for(const QString &term : termEmulator)
  {
    QString inPath = QStandardPaths::findExecutable(term);

    // can't find in path
    if(inPath.isEmpty())
      continue;

    QProcess *process = new QProcess;

    // run terminal sudo with emulator
    QStringList termParams;
    termParams << "-e"
               << QString("bash -c 'sudo %1 %2'").arg(fullExecutablePath).arg(params.join(QChar(' ')));

    process->start(term, termParams);

    // when the process exits, call the callback and delete
    QObject::connect(process, OverloadedSlot<int>::of(&QProcess::finished),
                     [process, finishedCallback](int exitCode) {
                       process->deleteLater();
                       GUIInvoke::call(finishedCallback);
                     });

    return true;
  }

  qCritical() << "Couldn't find graphical or terminal emulator to launch sudo.\n"
              << "Please run " << fullExecutablePath << "with args" << params << "manually.";

  return false;
#endif
}

QStringList ParseArgsList(const QString &args)
{
  QStringList ret;

  if(args.isEmpty())
    return ret;

// on windows just use the function provided by the system
#if defined(Q_OS_WIN32)
  std::wstring wargs = args.toStdWString();

  int argc = 0;
  wchar_t **argv = CommandLineToArgvW(wargs.c_str(), &argc);

  for(int i = 0; i < argc; i++)
    ret << QString::fromWCharArray(argv[i]);

  LocalFree(argv);
#else
  std::string argString = args.toStdString();

  // perform some kind of sane parsing
  bool dquot = false, squot = false;    // are we inside ''s or ""s

  // current character
  char *c = &argString[0];

  // current argument we're building
  std::string a;

  while(*c)
  {
    if(!dquot && !squot && (*c == ' ' || *c == '\t'))
    {
      if(!a.empty())
        ret << QString::fromStdString(a);

      a = "";
    }
    else if(!dquot && *c == '"')
    {
      dquot = true;
    }
    else if(!squot && *c == '\'')
    {
      squot = true;
    }
    else if(dquot && *c == '"')
    {
      dquot = false;
    }
    else if(squot && *c == '\'')
    {
      squot = false;
    }
    else if(squot)
    {
      // single quotes don't escape, just copy literally until we leave single quote mode
      a.push_back(*c);
    }
    else if(dquot)
    {
      // handle escaping
      if(*c == '\\')
      {
        c++;
        if(*c)
        {
          a.push_back(*c);
        }
        else
        {
          qCritical() << "Malformed args list:" << args;
          return ret;
        }
      }
      else
      {
        a.push_back(*c);
      }
    }
    else
    {
      a.push_back(*c);
    }

    c++;
  }

  // if we were building an argument when we hit the end of the string
  if(!a.empty())
    ret << QString::fromStdString(a);
#endif

  return ret;
}

void ShowProgressDialog(QWidget *window, const QString &labelText, ProgressFinishedMethod finished,
                        ProgressUpdateMethod update)
{
  RDProgressDialog dialog(labelText, window);

  // if we don't have an update function, set the progress display to be 'infinite spinner'
  dialog.setInfinite(!update);

  QSemaphore tickerSemaphore(1);

  // start a lambda thread to tick our functions and close the progress dialog when we're done.
  LambdaThread progressTickerThread([finished, update, &dialog, &tickerSemaphore]() {
    while(tickerSemaphore.available())
    {
      QThread::msleep(30);

      if(update)
        GUIInvoke::call([update, &dialog]() { dialog.setPercentage(update()); });

      GUIInvoke::call([finished, &tickerSemaphore]() {
        if(finished())
          tickerSemaphore.tryAcquire();
      });
    }

    GUIInvoke::call([&dialog]() { dialog.closeAndReset(); });
  });
  progressTickerThread.start();

  // show the dialog
  RDDialog::show(&dialog);

  // signal the thread to exit if somehow we got here without it finishing, then wait for it thread
  // to clean itself up
  tickerSemaphore.tryAcquire();
  progressTickerThread.wait();
}

QString GetSystemUsername()
{
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

  QString username = env.value("USER");
  if(username == QString())
    username = env.value("USERNAME");
  if(username == QString())
    username = "Unknown_User";

  return username;
}
