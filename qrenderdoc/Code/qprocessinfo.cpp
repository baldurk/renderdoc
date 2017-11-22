// Copyright (c) 2016, Baldur Karlsson
//
// Licensed under BSD 2-Clause License, see LICENSE file.
//
// Obtained from https://github.com/baldurk/qprocessinfo

#include "qprocessinfo.h"

#if defined(Q_OS_WIN32)

#include <windows.h>

#include <tlhelp32.h>

typedef DWORD(WINAPI *PFN_GETWINDOWTHREADPROCESSID)(HWND hWnd, LPDWORD lpdwProcessId);
typedef HWND(WINAPI *PFN_GETWINDOW)(HWND hWnd, UINT uCmd);
typedef BOOL(WINAPI *PFN_ISWINDOWVISIBLE)(HWND hWnd);
typedef int(WINAPI *PFN_GETWINDOWTEXTLENGTHW)(HWND hWnd);
typedef int(WINAPI *PFN_GETWINDOWTEXTW)(HWND hWnd, LPWSTR lpString, int nMaxCount);
typedef BOOL(WINAPI *PFN_ENUMWINDOWS)(WNDENUMPROC lpEnumFunc, LPARAM lParam);

namespace
{
struct callbackContext
{
  callbackContext(QProcessList &l) : list(l) {}
  QProcessList &list;

  PFN_GETWINDOWTHREADPROCESSID GetWindowThreadProcessId = NULL;
  PFN_GETWINDOW GetWindow = NULL;
  PFN_ISWINDOWVISIBLE IsWindowVisible = NULL;
  PFN_GETWINDOWTEXTLENGTHW GetWindowTextLengthW = NULL;
  PFN_GETWINDOWTEXTW GetWindowTextW = NULL;
  PFN_ENUMWINDOWS EnumWindows = NULL;
};
};

static BOOL CALLBACK fillWindowTitles(HWND hwnd, LPARAM lp)
{
  callbackContext *ctx = (callbackContext *)lp;

  DWORD pid = 0;
  ctx->GetWindowThreadProcessId(hwnd, &pid);

  HWND parent = ctx->GetWindow(hwnd, GW_OWNER);

  if(parent != 0)
    return TRUE;

  if(!ctx->IsWindowVisible(hwnd))
    return TRUE;

  for(QProcessInfo &info : ctx->list)
  {
    if(info.pid() == (uint32_t)pid)
    {
      int len = ctx->GetWindowTextLengthW(hwnd);
      wchar_t *buf = new wchar_t[len + 1];
      ctx->GetWindowTextW(hwnd, buf, len + 1);
      buf[len] = 0;
      info.setWindowTitle(QString::fromStdWString(std::wstring(buf)));
      delete[] buf;
      return TRUE;
    }
  }

  return TRUE;
}

QProcessList QProcessInfo::enumerate()
{
  QProcessList ret;

  HANDLE h = NULL;
  PROCESSENTRY32 pe = {0};
  pe.dwSize = sizeof(PROCESSENTRY32);
  h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(Process32First(h, &pe))
  {
    do
    {
      QProcessInfo info;
      info.setPid((uint32_t)pe.th32ProcessID);
      info.setName(QString::fromStdWString(std::wstring(pe.szExeFile)));

      ret.push_back(info);
    } while(Process32Next(h, &pe));
  }
  CloseHandle(h);

  HMODULE user32 = LoadLibraryA("user32.dll");

  if(user32)
  {
    callbackContext ctx(ret);

    ctx.GetWindowThreadProcessId =
        (PFN_GETWINDOWTHREADPROCESSID)GetProcAddress(user32, "GetWindowThreadProcessId");
    ctx.GetWindow = (PFN_GETWINDOW)GetProcAddress(user32, "GetWindow");
    ctx.IsWindowVisible = (PFN_ISWINDOWVISIBLE)GetProcAddress(user32, "IsWindowVisible");
    ctx.GetWindowTextLengthW =
        (PFN_GETWINDOWTEXTLENGTHW)GetProcAddress(user32, "GetWindowTextLengthW");
    ctx.GetWindowTextW = (PFN_GETWINDOWTEXTW)GetProcAddress(user32, "GetWindowTextW");
    ctx.EnumWindows = (PFN_ENUMWINDOWS)GetProcAddress(user32, "EnumWindows");

    if(ctx.GetWindowThreadProcessId && ctx.GetWindow && ctx.IsWindowVisible &&
       ctx.GetWindowTextLengthW && ctx.GetWindowTextW && ctx.EnumWindows)
    {
      ctx.EnumWindows(fillWindowTitles, (LPARAM)&ctx);
    }

    FreeLibrary(user32);
  }

  return ret;
}

#elif defined(Q_OS_UNIX)

#include <QDir>
#include <QProcess>
#include <QRegExp>
#include <QStandardPaths>
#include <QTextStream>

QProcessList QProcessInfo::enumerate()
{
  QProcessList ret;

  QDir proc(QStringLiteral("/proc"));

  QStringList files = proc.entryList();

  for(const QString &f : files)
  {
    bool ok = false;
    uint32_t pid = f.toUInt(&ok);

    if(ok)
    {
      QProcessInfo info;
      info.setPid(pid);

      QDir processDir(QStringLiteral("/proc/") + f);

      // default to the exe symlink if valid
      QFileInfo exe(processDir.absoluteFilePath(QStringLiteral("exe")));
      exe = QFileInfo(exe.symLinkTarget());
      info.setName(exe.completeBaseName());

      // if we didn't get a name from the symlink, check in the status file
      if(info.name().isEmpty())
      {
        QFile status(processDir.absoluteFilePath(QStringLiteral("status")));
        if(status.open(QIODevice::ReadOnly))
        {
          QByteArray contents = status.readAll();

          QTextStream in(&contents);
          while(!in.atEnd())
          {
            QString line = in.readLine();

            if(line.startsWith(QStringLiteral("Name:")))
            {
              line.remove(0, 5);
              // if we're using this name, surround with []s to indicate it's not a file
              info.setName(QStringLiteral("[%1]").arg(line.trimmed()));
              break;
            }
          }
          status.close();
        }
      }

      // get the command line
      QFile cmdline(processDir.absoluteFilePath(QStringLiteral("cmdline")));
      if(cmdline.open(QIODevice::ReadOnly))
      {
        QByteArray contents = cmdline.readAll();

        int nullIdx = contents.indexOf('\0');

        if(nullIdx > 0)
        {
          QString firstparam = QString::fromUtf8(contents.data(), nullIdx);

          // if name is a truncated form of a filename, replace it
          if(firstparam.endsWith(info.name()) && QFileInfo::exists(firstparam))
            info.setName(QFileInfo(firstparam).completeBaseName());

          // if we don't have a name, replace it but with []s
          if(info.name().isEmpty())
            info.setName(QStringLiteral("[%1]").arg(firstparam));

          contents.replace('\0', ' ');
        }

        info.setCommandLine(QString::fromUtf8(contents).trimmed());

        cmdline.close();
      }

      ret.push_back(info);
    }
  }

  {
    // get a list of all windows. This is faster than searching with --pid
    // for every PID, and usually there will be fewer windows than PIDs.
    QStringList params;
    params << QStringLiteral("search") << QStringLiteral("--onlyvisible") << QStringLiteral(".*");

    QList<QByteArray> windowlist;

    QString inPath = QStandardPaths::findExecutable(QStringLiteral("xdotool"));

    if(inPath.isEmpty())
    {
      // add a fake window title to the first process to indicate that xdotool is missing
      if(!ret.isEmpty())
        ret[0].setWindowTitle(QStringLiteral("Window titles not available - install `xdotool`"));
    }
    else
    {
      QProcess process;
      process.start(QStringLiteral("xdotool"), params);
      process.waitForFinished(100);

      windowlist = process.readAll().split('\n');
    }

    // if xdotool isn't installed or failed to run, we'll have an empty
    // list or else entries that aren't numbers, so we'll skip them
    for(const QByteArray &win : windowlist)
    {
      // empty result, no window matches
      if(win.size() == 0)
        continue;

      bool isUInt = false;
      win.toUInt(&isUInt);

      // skip invalid lines (maybe because xdotool failed)
      if(!isUInt)
        continue;

      // get the PID of the window first. If one isn't available we won't
      // be able to match it up to our entries so don't proceed further
      params.clear();
      params << QStringLiteral("getwindowpid") << QString::fromLatin1(win);

      uint32_t pid = 0;

      {
        QProcess process;
        process.start(QStringLiteral("xdotool"), params);
        process.waitForFinished(100);

        pid = process.readAll().trimmed().toUInt(&isUInt);
      }

      // can't find a PID, skip this window
      if(!isUInt || pid == 0)
        continue;

      // check to see if the geometry is somewhere offscreen
      params.clear();
      params << QStringLiteral("getwindowgeometry") << QString::fromLatin1(win);

      QList<QByteArray> winGeometry;

      {
        QProcess process;
        process.start(QStringLiteral("xdotool"), params);
        process.waitForFinished(100);

        winGeometry = process.readAll().split('\n');
      }

      // should be three lines: Window <id> \n Position: ... \n Geometry: ...
      if(winGeometry.size() >= 3)
      {
        QRegExp pos(QStringLiteral("Position: (-?\\d+),(-?\\d+)"));
        QRegExp geometry(QStringLiteral("Geometry: (\\d+)x(\\d+)"));

        QString posString = QString::fromUtf8(winGeometry[1]);
        QString geometryString = QString::fromUtf8(winGeometry[2]);

        int x = 0, y = 0, w = 1000, h = 1000;

        if(pos.indexIn(posString) >= 0)
        {
          x = pos.cap(1).toInt();
          y = pos.cap(2).toInt();
        }

        if(geometry.indexIn(geometryString) >= 0)
        {
          w = geometry.cap(1).toInt();
          h = geometry.cap(2).toInt();
        }

        // some invisible windows are placed off screen, if we detect that skip it
        if(x + w < 0 && y + h < 0)
          continue;
      }

      // take the first window name
      {
        params.clear();
        params << QStringLiteral("getwindowname") << QString::fromLatin1(win);

        QProcess process;
        process.start(QStringLiteral("xdotool"), params);
        process.waitForFinished(100);

        QString windowTitle = QString::fromUtf8(process.readAll().split('\n')[0]);

        for(QProcessInfo &info : ret)
        {
          if(info.pid() == pid)
          {
            info.setWindowTitle(windowTitle);
            break;
          }
        }
      }
    }
  }

  return ret;
}

#else

QProcessList QProcessInfo::enumerate()
{
  QProcessList ret;

  qWarning() << "Process enumeration not supported on this platform";

  return ret;
}

#endif

QProcessInfo::QProcessInfo()
{
  m_pid = 0;
}

uint32_t QProcessInfo::pid() const
{
  return m_pid;
}

void QProcessInfo::setPid(uint32_t pid)
{
  m_pid = pid;
}

const QString &QProcessInfo::name() const
{
  return m_name;
}

void QProcessInfo::setName(const QString &name)
{
  m_name = name;
}

const QString &QProcessInfo::windowTitle() const
{
  return m_title;
}

void QProcessInfo::setWindowTitle(const QString &title)
{
  m_title = title;
}

const QString &QProcessInfo::commandLine() const
{
  return m_cmdLine;
}

void QProcessInfo::setCommandLine(const QString &cmd)
{
  m_cmdLine = cmd;
}
