#ifndef TEXTUREVIEWER_H
#define TEXTUREVIEWER_H

#include <QFrame>
#include <QMouseEvent>
#include "Code/Core.h"

namespace Ui
{
class TextureViewer;
}

class TextureViewer : public QFrame, public ILogViewerForm
{
private:
  Q_OBJECT

public:
  explicit TextureViewer(Core *core, QWidget *parent = 0);
  ~TextureViewer();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnEventSelected(uint32_t eventID);

private slots:
  void on_render_clicked(QMouseEvent *e);

private:
  Ui::TextureViewer *ui;
  Core *m_Core;
  IReplayOutput *m_Output;

  TextureDisplay m_TexDisplay;
};

#endif    // TEXTUREVIEWER_H
