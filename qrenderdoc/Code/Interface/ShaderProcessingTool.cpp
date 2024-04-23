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

#include <QApplication>
#include <QFile>
#include <QStandardPaths>
#include "Code/QRDUtils.h"
#include "QRDInterface.h"

static const QString glsl_stage4[arraydim<ShaderStage>()] = {
    lit("vert"), lit("tesc"), lit("tese"), lit("geom"),
    lit("frag"), lit("comp"), lit("task"), lit("mesh"),
};

static const QString full_stage[arraydim<ShaderStage>()] = {
    lit("vertex"), lit("hull"),    lit("domain"),        lit("geometry"),
    lit("pixel"),  lit("compute"), lit("amplification"), lit("mesh"),
};

static const QString hlsl_stage2[arraydim<ShaderStage>()] = {
    lit("vs"), lit("hs"), lit("ds"), lit("gs"), lit("ps"), lit("cs"), lit("as"), lit("ms"),
};

static QString tmpPath(const QString &filename)
{
  return QDir(QDir::tempPath()).absoluteFilePath(filename);
}

QString vulkanVerForSpirVer(QString spirvVer)
{
  if(spirvVer == lit("spirv1.0") || spirvVer == lit("spirv1.1") || spirvVer == lit("spirv1.2"))
    return lit("vulkan1.0");
  if(spirvVer == lit("spirv1.3"))
    return lit("vulkan1.1");
  if(spirvVer == lit("spirv1.4"))
    return lit("vulkan1.1spirv1.4");
  if(spirvVer == lit("spirv1.5"))
    return lit("vulkan1.2");
  if(spirvVer == lit("spirv1.6"))
    return lit("vulkan1.3");
  else
    return lit("vulkan1.3");
}

static ShaderToolOutput RunTool(const ShaderProcessingTool &tool, QWidget *window,
                                QString input_file, QString output_file, QStringList &argList)
{
  bool writesToFile = true;
  bool readStdin = false;

  int idx = argList.indexOf(lit("{stdin}"));
  if(idx >= 0)
  {
    argList.removeAt(idx);
    readStdin = true;
  }

  if(output_file.isEmpty())
  {
    output_file = tmpPath(lit("shader_output"));
    writesToFile = false;
  }

  // ensure we don't have any leftover output files.
  QFile::remove(output_file);

  QString stdout_file = QDir(QDir::tempPath()).absoluteFilePath(lit("shader_stdout"));

  ShaderToolOutput ret;

  if(tool.executable.isEmpty())
  {
    ret.log = QApplication::translate("ShaderProcessingTool",
                                      "ERROR: No Executable specified in tool '%1'")
                  .arg(tool.name);

    return ret;
  }

  QString path = tool.executable;

  if(!QDir::isAbsolutePath(path))
  {
    path = QStandardPaths::findExecutable(path);

    if(path.isEmpty())
    {
      ret.log = QApplication::translate("ShaderProcessingTool",
                                        "ERROR: Couldn't find executable '%1' in path")
                    .arg(tool.executable);

      return ret;
    }
  }

  QByteArray stdout_data;
  QProcess process;

  QThread *mainThread = QThread::currentThread();

  LambdaThread *thread = new LambdaThread([&]() {
    if(readStdin)
      process.setStandardInputFile(input_file);

    if(!writesToFile)
      process.setStandardOutputFile(output_file);
    else
      process.setStandardOutputFile(stdout_file);

    // for now merge stdout/stderr together. Maybe we should separate these and somehow annotate
    // them? Merging is difficult without messing up order, and some tools output non-errors to
    // stderr
    process.setStandardErrorFile(stdout_file);

    process.start(tool.executable, argList);
    process.waitForFinished();

    {
      QFile outputHandle(output_file);
      if(outputHandle.open(QFile::ReadOnly))
      {
        ret.result = outputHandle.readAll();
        outputHandle.close();
      }
    }

    {
      QFile stdoutHandle(stdout_file);
      if(stdoutHandle.open(QFile::ReadOnly))
      {
        stdout_data = stdoutHandle.readAll();
        stdoutHandle.close();
      }
    }

    // The input files typically aren't large and we don't generate unique names so they won't be
    // overwritten.
    // Leaving them alone means the user can try to recreate the tool invocation themselves.
    // QFile::remove(input_file);
    QFile::remove(output_file);
    QFile::remove(stdout_file);

    process.moveToThread(mainThread);
  });

  thread->setName(lit("ShaderProcessingTool %1").arg(tool.name));
  thread->moveObjectToThread(&process);

  thread->start();

  ShowProgressDialog(
      window, QApplication::translate("ShaderProcessingTool", "Please wait - running external tool"),
      [thread]() { return !thread->isRunning(); });

  thread->deleteLater();

  QString processStatus;

  QProcess::ProcessError error = process.error();

  if(process.exitStatus() == QProcess::CrashExit)
    error = QProcess::Crashed;

  switch(error)
  {
    case QProcess::FailedToStart:
    {
      if(QDir::isAbsolutePath(tool.executable))
      {
        if(!QFile::exists(tool.executable))
        {
          processStatus =
              QApplication::translate("ShaderProcessingTool",
                                      "Process couldn't be started, \"%1\" does not exist.")
                  .arg(tool.executable);
        }
        else
        {
          processStatus = QApplication::translate(
                              "ShaderProcessingTool",
                              "Process couldn't be started, is \"%1\" a working executable?")
                              .arg(tool.executable);
        }
      }
      else
      {
        processStatus =
            QApplication::translate(
                "ShaderProcessingTool",
                "Process couldn't be started, \"%1\" was located as \"%2\" but didn't start.")
                .arg(tool.executable)
                .arg(path);
      }
      break;
    }
    case QProcess::Crashed:
    {
      processStatus =
          QApplication::translate("ShaderProcessingTool", "Process crashed with code %1.")
              .arg(process.exitCode());
      break;
    }
    case QProcess::ReadError:
    case QProcess::WriteError:
    {
      processStatus =
          QApplication::translate("ShaderProcessingTool", "Process failed during I/O with code %1.")
              .arg(process.exitCode());
      break;
    }
    case QProcess::Timedout:        // shouldn't happen, we don't use a timeout
    case QProcess::UnknownError:    // return value if nothing went wrong
    {
      processStatus =
          QApplication::translate("ShaderProcessingTool", "Process exited with code %1.")
              .arg(process.exitCode());
    }
  }

  ret.log = QApplication::translate("ShaderProcessingTool",
                                    "Running \"%1\" %2\n"
                                    "%3\n"
                                    "%4\n"
                                    "Output file is %5 bytes")
                .arg(path)
                .arg(argList.join(QLatin1Char(' ')))
                .arg(QString::fromUtf8(stdout_data))
                .arg(processStatus)
                .arg(ret.result.count());

  return ret;
}

ShaderToolOutput ShaderProcessingTool::DisassembleShader(QWidget *window,
                                                         const ShaderReflection *shaderDetails,
                                                         rdcstr arguments) const
{
  QStringList argList = ParseArgsList(arguments.isEmpty() ? DefaultArguments() : arguments);
  // always append IO arguments for known tools, so we read/write to our own files and override any
  // dangling output specified file in the embedded command line
  argList.append(ParseArgsList(IOArguments()));

  QString input_file, output_file;

  input_file = tmpPath(lit("shader_input"));

  QString spirvVer = lit("spirv1.0");
  for(const ShaderCompileFlag &flag : shaderDetails->debugInfo.compileFlags.flags)
    if(flag.name == "@spirver")
      spirvVer = flag.value;

  // replace arguments after expansion to avoid problems with quoting paths etc
  for(QString &arg : argList)
  {
    if(arg == lit("{input_file}"))
      arg = input_file;
    if(arg == lit("{output_file}"))
      arg = output_file = tmpPath(lit("shader_output"));
    if(arg == lit("{entry_point}"))
    {
      arg = shaderDetails->debugInfo.entrySourceName;
      if(arg.isEmpty())
        arg = lit("main");
    }

    // substring replacements to enable e.g. {hlsl_stage2}_6_0 and ={vulkan_ver}
    arg.replace(lit("{glsl_stage4}"), glsl_stage4[int(shaderDetails->stage)]);
    arg.replace(lit("{hlsl_stage2}"), hlsl_stage2[int(shaderDetails->stage)]);
    arg.replace(lit("{full_stage}"), full_stage[int(shaderDetails->stage)]);
    arg.replace(lit("{spirv_ver}"), spirvVer);
    arg.replace(lit("{vulkan_ver}"), vulkanVerForSpirVer(spirvVer));
  }

  QFile binHandle(input_file);
  if(binHandle.open(QFile::WriteOnly | QIODevice::Truncate))
  {
    binHandle.write(
        QByteArray((const char *)shaderDetails->rawBytes.data(), shaderDetails->rawBytes.count()));
    binHandle.close();
  }
  else
  {
    ShaderToolOutput ret;

    ret.log = QApplication::translate("ShaderProcessingTool",
                                      "ERROR: Couldn't write input to temporary file '%1'")
                  .arg(input_file);

    return ret;
  }

  return RunTool(*this, window, input_file, output_file, argList);
}

ShaderToolOutput ShaderProcessingTool::CompileShader(QWidget *window, rdcstr source,
                                                     rdcstr entryPoint, ShaderStage stage,
                                                     rdcstr spirvVer, rdcstr arguments) const
{
  QStringList argList = ParseArgsList(arguments.isEmpty() ? DefaultArguments() : arguments);
  // always append IO arguments for known tools, so we read/write to our own files and override any
  // dangling output specified file in the embedded command line
  argList.append(ParseArgsList(IOArguments()));

  QString input_file, output_file;

  input_file = tmpPath(lit("shader_input"));

  if(spirvVer.isEmpty())
    spirvVer = "spirv1.0";

  // replace arguments after expansion to avoid problems with quoting paths etc
  for(QString &arg : argList)
  {
    if(arg == lit("{input_file}"))
      arg = input_file;
    if(arg == lit("{output_file}"))
      arg = output_file = tmpPath(lit("shader_output"));
    if(arg == lit("{entry_point}"))
      arg = entryPoint;

    // substring replacements to enable e.g. {hlsl_stage2}_6_0 and ={vulkan_ver}
    arg.replace(lit("{glsl_stage4}"), glsl_stage4[int(stage)]);
    arg.replace(lit("{hlsl_stage2}"), hlsl_stage2[int(stage)]);
    arg.replace(lit("{full_stage}"), full_stage[int(stage)]);
    arg.replace(lit("{spirv_ver}"), spirvVer);
    arg.replace(lit("{vulkan_ver}"), vulkanVerForSpirVer(spirvVer));
  }

  QFile binHandle(input_file);
  if(binHandle.open(QFile::WriteOnly | QIODevice::Truncate))
  {
    binHandle.write(QByteArray((const char *)source.c_str(), source.count()));
    binHandle.close();
  }
  else
  {
    ShaderToolOutput ret;

    ret.log = QApplication::translate("ShaderProcessingTool",
                                      "ERROR: Couldn't write input to temporary file '%1'")
                  .arg(input_file);

    return ret;
  }

  return RunTool(*this, window, input_file, output_file, argList);
}
