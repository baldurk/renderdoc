#/******************************************************************************
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

rdcstr SPIRVDisassembler::DisassembleShader(QWidget *window, const ShaderReflection *shaderDetails) const
{
  if(executable.isEmpty())
    return "";

  QString spv_bin_file = QDir(QDir::tempPath()).absoluteFilePath(lit("spv_bin.spv"));

  QFile binHandle(spv_bin_file);
  if(binHandle.open(QFile::WriteOnly | QIODevice::Truncate))
  {
    binHandle.write(
        QByteArray((const char *)shaderDetails->rawBytes.data(), shaderDetails->rawBytes.count()));
    binHandle.close();
  }
  else
  {
    RDDialog::critical(
        window, QApplication::translate("SPIRVDisassembler", "Error writing temp file"),
        QApplication::translate("SPIRVDisassembler", "Couldn't write temporary SPIR-V file %1.")
            .arg(spv_bin_file));
    return "";
  }

  if(!QString(args).contains(lit("{spv_bin}")))
  {
    RDDialog::critical(
        window, QApplication::translate("SPIRVDisassembler", "Wrongly configured disassembler"),
        QApplication::translate(
            "SPIRVDisassembler",
            "Please use {spv_bin} in the disassembler arguments to specify the input file."));
    return "";
  }

  QString glsl;

  LambdaThread *thread = new LambdaThread([this, window, &glsl, spv_bin_file]() {
    QString spv_disas_file = QDir(QDir::tempPath()).absoluteFilePath(lit("spv_disas.txt"));

    QString expandedargs = args;

    bool writesToFile = expandedargs.contains(lit("{spv_disas}"));

    expandedargs.replace(lit("{spv_bin}"), spv_bin_file);
    expandedargs.replace(lit("{spv_disas}"), spv_disas_file);

    QStringList argList = ParseArgsList(expandedargs);

    QProcess process;
    process.start(executable, argList);
    process.waitForFinished();

    if(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
      GUIInvoke::call([window]() {
        RDDialog::critical(
            window, QApplication::translate("SPIRVDisassembler", "Error running disassembler"),
            QApplication::translate(
                "SPIRVDisassembler",
                "There was an error invoking the external SPIR-V disassembler."));
      });
    }

    if(writesToFile)
    {
      QFile outputHandle(spv_disas_file);
      if(outputHandle.open(QFile::ReadOnly | QIODevice::Text))
      {
        glsl = QString::fromUtf8(outputHandle.readAll());
        outputHandle.close();
      }
    }
    else
    {
      glsl = QString::fromUtf8(process.readAll());
    }

    QFile::remove(spv_bin_file);
    QFile::remove(spv_disas_file);
  });
  thread->start();

  ShowProgressDialog(window, QApplication::translate("SPIRVDisassembler",
                                                     "Please wait - running external disassembler"),
                     [thread]() { return !thread->isRunning(); });

  thread->deleteLater();

  return glsl;
}
