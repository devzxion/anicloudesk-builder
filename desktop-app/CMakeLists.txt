cmake_minimum_required(VERSION 3.28)

project(AniCloudDesktop VERSION 4.0.4 LANGUAGES CXX)

if(WIN32)
  enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(FetchContent)
include(GNUInstallDirs)

find_package(Qt6 6.10 REQUIRED COMPONENTS
  Core Gui Qml Quick QuickControls2 Network Sql Multimedia Concurrent Widgets Test QuickTest
)
find_package(unofficial-sodium CONFIG REQUIRED)

FetchContent_Declare(
  extra_cmake_modules
  GIT_REPOSITORY https://invent.kde.org/frameworks/extra-cmake-modules.git
  GIT_TAG v6.29.0
  GIT_SHALLOW TRUE
)
FetchContent_GetProperties(extra_cmake_modules)
if(NOT extra_cmake_modules_POPULATED)
  FetchContent_Populate(extra_cmake_modules)
endif()
list(APPEND CMAKE_MODULE_PATH "${extra_cmake_modules_SOURCE_DIR}/modules")

set(BUILD_WITH_QT5 OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  qtkeychain
  GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
  GIT_TAG 0.17.0
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(qtkeychain)

if(TARGET qt6keychain)
  set(ANICLOUD_QTKEYCHAIN_TARGET qt6keychain)
elseif(TARGET Qt6Keychain::Qt6Keychain)
  set(ANICLOUD_QTKEYCHAIN_TARGET Qt6Keychain::Qt6Keychain)
else()
  message(FATAL_ERROR "QtKeychain's Qt 6 CMake target was not created")
endif()

qt_standard_project_setup(REQUIRES 6.10)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(ANICLOUD_ACCOUNT_API_BASE_URL "https://api.anicloud.ink/public/api/v1" CACHE STRING "AniCloud account API")
set(ANICLOUD_RELEASE_REPOSITORY "devz-on/anicloudesk" CACHE STRING "GitHub release repository")
set(ANICLOUD_UPDATE_PUBLIC_KEY_HEX "" CACHE STRING "Ed25519 public key used to verify desktop release manifests")
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND ANICLOUD_UPDATE_PUBLIC_KEY_HEX STREQUAL "")
  message(FATAL_ERROR "Release builds require ANICLOUD_UPDATE_PUBLIC_KEY_HEX")
endif()

configure_file(src/BuildConfig.h.in generated/BuildConfig.h @ONLY)

qt_add_executable(AniCloud WIN32 MACOSX_BUNDLE
  src/main.cpp
  src/Models.h
  src/SecureStore.h
  src/SecureStore.cpp
  src/Database.h
  src/Database.cpp
  src/ApiClient.h
  src/ApiClient.cpp
  src/HlsTools.h
  src/HlsTools.cpp
  src/HlsGateway.h
  src/HlsGateway.cpp
  src/PlayerController.h
  src/PlayerController.cpp
  src/DownloadManager.h
  src/DownloadManager.cpp
  src/UpdateService.h
  src/UpdateService.cpp
  src/AppRuntime.h
  src/AppRuntime.cpp
)

qt_add_qml_module(AniCloud
  URI AniCloud
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/components/AnimeCard.qml
    qml/components/AppButton.qml
    qml/components/EmptyState.qml
    qml/components/LoadingSkeleton.qml
    qml/components/NavigationRail.qml
    qml/components/PlayerIconButton.qml
    qml/components/PosterRail.qml
    qml/pages/HomePage.qml
    qml/pages/DiscoverPage.qml
    qml/pages/DetailsPage.qml
    qml/pages/DownloadsPage.qml
    qml/pages/LibraryPage.qml
    qml/pages/ProfilePage.qml
    qml/pages/AuthPage.qml
    qml/pages/BroadcastsPage.qml
    qml/pages/PlayerPage.qml
  RESOURCES
    resources/icon.png
    resources/icons/home.svg
    resources/icons/discover.svg
    resources/icons/download.svg
    resources/icons/library.svg
    resources/icons/profile.svg
    resources/icons/notification.svg
    resources/icons/player-play.svg
    resources/icons/player-pause.svg
    resources/icons/player-back10.svg
    resources/icons/player-forward10.svg
    resources/icons/player-volume.svg
    resources/icons/player-muted.svg
    resources/icons/player-captions.svg
    resources/icons/player-settings.svg
    resources/icons/player-fullscreen.svg
    resources/icons/player-next.svg
    resources/icons/player-back.svg
)

target_include_directories(AniCloud PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(AniCloud PRIVATE
  QT_NO_CAST_FROM_ASCII
  QT_NO_CAST_TO_ASCII
)

target_link_libraries(AniCloud PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Network
  Qt6::Sql
  Qt6::Multimedia
  Qt6::Concurrent
  Qt6::Widgets
  unofficial-sodium::sodium
  ${ANICLOUD_QTKEYCHAIN_TARGET}
)

set_target_properties(AniCloud PROPERTIES
  OUTPUT_NAME "AniCloud"
  MACOSX_BUNDLE_BUNDLE_NAME "AniCloud"
  MACOSX_BUNDLE_GUI_IDENTIFIER "ink.anicloud.desktop"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  MACOSX_BUNDLE_COPYRIGHT "Copyright AniCloud"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
  MACOSX_BUNDLE_ICON_FILE "icon.png"
  WIN32_EXECUTABLE TRUE
)

if(WIN32)
  target_sources(AniCloud PRIVATE resources/windows.rc)
  add_executable(AniCloudLauncher WIN32 src/windows_launcher.cpp resources/windows.rc)
  set_target_properties(AniCloudLauncher PROPERTIES
    OUTPUT_NAME "AniCloud"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/launcher"
  )
  target_link_libraries(AniCloudLauncher PRIVATE shell32 user32)
  install(TARGETS AniCloudLauncher RUNTIME DESTINATION .)
elseif(APPLE)
  set_source_files_properties(resources/icon.png PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  target_sources(AniCloud PRIVATE resources/icon.png)
else()
  set_target_properties(AniCloud PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

install(TARGETS AniCloud
  BUNDLE DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(WIN32)
  if(NOT COMMAND x_vcpkg_install_local_dependencies)
    message(FATAL_ERROR "Windows packaging requires vcpkg install-time runtime dependency deployment")
  endif()
  x_vcpkg_install_local_dependencies(
    TARGETS AniCloud
    DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()
install(FILES THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/AniCloud)
if(UNIX AND NOT APPLE)
  install(FILES resources/linux/ink.anicloud.desktop.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES resources/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME ink.anicloud.desktop.png)
endif()

qt_generate_deploy_qml_app_script(
  TARGET AniCloud
  OUTPUT_SCRIPT ANICLOUD_QT_DEPLOY_SCRIPT
  NO_UNSUPPORTED_PLATFORM_ERROR
  EXCLUDE_PLUGINS
    qsqlibase
    qsqlmimer
    qsqlmysql
    qsqloci
    qsqlodbc
    qsqlpsql
)
install(SCRIPT ${ANICLOUD_QT_DEPLOY_SCRIPT})

set(CPACK_PACKAGE_NAME "AniCloud")
set(CPACK_PACKAGE_VENDOR "AniCloud")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AniCloud native desktop streaming client")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "AniCloud")
set(CPACK_PACKAGE_CONTACT "support@anicloud.ink")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md")
if(WIN32)
  set(CPACK_GENERATOR "NSIS;ZIP")
  set(CPACK_NSIS_DISPLAY_NAME "AniCloud")
  set(CPACK_NSIS_PACKAGE_NAME "AniCloud")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
elseif(UNIX)
  set(CPACK_GENERATOR "DEB;TGZ")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_SECTION "video")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  configure_file(packaging/linux/control.in ${CMAKE_CURRENT_BINARY_DIR}/debian-control @ONLY)
endif()
include(CPack)

enable_testing()
add_subdirectory(tests)
