#ifndef RESOURCEPREVIEW_H
#define RESOURCEPREVIEW_H

#include <QFrame>

namespace Ui
{
class ResourcePreview;
}

struct IReplayOutput;
class CaptureContext;

class ResourcePreview : public QFrame
{
  Q_OBJECT

public:
  explicit ResourcePreview(CaptureContext *c, IReplayOutput *output, QWidget *parent = 0);
  ~ResourcePreview();

  void setSlotName(const QString &n);
  void setResourceName(const QString &n);

  WId thumbWinId();

  void setActive(bool b)
  {
    m_Active = b;
    if(b)
      show();
    else
      hide();
  }
  bool isActive() { return m_Active; }
  void setSize(QSize s);

  void setSelected(bool sel);

private:
  Ui::ResourcePreview *ui;

  bool m_Active;
};

#endif    // RESOURCEPREVIEW_H
