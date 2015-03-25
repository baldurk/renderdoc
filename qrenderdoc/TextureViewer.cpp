#include "TextureViewer.h"
#include "ui_TextureViewer.h"

TextureViewer::TextureViewer(QWidget *parent) :
  QFrame(parent),
  ui(new Ui::TextureViewer)
{
  ui->setupUi(this);
}

TextureViewer::~TextureViewer()
{
  delete ui;
}
