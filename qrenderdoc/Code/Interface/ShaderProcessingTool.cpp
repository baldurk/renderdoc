/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
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

template <>
std::string DoStringise(const KnownShaderTool &el)
{
  BEGIN_ENUM_STRINGISE(KnownShaderTool);
  {
    STRINGISE_ENUM_CLASS_NAMED(Unknown, "Custom Tool");
    STRINGISE_ENUM_CLASS_NAMED(SPIRV_Cross, "SPIRV-Cross");
    STRINGISE_ENUM_CLASS_NAMED(spirv_dis, "spirv-dis");
    STRINGISE_ENUM_CLASS_NAMED(glslangValidatorGLSL, "glslang (GLSL)");
    STRINGISE_ENUM_CLASS_NAMED(glslangValidatorHLSL, "glslang (HLSL)");
    STRINGISE_ENUM_CLASS_NAMED(spirv_as, "spirv-as");
  }
  END_ENUM_STRINGISE();
}

static QString tmpPath(const QString &filename)
{
  return QDir(QDir::tempPath()).absoluteFilePath(filename);
}

static ShaderToolOutput RunTool(const ShaderProcessingTool &tool, QWidget *window,
                                QString input_file, QString output_file, QStringList &argList)
{
  bool writesToFile = true;

  if(input_file.isEmpty())
    input_file = tmpPath(lit("shader_input"));

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

  LambdaThread *thread = new LambdaThread([&]() {
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
  });
  thread->start();

  ShowProgressDialog(window, QApplication::translate("ShaderProcessingTool",
                                                     "Please wait - running external tool"),
                     [thread]() { return !thread->isRunning(); });

  thread->deleteLater();

  QString processStatus;

  if(process.exitStatus() == QProcess::CrashExit)
  {
    processStatus = QApplication::translate("ShaderProcessingTool", "Process crashed with code %1.")
                        .arg(process.exitCode());
  }
  else
  {
    processStatus = QApplication::translate("ShaderProcessingTool", "Process exited with code %1.")
                        .arg(process.exitCode());
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

  QString input_file, output_file;

  // replace arguments after expansion to avoid problems with quoting paths etc
  for(QString &arg : argList)
  {
    if(arg == lit("{input_file}"))
      arg = input_file = tmpPath(lit("shader_input"));
    if(arg == lit("{output_file}"))
      arg = output_file = tmpPath(lit("shader_output"));
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
                                                     rdcstr arguments) const
{
  QStringList argList = ParseArgsList(arguments.isEmpty() ? DefaultArguments() : arguments);

  QString input_file, output_file;

  const QString glsl_stage4[ENUM_ARRAY_SIZE(ShaderStage)] = {
      lit("vert"), lit("tesc"), lit("tese"), lit("geom"), lit("frag"), lit("comp"),
  };

  // replace arguments after expansion to avoid problems with quoting paths etc
  for(QString &arg : argList)
  {
    if(arg == lit("{input_file}"))
      arg = input_file = tmpPath(lit("shader_input"));
    if(arg == lit("{output_file}"))
      arg = output_file = tmpPath(lit("shader_output"));
    if(arg == lit("{entry_point}"))
      arg = entryPoint;
    if(arg == lit("{glsl_stage4}"))
      arg = glsl_stage4[int(stage)];
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
