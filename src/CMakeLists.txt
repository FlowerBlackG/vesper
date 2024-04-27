#[[
    vesper 顶层构建文件
    创建于 2023年12月26日 上海市嘉定区安亭镇
]]

cmake_minimum_required(VERSION 3.29.2)


#[[
    Vesper 版本号
]]
set(VESPER_VERSION_MAJOR 0)
set(VESPER_VERSION_MINOR 0)
set(VESPER_VERSION_PATCH 4)
set(VESPER_VERSION_TAIL "-dev") # 例："-dev"

set(VESPER_VERSION_CODE 4)

string(TIMESTAMP build_time_iso8601 UTC)  # example: 2024-04-19T13:39Z
string(TIMESTAMP build_time_human_readable "%B %d, %Y %H:%M:%S %Z")
set(VESPER_BUILD_TIME_ISO8601 ${build_time_iso8601})
set(VESPER_BUILD_TIME_HUMAN_READABLE ${build_time_human_readable})

set(
    VESPER_VERSION_NAME 
    "${VESPER_VERSION_MAJOR}.${VESPER_VERSION_MINOR}.${VESPER_VERSION_PATCH}${VESPER_VERSION_TAIL}"
)

#[[
    项目基本配置信息
]]
project(vesper)
configure_file(
    ${PROJECT_SOURCE_DIR}/config.cpp.in 
    ${PROJECT_SOURCE_DIR}/config.cpp
)

# 指定使用 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)


include_directories(${PROJECT_SOURCE_DIR})


#[[ 
    GCC(g++) 编译选项
]]
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")


#[[ 
    项目构造产物
]]
file(GLOB_RECURSE CPP_SOURCE_FILES *.cpp)
file(GLOB_RECURSE C_SOURCE_FILES *.c)
add_executable(
    ${PROJECT_NAME} ${CPP_SOURCE_FILES} ${C_SOURCE_FILES}
)


#[[ 
    寻找依赖库
]]
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake-modules)
find_package(Wayland REQUIRED)
find_package(Pixman REQUIRED)
find_package(Wlroots REQUIRED)
find_package(XKB REQUIRED)
find_package(LibVNCServer REQUIRED)
find_package(Systemd REQUIRED)
find_package(WaylandProtocols REQUIRED)

find_package(JPEG REQUIRED)


#[[
    关联依赖库
]]

include_directories(
    ${WAYLAND_INCLUDE_DIR} 
    ${WLR_INCLUDE_DIRS} 
    ${XKB_INCLUDE_DIRS} 
    ${PIXMAN_INCLUDE_DIRS}
    ${SYSTEMD_INCLUDE_DIRS}
    ${WAYLAND_PROTOCOLS_GEN_DIR}


    ${JPEG_INCLUDE_DIRS}
)


target_link_libraries(
    ${PROJECT_NAME} 

    ${WAYLAND_LIBRARIES} 
    ${WLR_LIBRARIES} 
    ${XKB_LIBRARIES} 
    ${PIXMAN_LIBRARIES}
    LibVNCServer::vncserver
    ${SYSTEMD_LIBRARIES}

    ${JPEG_LIBRARIES}
)


#[[
    wayland protocols
]]

message(STATUS "wayland protocols at: ${WAYLANDPROTOCOLS_PATH}")

set(WAYLAND_PROTOCOLS_GEN_DIR ${CMAKE_BINARY_DIR}/wayland-protocols)

make_directory(${WAYLAND_PROTOCOLS_GEN_DIR})


# xdg-shell

set(XDG_PROTOCOL_DEF "${WAYLANDPROTOCOLS_PATH}/stable/xdg-shell/xdg-shell.xml")

add_custom_command(
    OUTPUT ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-client-protocol.h
    COMMAND wayland-scanner client-header ${XDG_PROTOCOL_DEF} ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-client-protocol.h
)

add_custom_command(
    OUTPUT ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-protocol.h
    COMMAND wayland-scanner client-header ${XDG_PROTOCOL_DEF} ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-protocol.h
)

add_custom_command(
    OUTPUT ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-protocol.c
    COMMAND wayland-scanner private-code ${XDG_PROTOCOL_DEF} ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-protocol.c
    DEPENDS ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-client-protocol.h
        ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-protocol.h
)

target_include_directories(${PROJECT_NAME} PRIVATE ${WAYLAND_PROTOCOLS_GEN_DIR})
target_sources(${PROJECT_NAME} PRIVATE ${WAYLAND_PROTOCOLS_GEN_DIR}/xdg-shell-protocol.c)
