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
