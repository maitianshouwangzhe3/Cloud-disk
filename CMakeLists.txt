cmake_minimum_required(VERSION 3.10)

project(Cloud-disk)

set(CMAKE_CXX_STANDARD 17)

# 根据不同构建类型设置编译选项（这里只是示例，可按需调整）
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Compile Debug")
    # 针对 Debug 构建类型，添加一些常见的调试相关编译选项，比如开启调试符号等
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
else()
    message("Compile Release")
    # 针对 Release 构建类型，添加优化相关编译选项，例如开启一定级别的优化
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
endif()


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin) # 执行程序文件路径
# set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/lib)    # 库文件路径

add_definitions(-w)

add_subdirectory(code)