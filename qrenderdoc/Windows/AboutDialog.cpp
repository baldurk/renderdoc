#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include <QApplication>
#include <QLabel>
#include <QString>

AboutDialog::AboutDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::AboutDialog)
{
        ui->setupUi(this);
        ui->version->setText("Version v" + qApp->applicationVersion());
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
