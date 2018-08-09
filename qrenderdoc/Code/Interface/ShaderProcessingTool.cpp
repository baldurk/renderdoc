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

rdcstr ShaderProcessingTool::DisassembleShader(QWidget *window, const ShaderReflection *shaderDetails,
                                               rdcstr arguments) const
{
  if(executable.isEmpty())
    return "";

  QString input_file = QDir(QDir::tempPath()).absoluteFilePath(lit("shader_input"));
  QString output_file = QDir(QDir::tempPath()).absoluteFilePath(lit("shader_output"));

  QFile binHandle(input_file);
  if(binHandle.open(QFile::WriteOnly | QIODevice::Truncate))
  {
    binHandle.write(
        QByteArray((const char *)shaderDetails->rawBytes.data(), shaderDetails->rawBytes.count()));
    binHandle.close();
  }
  else
  {
    RDDialog::critical(
        window, QApplication::translate("ShaderProcessingTool", "Error writing temp file"),
        QApplication::translate("ShaderProcessingTool", "Couldn't write temporary file %1.")
            .arg(input_file));
    return "";
  }

  QString programArguments = arguments;

  if(programArguments.isEmpty())
    programArguments = DefaultArguments();

  if(!programArguments.contains(lit("{input_file}")))
  {
    RDDialog::critical(
        window, QApplication::translate("ShaderProcessingTool", "Wrongly configured tool"),
        QApplication::translate(
            "ShaderProcessingTool",
            "Please use {input_file} in the tool arguments to specify the input file."));
    return "";
  }

  QString outputData;

  QString expandedargs = programArguments;

  bool writesToFile = expandedargs.contains(lit("{output_file}"));

  expandedargs.replace(lit("{input_file}"), input_file);
  expandedargs.replace(lit("{output_file}"), output_file);

  QStringList argList = ParseArgsList(expandedargs);

  LambdaThread *thread =
      new LambdaThread([this, window, &outputData, argList, input_file, output_file, writesToFile]() {
        QProcess process;
        process.start(executable, argList);
        process.waitForFinished();

        if(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        {
          if(window)
          {
            GUIInvoke::call(window, [window]() {
              RDDialog::critical(
                  window, QApplication::translate("ShaderProcessingTool", "Error running tool"),
                  QApplication::translate(
                      "ShaderProcessingTool",
                      "There was an error invoking the external shader processing tool."));
            });
          }
        }

        if(writesToFile)
        {
          QFile outputHandle(output_file);
          if(outputHandle.open(QFile::ReadOnly))
          {
            outputData = QString::fromUtf8(outputHandle.readAll());
            outputHandle.close();
          }
        }
        else
        {
          outputData = QString::fromUtf8(process.readAll());
        }

        QFile::remove(input_file);
        QFile::remove(output_file);
      });
  thread->start();

  ShowProgressDialog(window, QApplication::translate("ShaderProcessingTool",
                                                     "Please wait - running external tool"),
                     [thread]() { return !thread->isRunning(); });

  thread->deleteLater();

  return outputData;
}

bytebuf ShaderProcessingTool::CompileShader(QWidget *window, rdcstr source, rdcstr entryPoint,
                                            ShaderStage stage, rdcstr arguments) const
{
  if(executable.isEmpty())
    return bytebuf();

  QString input_file = QDir(QDir::tempPath()).absoluteFilePath(lit("shader_input"));
  QString output_file = QDir(QDir::tempPath()).absoluteFilePath(lit("shader_output"));

  QFile binHandle(input_file);
  if(binHandle.open(QFile::WriteOnly | QIODevice::Truncate))
  {
    binHandle.write(QByteArray((const char *)source.c_str(), source.count()));
    binHandle.close();
  }
  else
  {
    RDDialog::critical(
        window, QApplication::translate("ShaderProcessingTool", "Error writing temp file"),
        QApplication::translate("ShaderProcessingTool", "Couldn't write temporary file %1.")
            .arg(input_file));
    return bytebuf();
  }

  QString programArguments = arguments;

  if(programArguments.isEmpty())
    programArguments = DefaultArguments();

  if(!programArguments.contains(lit("{input_file}")))
  {
    RDDialog::critical(
        window, QApplication::translate("ShaderProcessingTool", "Wrongly configured tool"),
        QApplication::translate(
            "ShaderProcessingTool",
            "Please use {input_file} in the tool arguments to specify the input file."));
    return bytebuf();
  }

  bytebuf outputData;

  QString expandedargs = programArguments;

  bool writesToFile = expandedargs.contains(lit("{output_file}"));

  expandedargs.replace(lit("{input_file}"), input_file);
  expandedargs.replace(lit("{entry_point}"), entryPoint);
  expandedargs.replace(lit("{output_file}"), output_file);

  const QString glsl_stage4[ENUM_ARRAY_SIZE(ShaderStage)] = {
      lit("vert"), lit("tesc"), lit("tese"), lit("geom"), lit("frag"), lit("comp"),
  };

  expandedargs.replace(lit("{glsl_stage4}"), glsl_stage4[int(stage)]);

  QStringList argList = ParseArgsList(expandedargs);

  LambdaThread *thread =
      new LambdaThread([this, window, &outputData, argList, input_file, output_file, writesToFile]() {
        QProcess process;
        process.start(executable, argList);
        process.waitForFinished();

        if(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        {
          if(window)
          {
            GUIInvoke::call(window, [window]() {
              RDDialog::critical(
                  window, QApplication::translate("ShaderProcessingTool", "Error running tool"),
                  QApplication::translate(
                      "ShaderProcessingTool",
                      "There was an error invoking the external shader processing tool."));
            });
          }
        }

        if(writesToFile)
        {
          QFile outputHandle(output_file);
          if(outputHandle.open(QFile::ReadOnly))
          {
            outputData = outputHandle.readAll();
            outputHandle.close();
          }
        }
        else
        {
          outputData = process.readAll();
        }

        QFile::remove(input_file);
        QFile::remove(output_file);
      });
  thread->start();

  ShowProgressDialog(window, QApplication::translate("ShaderProcessingTool",
                                                     "Please wait - running external tool"),
                     [thread]() { return !thread->isRunning(); });

  thread->deleteLater();

  return outputData;
}
