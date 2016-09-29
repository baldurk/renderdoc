#-------------------------------------------------
#
# Project created by QtCreator 2015-03-18T20:10:50
#
#-------------------------------------------------

QT       += core gui widgets

lessThan(QT_MAJOR_VERSION, 5): error("requires Qt 5")

TARGET = qrenderdoc
TEMPLATE = app

# include path for core renderdoc API
INCLUDEPATH += $$_PRO_FILE_PWD_/../renderdoc/api/replay

# Allow includes relative to the root
INCLUDEPATH += $$_PRO_FILE_PWD_/

# For ToolWindowManager
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/toolwindowmanager
# For FlowLayout
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/flowlayout

# Different output folders per platform
win32 {

	!contains(QMAKE_TARGET.arch, x86_64) {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Development
		Release:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Release

		Debug:MOC_DIR = $$_PRO_FILE_PWD_/Win32/Development
		Release:MOC_DIR = $$_PRO_FILE_PWD_/Win32/Release
		Debug:UI_DIR = $$_PRO_FILE_PWD_/Win32/Development
		Release:UI_DIR = $$_PRO_FILE_PWD_/Win32/Release
		Debug:RCC_DIR = $$_PRO_FILE_PWD_/Win32/Development
		Release:RCC_DIR = $$_PRO_FILE_PWD_/Win32/Release
		Debug:OBJECTS_DIR = $$_PRO_FILE_PWD_/Win32/Development
		Release:OBJECTS_DIR = $$_PRO_FILE_PWD_/Win32/Release

	} else {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../x64/Development
		Release:DESTDIR = $$_PRO_FILE_PWD_/../x64/Release

		Debug:MOC_DIR = $$_PRO_FILE_PWD_/x64/Development
		Release:MOC_DIR = $$_PRO_FILE_PWD_/x64/Release
		Debug:UI_DIR = $$_PRO_FILE_PWD_/x64/Development
		Release:UI_DIR = $$_PRO_FILE_PWD_/x64/Release
		Debug:RCC_DIR = $$_PRO_FILE_PWD_/x64/Development
		Release:RCC_DIR = $$_PRO_FILE_PWD_/x64/Release
		Debug:OBJECTS_DIR = $$_PRO_FILE_PWD_/x64/Development
		Release:OBJECTS_DIR = $$_PRO_FILE_PWD_/x64/Release
	}

	# Link against the core library
	LIBS += $$DESTDIR/renderdoc.lib

	QMAKE_CXXFLAGS_WARN_ON -= -w34100 
	DEFINES += RENDERDOC_PLATFORM_WIN32

} else {
	isEmpty(DESTDIR) {
		DESTDIR = $$_PRO_FILE_PWD_/../bin
	}

	# Temp files into .obj
	MOC_DIR = .obj
	UI_DIR = .obj
	RCC_DIR = .obj
	OBJECTS_DIR = .obj

	# Link against the core library
	LIBS += -L$$DESTDIR -lrenderdoc
	QMAKE_LFLAGS += '-Wl,-rpath,\'\$$ORIGIN\''

	QMAKE_CXXFLAGS += -std=c++11 -Wno-unused-parameter -Wno-reorder

	QT += x11extras
	DEFINES += RENDERDOC_PLATFORM_POSIX RENDERDOC_PLATFORM_LINUX
}

SOURCES += Code/main.cpp \
    Windows/MainWindow.cpp \
    Windows/EventBrowser.cpp \
    Windows/TextureViewer.cpp \
    Windows/AboutDialog.cpp \
    Widgets/CustomPaintWidget.cpp \
    3rdparty/toolwindowmanager/ToolWindowManager.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.cpp \
    Code/RenderManager.cpp \
    Code/Core.cpp \
    Widgets/LineEditFocusWidget.cpp \
    3rdparty/flowlayout/FlowLayout.cpp

HEADERS  += Windows/MainWindow.h \
    Windows/EventBrowser.h \
    Windows/TextureViewer.h \
    Windows/AboutDialog.h \
    Widgets/CustomPaintWidget.h \
    3rdparty/toolwindowmanager/ToolWindowManager.h \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.h \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.h \
    Code/Core.h \
    Code/RenderManager.h \
    Widgets/LineEditFocusWidget.h \
    3rdparty/flowlayout/FlowLayout.h

FORMS    += Windows/MainWindow.ui \
    Windows/EventBrowser.ui \
    Windows/TextureViewer.ui \
    Windows/AboutDialog.ui

RESOURCES += \
    resources.qrc
