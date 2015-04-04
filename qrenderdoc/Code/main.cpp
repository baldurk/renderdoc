#include "Windows/MainWindow.h"
#include <QApplication>

#include "renderdoc_replay.h"

int main(int argc, char *argv[])
{
    RENDERDOC_LogText("QRenderDoc initialising.");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
