#include "EventBrowser.h"
#include "ui_EventBrowser.h"

EventBrowser::EventBrowser(QWidget *parent) :
  QFrame(parent),
  ui(new Ui::EventBrowser)
{
  ui->setupUi(this);
}

EventBrowser::~EventBrowser()
{
  delete ui;
}
