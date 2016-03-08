set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)

set(sources
    Code/Core.cpp
    Code/Core.h
    Code/main.cpp
    Code/RenderManager.cpp
    Code/RenderManager.h
    Widgets/CustomPaintWidget.cpp
    Widgets/CustomPaintWidget.h
    Widgets/LineEditFocusWidget.cpp
    Widgets/LineEditFocusWidget.h
    Windows/EventBrowser.cpp
    Windows/EventBrowser.h
    Windows/MainWindow.cpp
    Windows/MainWindow.h
    Windows/TextureViewer.cpp
    Windows/TextureViewer.h
    3rdparty/flowlayout/FlowLayout.cpp
    3rdparty/flowlayout/FlowLayout.h
    3rdparty/toolwindowmanager/ToolWindowManagerArea.cpp
    3rdparty/toolwindowmanager/ToolWindowManagerArea.h
    3rdparty/toolwindowmanager/ToolWindowManager.cpp
    3rdparty/toolwindowmanager/ToolWindowManager.h
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.cpp
    3rdparty/toolwindowmanager/ToolWindowManagerWrapper.h)

set(forms
    Windows/MainWindow.ui
    Windows/EventBrowser.ui
    Windows/TextureViewer.ui)

set(resources resources.qrc)

set(includes
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api/replay
    PRIVATE 3rdparty/toolwindowmanager
    PRIVATE 3rdparty/flowlayout)

set(libraries
    PRIVATE renderdoc
    PRIVATE Qt5::Widgets)

if(UNIX)
    find_package(Qt5X11Extras)
    list(APPEND libraries PRIVATE Qt5::X11Extras)
endif()

qt5_wrap_ui(form_sources ${forms})
qt5_add_resources(resource_sources ${resources})
list(APPEND sources ${form_sources} ${resource_sources})

add_executable(qrenderdoc ${sources})
target_include_directories(qrenderdoc ${includes})
target_link_libraries(qrenderdoc ${libraries})
