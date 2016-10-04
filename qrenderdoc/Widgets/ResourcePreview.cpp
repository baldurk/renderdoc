#include "ResourcePreview.h"
#include "ui_ResourcePreview.h"

ResourcePreview::ResourcePreview(CaptureContext *c, IReplayOutput *output, QWidget *parent)
    : QFrame(parent), ui(new Ui::ResourcePreview)
{
  ui->setupUi(this);

  ui->thumbnail->SetOutput(c, output);

  QPalette Pal(ui->slotLabel->palette());

  QWidget tmp;

  Pal.setColor(ui->slotLabel->foregroundRole(), tmp.palette().color(QPalette::Foreground));
  Pal.setColor(ui->slotLabel->backgroundRole(), tmp.palette().color(QPalette::Dark));

  ui->slotLabel->setAutoFillBackground(true);
  ui->slotLabel->setPalette(Pal);
  ui->descriptionLabel->setAutoFillBackground(true);
  ui->descriptionLabel->setPalette(Pal);
}

ResourcePreview::~ResourcePreview()
{
  delete ui;
}

void ResourcePreview::setSlotName(const QString &n)
{
  ui->slotLabel->setText(n);
}

void ResourcePreview::setResourceName(const QString &n)
{
  ui->descriptionLabel->setText(n);
}

void ResourcePreview::setSize(QSize s)
{
  setFixedWidth(s.width());
  setFixedHeight(s.height());
  setMinimumSize(s);
  setMaximumSize(s);
}

void ResourcePreview::setSelected(bool sel)
{
  QPalette Pal(palette());

  Pal.setColor(QPalette::Foreground, sel ? Qt::red : Qt::black);

  setPalette(Pal);
}

WId ResourcePreview::thumbWinId()
{
  return ui->thumbnail->winId();
}
