#pragma once
#include <QLineEdit>

class LineEditFocusWidget : public QLineEdit
{
	private:
		Q_OBJECT
	public:
		explicit LineEditFocusWidget(QWidget *parent = 0);
		~LineEditFocusWidget();

	signals:
		void enter();
		void leave();

		public slots:

	protected:
		void focusInEvent(QFocusEvent *e);
		void focusOutEvent(QFocusEvent *e);
};
