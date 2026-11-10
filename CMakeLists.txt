cmake_minimum_required(VERSION 3.16)
project(blackout VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

set(PROJECT_SOURCES
    src/main.cpp
    src/backend.h
    src/backend.cpp
    src/theme.h
    src/theme.cpp
    src/translator.h
    src/translator.cpp
    src/resources.qrc
)

if(WIN32)
    list(APPEND PROJECT_SOURCES pkgbuild/blackout.rc)
    add_executable(blackout WIN32 ${PROJECT_SOURCES})
    target_link_libraries(blackout PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        user32
        shell32
    )
else()
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    find_library(LAYERSHELL_LIB LayerShellQtInterface)
    add_executable(blackout ${PROJECT_SOURCES})
    target_link_libraries(blackout PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::DBus
    )
    if(LAYERSHELL_LIB)
        target_link_libraries(blackout PRIVATE ${LAYERSHELL_LIB})
    endif()
endif()

install(TARGETS blackout RUNTIME DESTINATION bin)
