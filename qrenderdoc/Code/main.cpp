#include "Windows/MainWindow.h"
#include "Code/Core.h"

#include <QApplication>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>

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

	QApplication a(argc, argv);

	Core core(filename, remoteHost, remoteIdent, temp);

	return a.exec();
}
