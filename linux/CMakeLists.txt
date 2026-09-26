cmake_minimum_required(VERSION 3.16)

# Version is read from the repository's VERSION file ("v.0.0.67").
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" CRICKET_VERSION_RAW)
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" CRICKET_VERSION "${CRICKET_VERSION_RAW}")

project(cricket VERSION ${CRICKET_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(CRICKET_WITH_ASPELL "Enable aspell spell checking" ON)
option(CRICKET_WITH_DBUS "Enable desktop notifications via D-Bus" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network)
if(CRICKET_WITH_DBUS)
    find_package(Qt6 COMPONENTS DBus)
endif()

add_executable(cricket
    src/main.cpp
    src/config.cpp
    src/ircmessage.cpp
    src/ircconnection.cpp
    src/ircformat.cpp
    src/services.cpp
    src/inputedit.cpp
    src/chatview.cpp
    src/dialogs.cpp
    src/mainwindow.cpp
    resources/resources.qrc
)

target_compile_definitions(cricket PRIVATE CRICKET_VERSION="${CRICKET_VERSION}")
target_link_libraries(cricket PRIVATE Qt6::Widgets Qt6::Network)

if(CRICKET_WITH_DBUS AND Qt6DBus_FOUND)
    target_link_libraries(cricket PRIVATE Qt6::DBus)
    target_compile_definitions(cricket PRIVATE CRICKET_HAVE_DBUS)
    message(STATUS "Desktop notifications: enabled (D-Bus)")
else()
    message(STATUS "Desktop notifications: disabled")
endif()

if(CRICKET_WITH_ASPELL)
    find_path(ASPELL_INCLUDE_DIR aspell.h)
    find_library(ASPELL_LIBRARY aspell)
    if(ASPELL_INCLUDE_DIR AND ASPELL_LIBRARY)
        target_include_directories(cricket PRIVATE ${ASPELL_INCLUDE_DIR})
        target_link_libraries(cricket PRIVATE ${ASPELL_LIBRARY})
        target_compile_definitions(cricket PRIVATE CRICKET_HAVE_ASPELL)
        message(STATUS "Spell checking: enabled (aspell)")
    else()
        message(STATUS "Spell checking: disabled (aspell not found)")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cricket PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

include(GNUInstallDirs)
install(TARGETS cricket RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/cricket.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/cricket.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
