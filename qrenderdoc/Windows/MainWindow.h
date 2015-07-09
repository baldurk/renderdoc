#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <stdint.h>

namespace Ui {
	class MainWindow;
}

class Core;

class MainWindow : public QMainWindow
{
	private:
		Q_OBJECT

	public:
		explicit MainWindow(Core *core);
		~MainWindow();

		private slots:
		void on_action_Exit_triggered();

		void on_action_Open_Log_triggered();

	private:
		Ui::MainWindow *ui;
		Core *m_Core;
};

#endif // MAINWINDOW_H
