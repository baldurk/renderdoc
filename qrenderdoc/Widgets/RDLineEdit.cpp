#include "RDLineEdit.h"

RDLineEdit::RDLineEdit(QWidget *parent) : QLineEdit(parent)
{
}

RDLineEdit::~RDLineEdit()
{
}

void RDLineEdit::focusInEvent(QFocusEvent *e)
{
  QLineEdit::focusInEvent(e);
  emit(enter());
}

void RDLineEdit::focusOutEvent(QFocusEvent *e)
{
  QLineEdit::focusOutEvent(e);
  emit(leave());
}
