cmake_minimum_required(VERSION 3.22)
project(clipvault VERSION 0.1.0 LANGUAGES CXX)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)

find_package(SQLite3 REQUIRED)
if (NOT TARGET SQLite3::SQLite3 AND TARGET SQLite::SQLite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif ()

find_package(X11)
set(CLIPVAULT_HAVE_X11 0)
if (X11_FOUND)
    find_library(XTST_LIB Xtst)
    if (XTST_LIB)
        set(CLIPVAULT_HAVE_X11 1)
    endif ()
endif ()

add_executable(clipvault
        src/main.cpp
        assets/resources.qrc
        src/app.cpp
        src/config/settings.cpp
        src/config/autostart.cpp
        src/history/history_db.cpp
        src/history/clipboard_watcher.cpp
        src/ui/history_model.cpp
        src/ui/history_popup.cpp
        src/ui/tray.cpp
        src/integration/hotkey_manager.cpp
        src/integration/portal_request.cpp
        src/integration/portal_global_shortcuts.cpp
        src/integration/portal_paster.cpp
        src/integration/x11_hotkey.cpp
        src/integration/x11_paster.cpp
        src/util/paths.cpp
        src/util/crypto.cpp
        src/util/qt_helpers.cpp
        src/util/hotkey_parse.cpp
)

target_include_directories(clipvault PRIVATE src)

target_link_libraries(clipvault PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus
        SQLite3::SQLite3
)

if (CLIPVAULT_HAVE_X11)
    target_compile_definitions(clipvault PRIVATE CLIPVAULT_HAVE_X11=1)
    target_include_directories(clipvault PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(clipvault PRIVATE ${X11_LIBRARIES} ${XTST_LIB})
else ()
    target_compile_definitions(clipvault PRIVATE CLIPVAULT_HAVE_X11=0)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(clipvault PRIVATE -Wall -Wextra -Wpedantic -Wno-psabi)
endif ()

target_compile_options(clipvault PRIVATE
        $<$<CONFIG:Debug>:-O0;-g3>
)

target_compile_options(clipvault PRIVATE
        $<$<CONFIG:Release>:-O3>
)

target_compile_options(clipvault PRIVATE
        $<$<CONFIG:RelWithDebInfo>:-O2;-g>
)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok OUTPUT ipo_err)
if (ipo_ok)
    set_property(TARGET clipvault PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif ()

option(CLIPVAULT_SANITIZERS "Enable Address/Undefined sanitizers (Debug only)" OFF)
if (CLIPVAULT_SANITIZERS AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
    target_compile_options(clipvault PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined;-fno-omit-frame-pointer>
    )
    target_link_options(clipvault PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif ()

install(TARGETS clipvault RUNTIME DESTINATION bin)
