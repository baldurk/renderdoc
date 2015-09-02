#pragma once

#include <QWidget>

struct IReplayOutput;

class CustomPaintWidget : public QWidget
{
	private:
		Q_OBJECT
	public:
		explicit CustomPaintWidget(QWidget *parent = 0);
		~CustomPaintWidget();

		void SetOutput(IReplayOutput *out) { m_Output = out; }

    signals:
        void clicked(QMouseEvent *e);
        void mouseMove(QMouseEvent *e);

    private slots:
        void mousePressEvent(QMouseEvent *e);
        void mouseMoveEvent(QMouseEvent *e);

	public slots:

	protected:
		void paintEvent(QPaintEvent *e);
		QPaintEngine *paintEngine() const { return NULL; }

		IReplayOutput *m_Output;
};
