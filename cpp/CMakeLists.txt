cmake_minimum_required(VERSION 3.24)
project(voxprint-lucent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

qt_standard_project_setup()

qt_add_executable(voxprint-lucent
    src/main.cpp
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
)

target_link_libraries(voxprint-lucent PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent)
target_include_directories(voxprint-lucent PRIVATE src)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.7.0)
    # Qt 6.9+ ships a GuiPrivate package. Debian 13's Qt 6.8 puts the same
    # private headers inside Gui and has no GuiPrivate config file.
    find_package(Qt6 QUIET COMPONENTS GuiPrivate)
    if(TARGET Qt6::GuiPrivate)
        target_link_libraries(voxprint-lucent PRIVATE Qt6::GuiPrivate)
    else()
        find_package(Qt6 REQUIRED COMPONENTS Gui)
        set(_lucent_private "")
        if(DEFINED Qt6Gui_PRIVATE_INCLUDE_DIRS)
            list(APPEND _lucent_private ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
        endif()
        get_target_property(_gui_dirs Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_dir IN LISTS _gui_dirs)
            foreach(_candidate
                    "${_dir}/${Qt6_VERSION}/QtGui/private"
                    "${_dir}/../${Qt6_VERSION}/QtGui/private"
                    "${_dir}/QtGui/${Qt6_VERSION}/QtGui/private")
                if(EXISTS "${_candidate}/rhi/qrhi.h")
                    list(APPEND _lucent_private "${_candidate}")
                endif()
            endforeach()
        endforeach()
        if(_lucent_private STREQUAL "")
            message(FATAL_ERROR "Qt ${Qt6_VERSION} is missing Gui private headers (rhi/qrhi.h)")
        endif()
        target_include_directories(voxprint-lucent PRIVATE ${_lucent_private})
    endif()
    target_compile_definitions(voxprint-lucent PRIVATE LUCENT_HAS_RHI=1)
endif()

if(WIN32)
    target_link_libraries(voxprint-lucent PRIVATE shlwapi)
endif()

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
