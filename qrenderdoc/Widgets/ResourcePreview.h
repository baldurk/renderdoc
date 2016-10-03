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

private:
  Ui::ResourcePreview *ui;
};

#endif    // RESOURCEPREVIEW_H
