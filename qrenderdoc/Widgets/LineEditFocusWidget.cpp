#include "LineEditFocusWidget.h"

LineEditFocusWidget::LineEditFocusWidget(QWidget *parent) : QLineEdit(parent)
{
}

LineEditFocusWidget::~LineEditFocusWidget()
{

}

void LineEditFocusWidget::focusInEvent(QFocusEvent *e)
{
	QLineEdit::focusInEvent(e);
	emit(enter());
}

void LineEditFocusWidget::focusOutEvent(QFocusEvent *e)
{
	QLineEdit::focusOutEvent(e);
	emit(leave());
}
