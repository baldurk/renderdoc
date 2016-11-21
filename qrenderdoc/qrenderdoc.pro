#-------------------------------------------------
#
# Project created by QtCreator 2015-03-18T20:10:50
#
#-------------------------------------------------

QT       += core gui widgets

lessThan(QT_MAJOR_VERSION, 5): error("requires Qt 5")

equals(QT_MAJOR_VERSION, 5): lessThan(QT_MINOR_VERSION, 6): error("requires Qt 5.6")

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

	RC_INCLUDEPATH = $$_PRO_FILE_PWD_/../renderdoc/api/replay
	RC_FILE = Resources/qrenderdoc.rc

	# it is fine to alias these across targets, because the output
	# is identical on all targets
	MOC_DIR = $$_PRO_FILE_PWD_/generated
	UI_DIR = $$_PRO_FILE_PWD_/generated
	RCC_DIR = $$_PRO_FILE_PWD_/generated

	# generate pdb files even in release
	QMAKE_LFLAGS_RELEASE+=/MAP
	QMAKE_CFLAGS_RELEASE += /Zi
	QMAKE_LFLAGS_RELEASE +=/debug /opt:ref

	!contains(QMAKE_TARGET.arch, x86_64) {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Development
		Release:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Release

		Debug:OBJECTS_DIR = $$_PRO_FILE_PWD_/Win32/Development
		Release:OBJECTS_DIR = $$_PRO_FILE_PWD_/Win32/Release

	} else {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../x64/Development
		Release:DESTDIR = $$_PRO_FILE_PWD_/../x64/Release

		Debug:OBJECTS_DIR = $$_PRO_FILE_PWD_/obj/x64/Development
		Release:OBJECTS_DIR = $$_PRO_FILE_PWD_/obj/x64/Release
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
	DEFINES += RENDERDOC_PLATFORM_POSIX RENDERDOC_PLATFORM_LINUX RENDERDOC_WINDOWING_XLIB RENDERDOC_WINDOWING_XCB
}

SOURCES += 3rdparty/toolwindowmanager/ToolWindowManager.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.cpp \
    3rdparty/flowlayout/FlowLayout.cpp \
    Code/qrenderdoc.cpp \
    Code/qprocessinfo.cpp \
    Code/RenderManager.cpp \
    Code/CommonPipelineState.cpp \
    Code/PersistantConfig.cpp \
    Code/CaptureContext.cpp \
    Windows/Dialogs/AboutDialog.cpp \
    Windows/MainWindow.cpp \
    Windows/EventBrowser.cpp \
    Windows/TextureViewer.cpp \
    Widgets/Extended/RDLineEdit.cpp \
    Widgets/Extended/RDLabel.cpp \
    Widgets/Extended/RDDoubleSpinBox.cpp \
    Widgets/Extended/RDListView.cpp \
    Widgets/CustomPaintWidget.cpp \
    Widgets/ResourcePreview.cpp \
    Widgets/ThumbnailStrip.cpp \
    Widgets/TextureGoto.cpp \
    Widgets/RangeHistogram.cpp \
    Windows/Dialogs/TextureSaveDialog.cpp \
    Windows/Dialogs/CaptureDialog.cpp \
    Code/QRDUtils.cpp \
    Windows/Dialogs/LiveCapture.cpp \
    Widgets/Extended/RDListWidget.cpp

HEADERS  += 3rdparty/toolwindowmanager/ToolWindowManager.h \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.h \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.h \
    3rdparty/flowlayout/FlowLayout.h \
    Code/CaptureContext.h \
    Code/qprocessinfo.h \
    Code/RenderManager.h \
    Code/PersistantConfig.h \
    Code/CommonPipelineState.h \
    Windows/Dialogs/AboutDialog.h \
    Windows/MainWindow.h \
    Windows/EventBrowser.h \
    Windows/TextureViewer.h \
    Widgets/Extended/RDLineEdit.h \
    Widgets/Extended/RDLabel.h \
    Widgets/Extended/RDDoubleSpinBox.h \
    Widgets/Extended/RDListView.h \
    Widgets/CustomPaintWidget.h \
    Widgets/ResourcePreview.h \
    Widgets/ThumbnailStrip.h \
    Widgets/TextureGoto.h \
    Widgets/RangeHistogram.h \
    Windows/Dialogs/TextureSaveDialog.h \
    Windows/Dialogs/CaptureDialog.h \
    Code/QRDUtils.h \
    Windows/Dialogs/LiveCapture.h \
    Widgets/Extended/RDListWidget.h

FORMS    += Windows/Dialogs/AboutDialog.ui \
    Windows/MainWindow.ui \
    Windows/EventBrowser.ui \
    Windows/TextureViewer.ui \
    Widgets/ResourcePreview.ui \
    Widgets/ThumbnailStrip.ui \
    Windows/Dialogs/TextureSaveDialog.ui \
    Windows/Dialogs/CaptureDialog.ui \
    Windows/Dialogs/LiveCapture.ui

RESOURCES += \
    resources.qrc
