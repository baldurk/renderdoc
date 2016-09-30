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
  void render_mouseClick(QMouseEvent *e);
  void render_mouseMove(QMouseEvent *e);
  void render_resize(QResizeEvent *e);
  void on_renderHScroll_valueChanged(int position);
  void on_renderVScroll_valueChanged(int position);

private:
  void RT_FetchCurrentPixel(uint32_t x, uint32_t y, PixelValue &pickValue, PixelValue &realValue);
  void RT_PickPixelsAndUpdate();
  void RT_PickHoverAndUpdate();
  void RT_UpdateAndDisplay();

  void UI_UpdateStatusText();
  void UI_UpdateTextureDetails();
  void UI_OnTextureSelectionChanged(bool newdraw);

  bool ScrollUpdateScrollbars = true;

  float CurMaxScrollX();
  float CurMaxScrollY();

  QPoint getScrollPosition();
  void setScrollPosition(const QPoint &pos);

  void UI_SetScale(float s, int x, int y);
  void UI_CalcScrollbars();

  QPoint m_DragStartScroll;
  QPoint m_DragStartPos;

  QPoint m_CurHoverPixel;
  QPoint m_PickedPoint;

  PixelValue m_CurRealValue;
  PixelValue m_CurPixelValue;
  PixelValue m_CurHoverValue;

  int m_HighWaterStatusLength = 0;

  Ui::TextureViewer *ui;
  Core *m_Core = NULL;
  IReplayOutput *m_Output = NULL;

  TextureDisplay m_TexDisplay;
};

#endif    // TEXTUREVIEWER_H
