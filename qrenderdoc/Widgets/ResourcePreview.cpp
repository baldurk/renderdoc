#include "ResourcePreview.h"
#include "ui_ResourcePreview.h"

ResourcePreview::ResourcePreview(QWidget *parent) : QWidget(parent), ui(new Ui::ResourcePreview)
{
  ui->setupUi(this);
}

ResourcePreview::~ResourcePreview()
{
  delete ui;
}

void ResourcePreview::SetSize(QSize s)
{
  setFixedWidth(s.width());
  setFixedHeight(s.height());
  setMinimumSize(s);
  setMaximumSize(s);
}
