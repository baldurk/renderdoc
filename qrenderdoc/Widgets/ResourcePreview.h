#ifndef RESOURCEPREVIEW_H
#define RESOURCEPREVIEW_H

#include <QWidget>

namespace Ui
{
class ResourcePreview;
}

class ResourcePreview : public QWidget
{
  Q_OBJECT

public:
  explicit ResourcePreview(QWidget *parent = 0);
  ~ResourcePreview();

  void setActive(bool b)
  {
    m_Active = b;
    if(b)
      show();
    else
      hide();
  }
  bool isActive() { return m_Active; }
  void SetSize(QSize s);

private:
  Ui::ResourcePreview *ui;

  bool m_Active;
};

#endif    // RESOURCEPREVIEW_H
