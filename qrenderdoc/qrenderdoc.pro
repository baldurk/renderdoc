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
# For Scintilla
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/include/qt
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/include

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

	CONFIG += warn_off
	CONFIG += c++11
	QMAKE_CFLAGS_WARN_OFF -= -w
	QMAKE_CXXFLAGS_WARN_OFF -= -w

	macx: {
		DEFINES += RENDERDOC_PLATFORM_POSIX RENDERDOC_PLATFORM_APPLE
	} else {
		QT += x11extras
		DEFINES += RENDERDOC_PLATFORM_POSIX RENDERDOC_PLATFORM_LINUX RENDERDOC_WINDOWING_XLIB RENDERDOC_WINDOWING_XCB
	}
}

# Add our sources first so Qt Creator adds new files here

SOURCES += Code/qrenderdoc.cpp \
    Code/qprocessinfo.cpp \
    Code/RenderManager.cpp \
    Code/CommonPipelineState.cpp \
    Code/PersistantConfig.cpp \
    Code/CaptureContext.cpp \
    Code/ScintillaSyntax.cpp \
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
    Widgets/Extended/RDListWidget.cpp \
    Windows/APIInspector.cpp \
    Windows/PipelineState/PipelineStateViewer.cpp \
    Windows/PipelineState/VulkanPipelineStateViewer.cpp \
    Windows/PipelineState/D3D11PipelineStateViewer.cpp \
    Windows/PipelineState/D3D12PipelineStateViewer.cpp \
    Windows/PipelineState/GLPipelineStateViewer.cpp \
    Widgets/Extended/RDTreeWidget.cpp \
    Windows/ConstantBufferPreviewer.cpp \
    Widgets/BufferFormatSpecifier.cpp \
    Code/FormatElement.cpp \
    Windows/BufferViewer.cpp \
    Widgets/Extended/RDTableView.cpp \
    Windows/DebugMessageView.cpp \
    Windows/StatisticsViewer.cpp \
    Windows/Dialogs/SettingsDialog.cpp \
    Windows/Dialogs/OrderedListEditor.cpp \
    Widgets/Extended/RDTableWidget.cpp

HEADERS += Code/CaptureContext.h \
    Code/qprocessinfo.h \
    Code/RenderManager.h \
    Code/PersistantConfig.h \
    Code/CommonPipelineState.h \
    Code/ScintillaSyntax.h \
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
    Widgets/Extended/RDListWidget.h \
    Windows/APIInspector.h \
    Windows/PipelineState/PipelineStateViewer.h \
    Windows/PipelineState/VulkanPipelineStateViewer.h \
    Windows/PipelineState/D3D11PipelineStateViewer.h \
    Windows/PipelineState/D3D12PipelineStateViewer.h \
    Windows/PipelineState/GLPipelineStateViewer.h \
    Widgets/Extended/RDTreeWidget.h \
    Windows/ConstantBufferPreviewer.h \
    Widgets/BufferFormatSpecifier.h \
    Windows/BufferViewer.h \
    Widgets/Extended/RDTableView.h \
    Windows/DebugMessageView.h \
    Windows/StatisticsViewer.h \
    Windows/Dialogs/SettingsDialog.h \
    Windows/Dialogs/OrderedListEditor.h \
    Widgets/Extended/RDTableWidget.h

FORMS    += Windows/Dialogs/AboutDialog.ui \
    Windows/MainWindow.ui \
    Windows/EventBrowser.ui \
    Windows/TextureViewer.ui \
    Widgets/ResourcePreview.ui \
    Widgets/ThumbnailStrip.ui \
    Windows/Dialogs/TextureSaveDialog.ui \
    Windows/Dialogs/CaptureDialog.ui \
    Windows/Dialogs/LiveCapture.ui \
    Windows/APIInspector.ui \
    Windows/PipelineState/PipelineStateViewer.ui \
    Windows/PipelineState/VulkanPipelineStateViewer.ui \
    Windows/PipelineState/D3D11PipelineStateViewer.ui \
    Windows/PipelineState/D3D12PipelineStateViewer.ui \
    Windows/PipelineState/GLPipelineStateViewer.ui \
    Windows/ConstantBufferPreviewer.ui \
    Widgets/BufferFormatSpecifier.ui \
    Windows/BufferViewer.ui \
    Windows/ShaderViewer.ui \
    Windows/DebugMessageView.ui \
    Windows/StatisticsViewer.ui \
    Windows/Dialogs/SettingsDialog.ui \
    Windows/Dialogs/OrderedListEditor.ui

RESOURCES += resources.qrc

# Add ToolWindowManager

SOURCES += 3rdparty/toolwindowmanager/ToolWindowManager.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.cpp

HEADERS += 3rdparty/toolwindowmanager/ToolWindowManager.h \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.h \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.h

# Add FlowLayout

SOURCES += 3rdparty/flowlayout/FlowLayout.cpp
HEADERS += 3rdparty/flowlayout/FlowLayout.h

# Add Scintilla last as it has extra search paths

# Needed for building
DEFINES += SCINTILLA_QT=1 MAKING_LIBRARY=1 SCI_LEXER=1
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/src
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/lexlib

SOURCES += $$_PRO_FILE_PWD_/3rdparty/scintilla/lexlib/*.cxx \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/lexers/*.cxx \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/src/*.cxx \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEdit/*.cpp \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEditBase/*.cpp \
    Windows/ShaderViewer.cpp

HEADERS += $$_PRO_FILE_PWD_/3rdparty/scintilla/lexlib/*.h \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/src/*.h \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEdit/*.h \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEditBase/*.h \
    Windows/ShaderViewer.h

