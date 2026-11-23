cmake_minimum_required(VERSION 3.16)

## Build command:
## cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
## cmake --build build

project(FlowMapWater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 3.4.0 CONFIG REQUIRED)
find_package(SDL3_image 3.4.0 CONFIG REQUIRED)
# find_package(SDL3_ttf 3.2.2 CONFIG REQUIRED)

if(MSVC)
add_compile_options("/source-charset:utf-8")
endif()

add_executable(${PROJECT_NAME} main.cpp)


target_link_libraries(${PROJECT_NAME} PUBLIC
SDL3::SDL3
SDL3_image::SDL3_image
# SDL3_ttf::SDL3_ttf
)

# 把资源文件复制到构建目录，方便直接运行
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/data/flowmap.png
        ${CMAKE_SOURCE_DIR}/data/base.png
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)