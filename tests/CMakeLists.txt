set(TALKBOX_TESTS
    tst_database
    tst_snippet_repository
    tst_snippet_service
    tst_attachment_service
    tst_category_repository
    tst_backup_service
    tst_system_clipboard
    tst_persistence
)

find_package(Qt6 REQUIRED COMPONENTS Test)

foreach(test_name IN LISTS TALKBOX_TESTS)
    add_executable(${test_name}
        unit/${test_name}.cpp
        unit/TestUtils.h
    )
    target_link_libraries(${test_name} PRIVATE talkbox_core Qt6::Test)
    # "-o -,txt" forces Qt Test results to stdout so ctest can capture
    # them in headless runs.
    add_test(NAME ${test_name} COMMAND ${test_name} -o -,txt)
    # The test executables need the Qt runtime DLLs on PATH. $<SEMICOLON>
    # keeps the Windows PATH separator out of CMake's list parsing.
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}$<SEMICOLON>$ENV{PATH}"
    )
endforeach()

# The backup test inspects the .talkboxbackup container directly.
target_link_libraries(tst_backup_service PRIVATE Qt6::CorePrivate)

# ---------------------------------------------------------------------------
# UI integration test: loads the real QML module (from the build tree) and
# drives it with synthetic input events, independent of the desktop session.
# ---------------------------------------------------------------------------
add_executable(tst_ui_flows integration/tst_ui_flows.cpp)
add_dependencies(tst_ui_flows TalkBOX) # the module qmldir must exist
target_link_libraries(tst_ui_flows PRIVATE
    talkbox_core
    Qt6::Test
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)
target_compile_definitions(tst_ui_flows PRIVATE
    TALKBOX_QML_IMPORT_DIR="$<TARGET_FILE_DIR:TalkBOX>/.."
)
add_test(NAME tst_ui_flows COMMAND tst_ui_flows -o -,txt)
set_tests_properties(tst_ui_flows PROPERTIES
    ENVIRONMENT "PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}$<SEMICOLON>$ENV{PATH}"
)
