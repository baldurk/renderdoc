#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include "Code/Core.h"
#include "Windows/MainWindow.h"

int main(int argc, char *argv[])
{
  RENDERDOC_LogText("QRenderDoc initialising.");

  QString filename = "";
  bool temp = false;

  for(int i = 0; i < argc; i++)
  {
    if(!QString::compare(argv[i], "--tempfile", Qt::CaseInsensitive))
      temp = true;
  }

  QString remoteHost = "";
  uint remoteIdent = 0;

  for(int i = 0; i + 1 < argc; i++)
  {
    if(!QString::compare(argv[i], "--REMOTEACCESS", Qt::CaseInsensitive))
    {
      QRegularExpression regexp("^([a-zA-Z0-9_-]+:)?([0-9]+)$");

      QRegularExpressionMatch match = regexp.match(argv[i + 1]);

      if(match.hasMatch())
      {
        QString host = match.captured(1);

        if(host.length() > 0 && host[host.length() - 1] == ':')
          host.chop(1);

        bool ok = false;
        uint32_t ident = match.captured(2).toUInt(&ok);

        if(ok)
        {
          remoteHost = host;
          remoteIdent = ident;
        }
      }
    }
  }

  if(argc > 1)
  {
    filename = argv[argc - 1];
    QFileInfo checkFile(filename);
    if(!checkFile.exists() || !checkFile.isFile())
      filename = "";
  }

  argc += 2;

  char **argv_mod = new char *[argc];

  for(int i = 0; i < argc - 2; i++)
    argv_mod[i] = argv[i];

  char arg[] = "-platformpluginpath";
  QString path = QFileInfo(argv[0]).absoluteDir().absolutePath();
  QByteArray pathChars = path.toUtf8();

  argv_mod[argc - 2] = arg;
  argv_mod[argc - 1] = pathChars.data();

  QApplication a(argc, argv_mod);

  Core core(filename, remoteHost, remoteIdent, temp);

  int ret = a.exec();

  delete[] argv_mod;

  return ret;
}
