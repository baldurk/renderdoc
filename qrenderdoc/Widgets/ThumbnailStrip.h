#ifndef THUMBNAILSTRIP_H
#define THUMBNAILSTRIP_H

#include <QWidget>

namespace Ui
{
class ThumbnailStrip;
}

class ResourcePreview;
class QBoxLayout;

class ThumbnailStrip : public QWidget
{
  Q_OBJECT

public:
  explicit ThumbnailStrip(QWidget *parent = 0);
  ~ThumbnailStrip();

  void AddPreview(ResourcePreview *prev);

  void ClearThumbnails();
  const QVector<ResourcePreview *> &GetThumbs() { return m_Thumbnails; }
  void RefreshLayout();

signals:
  void mouseClick(QMouseEvent *event);

private:
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

  QVector<ResourcePreview *> m_Thumbnails;

  QBoxLayout *layout;

  Ui::ThumbnailStrip *ui;
};

#endif    // THUMBNAILSTRIP_H
