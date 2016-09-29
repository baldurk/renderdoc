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
  void on_render_mousemove(QMouseEvent *e);

private:
  void RT_FetchCurrentPixel(uint32_t x, uint32_t y, PixelValue &pickValue, PixelValue &realValue);
  void RT_PickPixelsAndUpdate();
  void RT_PickHoverAndUpdate();

  void UI_UpdateStatusText();

  QPoint m_DragStartScroll;
  QPoint m_DragStartPos;

  QPoint m_CurHoverPixel;
  QPoint m_PickedPoint;

  PixelValue m_CurRealValue;
  PixelValue m_CurPixelValue;
  PixelValue m_CurHoverValue;

  int m_HighWaterStatusLength;

  Ui::TextureViewer *ui;
  Core *m_Core;
  IReplayOutput *m_Output;

  TextureDisplay m_TexDisplay;
};

#endif    // TEXTUREVIEWER_H
