#ifndef TEXTUREVIEWER_H
#define TEXTUREVIEWER_H

#include <QFrame>

#include "Code/Core.h"

namespace Ui {
class TextureViewer;
}

class TextureViewer : public QFrame, public ILogViewerForm
{
    Q_OBJECT

  public:
    explicit TextureViewer(Core *core, QWidget *parent = 0);
    ~TextureViewer();

    void OnLogfileLoaded();
    void OnLogfileClosed();
    void OnEventSelected(uint32_t frameID, uint32_t eventID);

  private:
    Ui::TextureViewer *ui;
    Core *m_Core;
    IReplayOutput *m_Output;
};

#endif // TEXTUREVIEWER_H
