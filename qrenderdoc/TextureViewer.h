#ifndef TEXTUREVIEWER_H
#define TEXTUREVIEWER_H

#include <QFrame>

namespace Ui {
class TextureViewer;
}

class TextureViewer : public QFrame
{
    Q_OBJECT

  public:
    explicit TextureViewer(QWidget *parent = 0);
    ~TextureViewer();

    QWidget *renderSurf();

  private:
    Ui::TextureViewer *ui;
};

#endif // TEXTUREVIEWER_H
