#-------------------------------------------------
#
# Project created by QtCreator 2015-03-18T20:10:50
#
#-------------------------------------------------

QT       += core gui widgets svg network

CONFIG   += silent

lessThan(QT_MAJOR_VERSION, 5): error("requires Qt 5.6; found $$[QT_VERSION]")

equals(QT_MAJOR_VERSION, 5): lessThan(QT_MINOR_VERSION, 6): error("requires Qt 5.6; found $$[QT_VERSION]")

TARGET = qrenderdoc
TEMPLATE = app

# include path for core renderdoc API
INCLUDEPATH += $$_PRO_FILE_PWD_/../renderdoc/api/replay

# Allow includes relative to the root
INCLUDEPATH += $$_PRO_FILE_PWD_/

# And relative to 3rdparty
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty

# For Scintilla source builds - we unfortunately are not able to scope these to only
# those source files
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/include/qt
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/include

# Disable conversions to/from const char * in QString
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

# Disable deprecation warnings that are default on in 5.13 and up
DEFINES += QT_NO_DEPRECATED_WARNINGS

# HA HA good joke, QT_NO_DEPRECATED_WARNINGS only covers SOME warnings, not all
QMAKE_CXXFLAGS += -Wno-deprecated-declarations

# Different output folders per platform
win32 {

	RC_INCLUDEPATH = $$_PRO_FILE_PWD_/../renderdoc/api/replay
	RC_FILE = Resources/qrenderdoc.rc

	# generate pdb files even in release
	QMAKE_LFLAGS_RELEASE+=/MAP
	QMAKE_CFLAGS_RELEASE += /Zi
	QMAKE_LFLAGS_RELEASE +=/debug /opt:ref

	!contains(QMAKE_TARGET.arch, x86_64) {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Development
		Release:DESTDIR = $$_PRO_FILE_PWD_/../Win32/Release
	} else {
		Debug:DESTDIR = $$_PRO_FILE_PWD_/../x64/Development
		Release:DESTDIR = $$_PRO_FILE_PWD_/../x64/Release
	}

	# Run SWIG here, since normally we run it from VS
	swig.name = SWIG ${QMAKE_FILE_IN}
	swig.input = SWIGSOURCES
	swig.output = ${QMAKE_FILE_BASE}_python.cxx
	swig.commands = $$_PRO_FILE_PWD_/3rdparty/swig/swig.exe -v -Wextra -Werror -O -interface ${QMAKE_FILE_BASE} -c++ -python -modern -modernargs -enumclass -fastunpack -py3 -builtin -I$$_PRO_FILE_PWD_ -I$$_PRO_FILE_PWD_/../renderdoc/api/replay -outdir . -o ${QMAKE_FILE_BASE}_python.cxx ${QMAKE_FILE_IN}
	swig.CONFIG += target_predeps
	swig.variable_out = GENERATED_SOURCES
	silent:swig.commands = @echo SWIG ${QMAKE_FILE_IN} && $$swig.commands
	QMAKE_EXTRA_COMPILERS += swig

	# add qrc file with qt.conf
	RESOURCES += Resources/qtconf.qrc

	SWIGSOURCES += Code/pyrenderdoc/renderdoc.i
	SWIGSOURCES += Code/pyrenderdoc/qrenderdoc.i

	# Include and link against python
	INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/python/include
	!contains(QMAKE_TARGET.arch, x86_64) {
		LIBS += $$_PRO_FILE_PWD_/3rdparty/python/Win32/python36.lib
	} else {
		LIBS += $$_PRO_FILE_PWD_/3rdparty/python/x64/python36.lib
	}

	# Include and link against PySide2
	exists( $$_PRO_FILE_PWD_/3rdparty/pyside/include/PySide2/pyside.h ) {
		DEFINES += PYSIDE2_ENABLED=1
		INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/pyside/include/shiboken2
		INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/pyside/include/PySide2
		INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/pyside/include/PySide2/QtCore
		INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/pyside/include/PySide2/QtGui
		INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/pyside/include/PySide2/QtWidgets
		!contains(QMAKE_TARGET.arch, x86_64) {
			LIBS += $$_PRO_FILE_PWD_/3rdparty/pyside/Win32/shiboken2.lib
		} else {
			LIBS += $$_PRO_FILE_PWD_/3rdparty/pyside/x64/shiboken2.lib
		}
	}

	LIBS += user32.lib

	# Link against the core library
	LIBS += $$DESTDIR/renderdoc.lib

	# Link against the version library
	LIBS += $$DESTDIR/version.lib

	QMAKE_CXXFLAGS_WARN_ON -= -w34100 
	DEFINES += RENDERDOC_PLATFORM_WIN32

} else {
	isEmpty(CMAKE_DIR) {
		error("When run from outside CMake, please set the Build Environment Variable CMAKE_DIR to point to your CMake build root. In Qt Creator add CMAKE_DIR=/path/to/renderdoc/build under 'Additional arguments' in the qmake Build Step. If running qmake directly, add CMAKE_DIR=/path/to/renderdoc/build/ to the command line.")
	}

	DESTDIR=$$CMAKE_DIR/bin

	include($$CMAKE_DIR/qrenderdoc/qrenderdoc_cmake.pri)

	# Temp files into .obj
	MOC_DIR = .obj
	UI_DIR = .obj
	RCC_DIR = .obj
	OBJECTS_DIR = .obj

	# Link against the core library
	LIBS += -lrenderdoc
	QMAKE_LFLAGS += '-Wl,-rpath,\'\$$ORIGIN\',-rpath,\'\$$ORIGIN/../lib'$$LIB_SUFFIX'/'$$LIB_SUBFOLDER_TRAIL_SLASH'\''

	# Add the SWIG files that were generated in cmake
	SOURCES += $$CMAKE_DIR/qrenderdoc/renderdoc_python.cxx
	SOURCES += $$CMAKE_DIR/qrenderdoc/qrenderdoc_python.cxx

	CONFIG += warn_off
	CONFIG += c++14
	QMAKE_CFLAGS_WARN_OFF -= -w
	QMAKE_CXXFLAGS_WARN_OFF -= -w

	macx: {
		SOURCES += Code/AppleUtils.mm

		LIBS += -framework Cocoa -framework QuartzCore

		DEFINES += RENDERDOC_PLATFORM_POSIX RENDERDOC_PLATFORM_APPLE
		ICON = $$OSX_ICONFILE

		# add qrc file with qt.conf
		RESOURCES += Resources/qtconf.qrc
		
		librd.files = $$files($$DESTDIR/../lib/librenderdoc.dylib)
		librd.path = Contents/lib
		QMAKE_BUNDLE_DATA += librd

		INFO_PLIST_PATH = $$shell_quote($$DESTDIR/$${TARGET}.app/Contents/Info.plist)
		QTPLUGINS_PATH = $$shell_quote($$DESTDIR/$${TARGET}.app/Contents/qtplugins)
		QMAKE_POST_LINK += ln -sf $$[QT_INSTALL_PLUGINS] $${QTPLUGINS_PATH} ;
		QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/../util/set_plist_version.sh $${RENDERDOC_VERSION}.0 $${INFO_PLIST_PATH}
	} else {
		QT += x11extras
		DEFINES += RENDERDOC_PLATFORM_POSIX RENDERDOC_PLATFORM_LINUX RENDERDOC_WINDOWING_XLIB RENDERDOC_WINDOWING_XCB
		QMAKE_LFLAGS += '-Wl,--no-as-needed -rdynamic'
	}
}

# Add our sources first so Qt Creator adds new files here

SOURCES += Code/qrenderdoc.cpp \
    Code/qprocessinfo.cpp \
    Code/ReplayManager.cpp \
    Code/CaptureContext.cpp \
    Code/ScintillaSyntax.cpp \
    Code/QRDUtils.cpp \
    Code/MiniQtHelper.cpp \
    Code/BufferFormatter.cpp \
    Code/Resources.cpp \
    Code/RGPInterop.cpp \
    Code/pyrenderdoc/PythonContext.cpp \
    Code/Interface/QRDInterface.cpp \
    Code/Interface/Analytics.cpp \
    Code/Interface/ShaderProcessingTool.cpp \
    Code/Interface/PersistantConfig.cpp \
    Code/Interface/RemoteHost.cpp \
    Styles/StyleData.cpp \
    Styles/RDStyle/RDStyle.cpp \
    Styles/RDTweakedNativeStyle/RDTweakedNativeStyle.cpp \
    Windows/Dialogs/AboutDialog.cpp \
    Windows/Dialogs/CrashDialog.cpp \
    Windows/Dialogs/UpdateDialog.cpp \
    Windows/MainWindow.cpp \
    Windows/EventBrowser.cpp \
    Windows/TextureViewer.cpp \
    Windows/ShaderViewer.cpp \
    Windows/ShaderMessageViewer.cpp \
    Windows/DescriptorViewer.cpp \
    Widgets/Extended/RDLineEdit.cpp \
    Widgets/Extended/RDTextEdit.cpp \
    Widgets/Extended/RDLabel.cpp \
    Widgets/Extended/RDMenu.cpp \
    Widgets/Extended/RDHeaderView.cpp \
    Widgets/Extended/RDToolButton.cpp \
    Widgets/Extended/RDDoubleSpinBox.cpp \
    Widgets/Extended/RDListView.cpp \
    Widgets/ComputeDebugSelector.cpp \
    Widgets/CustomPaintWidget.cpp \
    Widgets/ResourcePreview.cpp \
    Widgets/ThumbnailStrip.cpp \
    Widgets/ReplayOptionsSelector.cpp \
    Widgets/TextureGoto.cpp \
    Widgets/RangeHistogram.cpp \
    Widgets/CollapseGroupBox.cpp \
    Windows/Dialogs/TextureSaveDialog.cpp \
    Windows/Dialogs/CaptureDialog.cpp \
    Windows/Dialogs/LiveCapture.cpp \
    Widgets/Extended/RDListWidget.cpp \
    Windows/APIInspector.cpp \
    Windows/PipelineState/PipelineStateViewer.cpp \
    Windows/PipelineState/VulkanPipelineStateViewer.cpp \
    Windows/PipelineState/D3D11PipelineStateViewer.cpp \
    Windows/PipelineState/D3D12PipelineStateViewer.cpp \
    Windows/PipelineState/GLPipelineStateViewer.cpp \
    Widgets/Extended/RDTreeView.cpp \
    Widgets/Extended/RDTreeWidget.cpp \
    Widgets/BufferFormatSpecifier.cpp \
    Windows/BufferViewer.cpp \
    Widgets/Extended/RDTableView.cpp \
    Windows/DebugMessageView.cpp \
    Windows/LogView.cpp \
    Windows/CommentView.cpp \
    Windows/StatisticsViewer.cpp \
    Windows/TimelineBar.cpp \
    Windows/Dialogs/SettingsDialog.cpp \
    Widgets/OrderedListEditor.cpp \
    Widgets/MarkerBreadcrumbs.cpp \
    Widgets/Extended/RDTableWidget.cpp \
    Windows/Dialogs/SuggestRemoteDialog.cpp \
    Windows/Dialogs/VirtualFileDialog.cpp \
    Windows/Dialogs/RemoteManager.cpp \
    Windows/Dialogs/ExtensionManager.cpp \
    Windows/PixelHistoryView.cpp \
    Widgets/PipelineFlowChart.cpp \
    Windows/Dialogs/EnvironmentEditor.cpp \
    Widgets/FindReplace.cpp \
    Widgets/Extended/RDSplitter.cpp \
    Windows/Dialogs/TipsDialog.cpp \
    Windows/Dialogs/ConfigEditor.cpp \
    Windows/PythonShell.cpp \
    Windows/Dialogs/PerformanceCounterSelection.cpp \
    Windows/PerformanceCounterViewer.cpp \
    Windows/ResourceInspector.cpp \
    Windows/Dialogs/AnalyticsConfirmDialog.cpp \
    Windows/Dialogs/AnalyticsPromptDialog.cpp \
    Windows/Dialogs/AxisMappingDialog.cpp
HEADERS += Code/CaptureContext.h \
    Code/qprocessinfo.h \
    Code/ReplayManager.h \
    Code/ScintillaSyntax.h \
    Code/QRDUtils.h \
    Code/MiniQtHelper.h \
    Code/Resources.h \
    Code/RGPInterop.h \
    Code/pyrenderdoc/PythonContext.h \
    Code/pyrenderdoc/pyconversion.h \
    Code/pyrenderdoc/interface_check.h \
    Code/Interface/QRDInterface.h \
    Code/Interface/Analytics.h \
    Code/Interface/PersistantConfig.h \
    Code/Interface/Extensions.h \
    Code/Interface/RemoteHost.h \
    Styles/StyleData.h \
    Styles/RDStyle/RDStyle.h \
    Styles/RDTweakedNativeStyle/RDTweakedNativeStyle.h \
    Windows/Dialogs/AboutDialog.h \
    Windows/Dialogs/CrashDialog.h \
    Windows/Dialogs/UpdateDialog.h \
    Windows/MainWindow.h \
    Windows/EventBrowser.h \
    Windows/TextureViewer.h \
    Windows/ShaderViewer.h \
    Windows/ShaderMessageViewer.h \
    Windows/DescriptorViewer.h \
    Widgets/Extended/RDLineEdit.h \
    Widgets/Extended/RDTextEdit.h \
    Widgets/Extended/RDLabel.h \
    Widgets/Extended/RDMenu.h \
    Widgets/Extended/RDHeaderView.h \
    Widgets/Extended/RDToolButton.h \
    Widgets/Extended/RDDoubleSpinBox.h \
    Widgets/Extended/RDListView.h \
    Widgets/ComputeDebugSelector.h \
    Widgets/CustomPaintWidget.h \
    Widgets/ResourcePreview.h \
    Widgets/ThumbnailStrip.h \
    Widgets/ReplayOptionsSelector.h \
    Widgets/TextureGoto.h \
    Widgets/RangeHistogram.h \
    Widgets/CollapseGroupBox.h \
    Windows/Dialogs/TextureSaveDialog.h \
    Windows/Dialogs/CaptureDialog.h \
    Windows/Dialogs/LiveCapture.h \
    Widgets/Extended/RDListWidget.h \
    Windows/APIInspector.h \
    Windows/PipelineState/PipelineStateViewer.h \
    Windows/PipelineState/VulkanPipelineStateViewer.h \
    Windows/PipelineState/D3D11PipelineStateViewer.h \
    Windows/PipelineState/D3D12PipelineStateViewer.h \
    Windows/PipelineState/GLPipelineStateViewer.h \
    Widgets/Extended/RDTreeView.h \
    Widgets/Extended/RDTreeWidget.h \
    Widgets/BufferFormatSpecifier.h \
    Windows/BufferViewer.h \
    Widgets/Extended/RDTableView.h \
    Windows/DebugMessageView.h \
    Windows/LogView.h \
    Windows/CommentView.h \
    Windows/StatisticsViewer.h \
    Windows/TimelineBar.h \
    Windows/Dialogs/SettingsDialog.h \
    Widgets/OrderedListEditor.h \
    Widgets/MarkerBreadcrumbs.h \
    Widgets/Extended/RDTableWidget.h \
    Windows/Dialogs/SuggestRemoteDialog.h \
    Windows/Dialogs/VirtualFileDialog.h \
    Windows/Dialogs/RemoteManager.h \
    Windows/Dialogs/ExtensionManager.h \
    Windows/PixelHistoryView.h \
    Widgets/PipelineFlowChart.h \
    Windows/Dialogs/EnvironmentEditor.h \
    Widgets/FindReplace.h \
    Widgets/Extended/RDSplitter.h \
    Windows/Dialogs/TipsDialog.h \
    Windows/Dialogs/ConfigEditor.h \
    Windows/PythonShell.h \
    Windows/Dialogs/PerformanceCounterSelection.h \
    Windows/PerformanceCounterViewer.h \
    Windows/ResourceInspector.h \
    Windows/Dialogs/AnalyticsConfirmDialog.h \
    Windows/Dialogs/AnalyticsPromptDialog.h \
    Windows/Dialogs/AxisMappingDialog.h
FORMS    += Windows/Dialogs/AboutDialog.ui \
    Windows/Dialogs/CrashDialog.ui \
    Windows/Dialogs/UpdateDialog.ui \
    Windows/MainWindow.ui \
    Windows/EventBrowser.ui \
    Windows/TextureViewer.ui \
    Widgets/ResourcePreview.ui \
    Widgets/ThumbnailStrip.ui \
    Widgets/ReplayOptionsSelector.ui \
    Windows/Dialogs/TextureSaveDialog.ui \
    Windows/Dialogs/CaptureDialog.ui \
    Windows/Dialogs/LiveCapture.ui \
    Windows/APIInspector.ui \
    Windows/PipelineState/PipelineStateViewer.ui \
    Windows/PipelineState/VulkanPipelineStateViewer.ui \
    Windows/PipelineState/D3D11PipelineStateViewer.ui \
    Windows/PipelineState/D3D12PipelineStateViewer.ui \
    Windows/PipelineState/GLPipelineStateViewer.ui \
    Widgets/BufferFormatSpecifier.ui \
    Widgets/ComputeDebugSelector.ui \
    Windows/BufferViewer.ui \
    Windows/ShaderViewer.ui \
    Windows/ShaderMessageViewer.ui \
    Windows/DescriptorViewer.ui \
    Windows/DebugMessageView.ui \
    Windows/LogView.ui \
    Windows/CommentView.ui \
    Windows/StatisticsViewer.ui \
    Windows/Dialogs/SettingsDialog.ui \
    Windows/Dialogs/SuggestRemoteDialog.ui \
    Windows/Dialogs/VirtualFileDialog.ui \
    Windows/Dialogs/RemoteManager.ui \
    Windows/Dialogs/ExtensionManager.ui \
    Windows/PixelHistoryView.ui \
    Windows/Dialogs/EnvironmentEditor.ui \
    Widgets/FindReplace.ui \
    Windows/Dialogs/TipsDialog.ui \
    Windows/Dialogs/ConfigEditor.ui \
    Windows/PythonShell.ui \
    Windows/Dialogs/PerformanceCounterSelection.ui \
    Windows/PerformanceCounterViewer.ui \
    Windows/ResourceInspector.ui \
    Windows/Dialogs/AnalyticsConfirmDialog.ui \
    Windows/Dialogs/AnalyticsPromptDialog.ui \
    Windows/Dialogs/AxisMappingDialog.ui

RESOURCES += Resources/resources.qrc

# Add ToolWindowManager

SOURCES += 3rdparty/toolwindowmanager/ToolWindowManager.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerSplitter.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerTabBar.cpp \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.cpp

HEADERS += 3rdparty/toolwindowmanager/ToolWindowManager.h \
    3rdparty/toolwindowmanager/ToolWindowManagerArea.h \
    3rdparty/toolwindowmanager/ToolWindowManagerSplitter.h \
    3rdparty/toolwindowmanager/ToolWindowManagerTabBar.h \
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.h

# Add FlowLayout

SOURCES += 3rdparty/flowlayout/FlowLayout.cpp
HEADERS += 3rdparty/flowlayout/FlowLayout.h

# Add pythoncapi-compat

HEADERS += 3rdparty/pythoncapi_compat.h

# Add Scintilla last as it has extra search paths

# Needed for building
DEFINES += SCINTILLA_QT=1 MAKING_LIBRARY=1 SCI_LEXER=1
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/src
INCLUDEPATH += $$_PRO_FILE_PWD_/3rdparty/scintilla/lexlib

SOURCES += $$_PRO_FILE_PWD_/3rdparty/scintilla/lexlib/*.cxx \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/lexers/*.cxx \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/src/*.cxx \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEdit/*.cpp \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEditBase/*.cpp

HEADERS += $$_PRO_FILE_PWD_/3rdparty/scintilla/lexlib/*.h \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/src/*.h \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEdit/*.h \
    $$_PRO_FILE_PWD_/3rdparty/scintilla/qt/ScintillaEditBase/*.h

