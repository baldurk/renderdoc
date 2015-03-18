#-------------------------------------------------
#
# Project created by QtCreator 2015-03-18T20:10:50
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qrenderdoc
TEMPLATE = app

# Temp files into .obj
MOC_DIR = .obj
UI_DIR = .obj
OBJECTS_DIR = .obj

# include path for core renderdoc API
INCLUDEPATH += $$_PRO_FILE_PWD_/../renderdoc/api/replay

# Different output folders per platform
win32 {

	!contains(QMAKE_TARGET.arch, x86_64) {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Profile
		Release:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Release

	} else {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../x64/Profile
		Release:DESTDIR = $$_PRO_FILE_PWD_/../x64/Release
	}

	# Link against the core library
	LIBS += $$DESTDIR/renderdoc.lib

} else {

	DESTDIR = $$_PRO_FILE_PWD_/../bin

	# Link against the core library
	LIBS += -L$$_PRO_FILE_PWD_/../renderdoc -lrenderdoc

}

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui
