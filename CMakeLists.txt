cmake_minimum_required(VERSION 3.20)

project(dm_gui_tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(dm_gui
    app/src/main.cpp
    app/src/dm_device_wrapper.cpp
    app/src/dm_device_wrapper.h
    app/src/main_window.cpp
    app/src/main_window.h
)

target_include_directories(dm_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
)

target_link_libraries(dm_gui PRIVATE Qt6::Widgets)

if(WIN32)
    if(MSVC)
        set(DM_SDK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/windows/msvc)
        target_link_libraries(dm_gui PRIVATE ${DM_SDK_LIB_DIR}/dm_device.lib)
        add_custom_command(TARGET dm_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DM_SDK_LIB_DIR}/dm_device.dll
                $<TARGET_FILE_DIR:dm_gui>
        )
    else()
        set(DM_SDK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/windows/mingw)
        target_link_libraries(dm_gui PRIVATE ${DM_SDK_LIB_DIR}/libdm_device.dll.a)
        add_custom_command(TARGET dm_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DM_SDK_LIB_DIR}/libdm_device.dll
                $<TARGET_FILE_DIR:dm_gui>
        )
    endif()
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(DM_SDK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/linux/aarch64)
    else()
        set(DM_SDK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/linux/x86_64)
    endif()
    target_include_directories(dm_gui PRIVATE ${LIBUSB_INCLUDE_DIRS})
    target_link_libraries(dm_gui PRIVATE ${DM_SDK_LIB_DIR}/libdm_device.so ${LIBUSB_LIBRARIES})
    add_custom_command(TARGET dm_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DM_SDK_LIB_DIR}/libdm_device.so
            $<TARGET_FILE_DIR:dm_gui>
    )
endif()

install(TARGETS dm_gui
    RUNTIME DESTINATION .
)

if(WIN32)
    if(MSVC)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/windows/msvc/dm_device.dll DESTINATION .)
    else()
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/windows/mingw/libdm_device.dll DESTINATION .)
    endif()
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/linux/aarch64/libdm_device.so DESTINATION .)
    else()
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/linux/x86_64/libdm_device.so DESTINATION .)
    endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Package.cmake)
