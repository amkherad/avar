cmake_minimum_required(VERSION 3.16)

project(avar-gui-qt
    VERSION 0.1.0
    DESCRIPTION "Native Qt GUI for Avar (desktop and WebAssembly)"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(AvarGuiQtOptions)
include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network Test)
find_package(Qt6 ${Qt6_VERSION} QUIET COMPONENTS WebSockets Svg)

set(AVAR_GUI_QT_SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/core/CrashHandler.cpp
    src/core/GuiLog.cpp
    src/core/Hosting.cpp
    src/config/LayoutPreferences.cpp
    src/session/SessionManager.cpp
    src/api/JsonRpc.cpp
    src/api/DaemonClient.cpp
    src/api/DaemonClientMethods.cpp
    src/theme/ThemeTokens.cpp
    src/theme/ThemeManager.cpp
    src/theme/StylesheetBuilder.cpp
    src/sync/SyncCoordinator.cpp
    src/config/AppSettings.cpp
    src/i18n/Translator.cpp
    src/extension/ExtensionBridgeClient.cpp
    src/ui/MainWindow.cpp
    src/ui/AppShell.cpp
    src/ui/DesktopShellWindow.cpp
    src/ui/AvarWindow.cpp
    src/ui/widgets/AvarButton.cpp
    src/ui/widgets/SettingsSidebarNav.cpp
    src/ui/widgets/ResizeHandle.cpp
    src/ui/widgets/SessionSelector.cpp
    src/ui/widgets/QueuePanelWidget.cpp
    src/ui/widgets/FooterBar.cpp
    src/ui/widgets/ConsoleDock.cpp
    src/ui/widgets/DownloadDetailPanelWidget.cpp
    src/ui/widgets/DownloadGridView.cpp
    src/ui/widgets/WindowTitleBar.cpp
    src/ui/pages/AddDownloadPopupPage.cpp
    src/ui/pages/BatchAddDownloadsPopupPage.cpp
    src/ui/pages/ConfirmDialogPopupPage.cpp
    src/ui/pages/DownloadDetailPopupPage.cpp
    src/ui/pages/DashboardPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/HelpPage.cpp
    src/models/DownloadTableModel.cpp
    src/models/DownloadFilterProxyModel.cpp
    src/models/QueueListModel.cpp
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_add_executable(avar-gui-qt
    ${AVAR_GUI_QT_SOURCES}
    resources/avar_gui.qrc
)

target_include_directories(avar-gui-qt
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(avar-gui-qt
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)

if(TARGET Qt6::Svg)
    target_link_libraries(avar-gui-qt PRIVATE Qt6::Svg)
    target_compile_definitions(avar-gui-qt PRIVATE AVAR_GUI_HAS_QT_SVG=1)
endif()

if(TARGET Qt6::WebSockets)
    target_link_libraries(avar-gui-qt PRIVATE Qt6::WebSockets)
    target_compile_definitions(avar-gui-qt PRIVATE AVAR_GUI_HAS_WEBSOCKETS=1)
endif()

avar_gui_qt_apply_target_options(avar-gui-qt)

if(AVAR_GUI_QT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS avar-gui-qt RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
