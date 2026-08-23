qt_add_executable(anicloud_hls_tests
  test_hlstools.cpp
  ../src/HlsTools.cpp
)
target_include_directories(anicloud_hls_tests PRIVATE ../src)
target_compile_definitions(anicloud_hls_tests PRIVATE ANICLOUD_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(anicloud_hls_tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME hls_tools COMMAND anicloud_hls_tests)

qt_add_executable(anicloud_subtitle_tests
  test_subtitletools.cpp
  ../src/SubtitleTools.cpp
)
target_include_directories(anicloud_subtitle_tests PRIVATE ../src)
target_link_libraries(anicloud_subtitle_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME native_subtitle_cues COMMAND anicloud_subtitle_tests)

qt_add_executable(anicloud_hls_gateway_tests
  test_hlsgateway.cpp
  ../src/HlsGateway.cpp
  ../src/HlsTools.cpp
)
target_include_directories(anicloud_hls_gateway_tests PRIVATE ../src)
target_link_libraries(anicloud_hls_gateway_tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME hls_loopback_gateway COMMAND anicloud_hls_gateway_tests)

qt_add_executable(anicloud_update_tests
  test_update.cpp
  ../src/UpdateService.cpp
)
target_include_directories(anicloud_update_tests PRIVATE ../src "${CMAKE_CURRENT_BINARY_DIR}/../generated")
target_link_libraries(anicloud_update_tests PRIVATE Qt6::Core Qt6::Network Qt6::Widgets Qt6::Test unofficial-sodium::sodium)
if(WIN32)
  target_link_libraries(anicloud_update_tests PRIVATE shell32)
endif()
add_test(NAME signed_updates COMMAND anicloud_update_tests)

qt_add_executable(anicloud_qml_policy_tests test_qmlpolicy.cpp)
target_compile_definitions(anicloud_qml_policy_tests PRIVATE ANICLOUD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(anicloud_qml_policy_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME qml_routes_and_accessibility COMMAND anicloud_qml_policy_tests)

qt_add_executable(anicloud_persistence_tests
  test_persistence.cpp
  ../src/Database.cpp
  ../src/SecureStore.cpp
)
target_include_directories(anicloud_persistence_tests PRIVATE ../src)
target_link_libraries(anicloud_persistence_tests PRIVATE
  Qt6::Core Qt6::Sql Qt6::Test unofficial-sodium::sodium ${ANICLOUD_QTKEYCHAIN_TARGET}
)
add_test(NAME sqlite_encryption_and_owner_isolation COMMAND anicloud_persistence_tests)

qt_add_executable(anicloud_api_mapping_tests
  test_api_mapping.cpp
  ../src/ApiClient.cpp
  ../src/Database.cpp
  ../src/SecureStore.cpp
)
target_include_directories(anicloud_api_mapping_tests PRIVATE ../src "${CMAKE_CURRENT_BINARY_DIR}/../generated")
target_link_libraries(anicloud_api_mapping_tests PRIVATE
  Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Test unofficial-sodium::sodium ${ANICLOUD_QTKEYCHAIN_TARGET}
)
add_test(NAME provider_dto_mapping COMMAND anicloud_api_mapping_tests)

qt_add_executable(anicloud_quick_tests quicktest_main.cpp)
target_compile_definitions(anicloud_quick_tests PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
target_link_libraries(anicloud_quick_tests PRIVATE Qt6::Quick Qt6::QuickTest)
add_test(NAME qt_quick_adaptive_shell COMMAND anicloud_quick_tests)
set_tests_properties(qt_quick_adaptive_shell PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
