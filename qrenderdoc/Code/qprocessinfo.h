// Copyright (c) 2016, Baldur Karlsson
//
// Licensed under BSD 2-Clause License, see LICENSE file.
//
// Obtained from https://github.com/baldurk/qprocessinfo

#pragma once

#include <QList>

class QProcessInfo;
typedef QList<QProcessInfo> QProcessList;

class QProcessInfo
{
public:
  static QProcessList enumerate();

  uint32_t pid() const;
  void setPid(uint32_t pid);

  const QString &name() const;
  void setName(const QString &name);

  const QString &windowTitle() const;
  void setWindowTitle(const QString &title);

  const QString &commandLine() const;
  void setCommandLine(const QString &cmd);

private:
  uint32_t m_pid;
  QString m_name;
  QString m_title;
  QString m_cmdLine;
};
