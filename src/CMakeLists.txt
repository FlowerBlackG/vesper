#[[
    vesper 顶层构建文件
    创建于 2023年12月26日 上海市嘉定区安亭镇
]]

cmake_minimum_required(VERSION 3.28.1)


#[[
    Vesper 版本号
]]
set(VESPER_VERSION_MAJOR 0)
set(VESPER_VERSION_MINOR 0)
set(VESPER_VERSION_PATCH 1)
set(VESPER_VERSION_TAIL "") # 例："-dev"

set(VESPER_VERSION_CODE 1)

set(
    VESPER_VERSION_NAME 
    "${VESPER_VERSION_MAJOR}.${VESPER_VERSION_MINOR}.${VESPER_VERSION_PATCH}${VESPER_VERSION_TAIL}"
)

#[[
    项目基本配置信息
]]
project(vesper)
configure_file(
    ${PROJECT_SOURCE_DIR}/config.h.in 
    ${PROJECT_SOURCE_DIR}/config.h
)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake-modules)

# 指定使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

include_directories(${PROJECT_SOURCE_DIR})



#[[ 
    GCC(g++) 编译选项
]]
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

#[[ 
    项目构造产物
]]
file(GLOB_RECURSE CPP_SOURCE_FILES *.cpp)
add_executable(
    ${PROJECT_NAME} ${CPP_SOURCE_FILES}
)


#[[ 
    依赖库
]]
find_package(Wayland REQUIRED)
find_package(Pixman REQUIRED)
find_package(Wlroots REQUIRED)
find_package(XKB REQUIRED)
find_package(LibVNCServer REQUIRED)

include_directories(
    ${WAYLAND_INCLUDE_DIR} 
    ${WLR_INCLUDE_DIRS} 
    ${XKB_INCLUDE_DIRS} 
    ${PIXMAN_INCLUDE_DIRS}
)


target_link_libraries(
    ${PROJECT_NAME} 

    ${WAYLAND_LIBRARIES} 
    ${WLR_LIBRARIES} 
    ${XKB_LIBRARIES} 
    ${PIXMAN_LIBRARIES}
    LibVNCServer::vncserver
)
