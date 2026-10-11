cmake_minimum_required(VERSION 3.24)
project(voxprint-lucent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

qt_standard_project_setup()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../BUILD.json" LUCENT_BUILD_JSON)
string(JSON LUCENT_BUILD_OFFSET GET "${LUCENT_BUILD_JSON}" offset)
string(JSON LUCENT_CODENAME GET "${LUCENT_BUILD_JSON}" codename)
if(NOT DEFINED LUCENT_CI_RUN_NUMBER)
    set(LUCENT_CI_RUN_NUMBER "$ENV{GITHUB_RUN_NUMBER}")
endif()
if(LUCENT_CI_RUN_NUMBER STREQUAL "")
    set(LUCENT_CI_RUN_NUMBER 0)
endif()
math(EXPR LUCENT_BUILD_NUMBER "${LUCENT_CI_RUN_NUMBER} + ${LUCENT_BUILD_OFFSET}")
set(LUCENT_VERSION "0.1.0-rc4")
configure_file(src/lucent_build.h.in "${CMAKE_CURRENT_BINARY_DIR}/lucent_build.h" @ONLY)

qt_add_executable(voxprint-lucent
    src/main.cpp
    src/dry_run.cpp
    src/decode.cpp
    src/theme.cpp
    src/viewer.cpp
    src/placement.cpp
    src/natural.cpp
    src/catalog.cpp
    src/imagecache.cpp
    src/phrases.cpp
    src/hotkeys.cpp
    src/rhi_view.cpp
    src/browser.cpp
    src/icons.cpp
    src/gallery.cpp
    src/splash.cpp
    src/chrome.cpp
    src/selftest.cpp
    src/display_hdr.cpp
)

target_compile_definitions(voxprint-lucent PRIVATE LUCENT_VERSION="${LUCENT_VERSION}")

target_link_libraries(voxprint-lucent PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent)
target_include_directories(voxprint-lucent PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}")

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.7.0)
    # <rhi/qrhi.h> ships with Qt6::GuiPrivate. Debian 13's qt6-base-dev still
    # defines that imported target, but its versioned include directory does
    # not exist until qt6-base-private-dev is installed. Linking the broken
    # target fails at generate time, so use it only when every include
    # directory is present. Otherwise add the directory that contains the header.
    find_package(Qt6 QUIET COMPONENTS GuiPrivate)
    set(_lucent_private_target_ok FALSE)
    if(TARGET Qt6::GuiPrivate)
        get_target_property(_lucent_priv_dirs Qt6::GuiPrivate INTERFACE_INCLUDE_DIRECTORIES)
        if(_lucent_priv_dirs)
            set(_lucent_private_target_ok TRUE)
            set(_lucent_saw_private_include FALSE)
            foreach(_dir IN LISTS _lucent_priv_dirs)
                # Official Qt stores these as generator expressions. They are
                # not paths at configure time; generate resolves them.
                if(_dir STREQUAL "" OR _dir MATCHES "^\\$<")
                    if(NOT _dir STREQUAL "")
                        set(_lucent_saw_private_include TRUE)
                    endif()
                    continue()
                endif()
                if(EXISTS "${_dir}")
                    set(_lucent_saw_private_include TRUE)
                else()
                    set(_lucent_private_target_ok FALSE)
                endif()
            endforeach()
            if(NOT _lucent_saw_private_include)
                set(_lucent_private_target_ok FALSE)
            endif()
        endif()
    endif()
    if(_lucent_private_target_ok)
        target_link_libraries(voxprint-lucent PRIVATE Qt6::GuiPrivate)
    else()
        find_package(Qt6 REQUIRED COMPONENTS Gui)
        set(_lucent_private "")
        if(DEFINED Qt6Gui_PRIVATE_INCLUDE_DIRS)
            foreach(_dir IN LISTS Qt6Gui_PRIVATE_INCLUDE_DIRS)
                if(EXISTS "${_dir}/rhi/qrhi.h")
                    list(APPEND _lucent_private "${_dir}")
                endif()
            endforeach()
        endif()
        set(_lucent_roots "")
        get_target_property(_gui_dirs Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
        if(_gui_dirs)
            list(APPEND _lucent_roots ${_gui_dirs})
        endif()
        if(CMAKE_LIBRARY_ARCHITECTURE)
            list(APPEND _lucent_roots "/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}/qt6")
        endif()
        list(APPEND _lucent_roots "/usr/include/qt6")
        foreach(_dir IN LISTS _lucent_roots)
            foreach(_candidate
                    "${_dir}/${Qt6_VERSION}/QtGui"
                    "${_dir}/QtGui/${Qt6_VERSION}/QtGui"
                    "${_dir}/../${Qt6_VERSION}/QtGui"
                    "${_dir}/${Qt6_VERSION}/QtGui/private"
                    "${_dir}/QtGui/${Qt6_VERSION}/QtGui/private")
                if(EXISTS "${_candidate}/rhi/qrhi.h")
                    list(APPEND _lucent_private "${_candidate}")
                endif()
            endforeach()
        endforeach()
        if(_lucent_private)
            list(REMOVE_DUPLICATES _lucent_private)
        endif()
        if(_lucent_private STREQUAL "")
            message(FATAL_ERROR
                "Qt ${Qt6_VERSION} is missing Gui private headers (rhi/qrhi.h). "
                "Install qt6-base-private-dev, or use a Qt build whose GuiPrivate includes exist.")
        endif()
        target_include_directories(voxprint-lucent PRIVATE ${_lucent_private})
    endif()
    target_compile_definitions(voxprint-lucent PRIVATE LUCENT_HAS_RHI=1)
endif()

if(WIN32)
    target_link_libraries(voxprint-lucent PRIVATE shlwapi dxgi)
endif()

find_package(PkgConfig QUIET)
set(LIBAVIF_FOUND FALSE)
set(LIBJXL_FOUND FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBAVIF libavif)
endif()
if(LIBAVIF_FOUND)
    target_include_directories(voxprint-lucent PRIVATE ${LIBAVIF_INCLUDE_DIRS})
    target_link_libraries(voxprint-lucent PRIVATE ${LIBAVIF_LINK_LIBRARIES})
    target_compile_definitions(voxprint-lucent PRIVATE LUCENT_HAS_LIBAVIF=1)
endif()
if(PkgConfig_FOUND)
    pkg_check_modules(LIBJXL libjxl)
    pkg_check_modules(LIBJXL_THREADS libjxl_threads)
endif()
if(LIBJXL_FOUND AND LIBJXL_THREADS_FOUND)
    target_include_directories(voxprint-lucent PRIVATE ${LIBJXL_INCLUDE_DIRS} ${LIBJXL_THREADS_INCLUDE_DIRS})
    target_link_libraries(voxprint-lucent PRIVATE ${LIBJXL_LINK_LIBRARIES} ${LIBJXL_THREADS_LINK_LIBRARIES})
    target_compile_definitions(voxprint-lucent PRIVATE LUCENT_HAS_LIBJXL=1)
endif()
message(STATUS "Lucent libavif: ${LIBAVIF_FOUND}")
message(STATUS "Lucent libjxl: ${LIBJXL_FOUND}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    if(MSVC)
        target_compile_options(voxprint-lucent PRIVATE /arch:AVX2 /O2)
    else()
        target_compile_options(voxprint-lucent PRIVATE -O2 -march=x86-64-v3)
    endif()
endif()

add_custom_command(TARGET voxprint-lucent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../share
        $<TARGET_FILE_DIR:voxprint-lucent>/share
    COMMENT "Copy shared palette next to voxprint-lucent"
)
