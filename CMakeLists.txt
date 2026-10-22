cmake_minimum_required(VERSION 3.21)

project(TalkBOX
    VERSION 1.1.1
    DESCRIPTION "TalkBOX - local Windows snippet library (Qt 6)"
    LANGUAGES CXX
)

# Four-part version for the Windows resource block (1.1.1.0).
set(TALKBOX_FILE_VERSION ${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0)
set(TALKBOX_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Concurrent
    Gui
    Qml
    Quick
    QuickControls2
    Sql
    Svg
)

qt_standard_project_setup(REQUIRES 6.5)

# QML files live in subdirectories (components/, dialogs/); the NEW policy
# generates qmldir files for those directories automatically.
qt_policy(SET QTP0004 NEW)

# ---------------------------------------------------------------------------
# Core library: data access, models and services (no UI).
# ---------------------------------------------------------------------------
add_library(talkbox_core STATIC
    src/app/AppPaths.h
    src/app/AppPaths.cpp
    src/app/Logging.h
    src/app/Logging.cpp
    src/data/Category.h
    src/data/CategoryRepository.h
    src/data/CategoryRepository.cpp
    src/data/DatabaseManager.h
    src/data/DatabaseManager.cpp
    src/data/Migrations.h
    src/data/Migrations.cpp
    src/data/Snippet.h
    src/data/SnippetRepository.h
    src/data/SnippetRepository.cpp
    src/models/CategoryListModel.h
    src/models/CategoryListModel.cpp
    src/models/SnippetListModel.h
    src/models/SnippetListModel.cpp
    src/platform/WindowChrome.h
    src/platform/WindowChrome.cpp
    src/services/AppInfo.h
    src/services/AppInfo.cpp
    src/services/AttachmentService.h
    src/services/AttachmentService.cpp
    src/services/BackupService.h
    src/services/BackupService.cpp
    src/services/CategoryService.h
    src/services/CategoryService.cpp
    src/services/ClipboardWriter.h
    src/services/SnippetService.h
    src/services/SnippetService.cpp
    src/services/SystemService.h
    src/services/SystemService.cpp
)

target_include_directories(talkbox_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(talkbox_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
        Qt6::Concurrent
    PRIVATE
        # QZipWriter/QZipReader live in QtCore's private headers; the
        # private target only adds the include path (no extra runtime
        # dependency).
        Qt6::CorePrivate
)

if(WIN32)
    # WindowChrome talks to DWM directly (shadow / border / corner attributes).
    target_link_libraries(talkbox_core PRIVATE dwmapi)
endif()

# ---------------------------------------------------------------------------
# Application executable + QML module.
# ---------------------------------------------------------------------------
qt_add_executable(TalkBOX
    src/main.cpp
    assets/talkbox.rc
)

# The resource compiler gets the icon next to the .rc file, and the version
# block is generated from the CMake project version.
set_source_files_properties(assets/talkbox.rc PROPERTIES
    COMPILE_DEFINITIONS
    "TALKBOX_FILE_VERSION=${TALKBOX_FILE_VERSION};TALKBOX_VERSION_STRING=\"${TALKBOX_VERSION_STRING}\""
)

target_link_libraries(TalkBOX PRIVATE
    talkbox_core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
)

target_compile_definitions(TalkBOX PRIVATE TALKBOX_VERSION="${TALKBOX_VERSION_STRING}")

set_target_properties(TalkBOX PROPERTIES
    WIN32_EXECUTABLE TRUE
)

set(TALKBOX_QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/StartupError.qml
    qml/components/AppButton.qml
    qml/components/CaptionButton.qml
    qml/components/CardMenuItem.qml
    qml/components/CategoryChip.qml
    qml/components/CategoryPicker.qml
    qml/components/DialogScrim.qml
    qml/components/EmptyState.qml
    qml/components/IconButton.qml
    qml/components/SearchField.qml
    qml/components/SnippetCard.qml
    qml/components/TitleBar.qml
    qml/components/ToastBar.qml
    qml/components/ToggleSwitch.qml
    qml/components/TypeFilterButton.qml
    qml/dialogs/ChangeCategoryDialog.qml
    qml/dialogs/ConfirmDialog.qml
    qml/dialogs/DeleteConfirmDialog.qml
    qml/dialogs/EditTextDialog.qml
    qml/dialogs/ImagePreviewDialog.qml
    qml/dialogs/NewSnippetDialog.qml
    qml/dialogs/RenameDialog.qml
    qml/dialogs/SettingsDialog.qml
    qml/dialogs/TextInputDialog.qml
)

set(TALKBOX_QML_RESOURCES
    qml/icons/app.svg
    qml/icons/close.svg
    qml/icons/grid-blue.svg
    qml/icons/grid-gray.svg
    qml/icons/image-blue.svg
    qml/icons/image-gray.svg
    qml/icons/more.svg
    qml/icons/pdf-blue.svg
    qml/icons/pdf-gray.svg
    qml/icons/pin-blue.svg
    qml/icons/pin-gray.svg
    qml/icons/plus-blue.svg
    qml/icons/plus.svg
    qml/icons/search.svg
    qml/icons/settings.svg
    qml/icons/text-lines-gray.svg
    qml/icons/window-close-white.svg
    qml/icons/window-close.svg
    qml/icons/window-maximize.svg
    qml/icons/window-minimize.svg
    qml/icons/window-restore.svg
)

# The QML files live in a "qml/" subdirectory; QT_RESOURCE_ALIAS keeps the
# module layout flat (Main.qml at the module root) regardless of how the
# build system maps source paths into the resource system.
foreach(qml_file IN LISTS TALKBOX_QML_FILES TALKBOX_QML_RESOURCES)
    string(REGEX REPLACE "^qml/" "" resource_alias "${qml_file}")
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${resource_alias}")
endforeach()

# Theme.qml is a QML singleton (pragma Singleton). Qt 6.8 does not always
# infer this automatically, so mark it explicitly.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(TalkBOX
    URI TalkBox
    VERSION 1.0
    QML_FILES
        ${TALKBOX_QML_FILES}
    RESOURCES
        ${TALKBOX_QML_RESOURCES}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
