cmake_minimum_required(VERSION 3.0.0)
project(azure)

# 启用Makefile构建过程中的详细输出
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -O3 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

set(LIB_SRC 
    src/log.cpp
    src/util.cpp
)

# 添加动态库(SHARED)，库的源文件指定为LIB_SRC
add_library(azure SHARED ${LIB_SRC})

# add_library(azure_static STATIC ${LIB_SRC})
# SET_TARGET_PROPERTIES(azure_static PROPERTIES OUTPUT_NAME "azure")

add_executable(test tests/test.cpp)

# test依赖于动态库azure，先生成azure再生成test
add_dependencies(test azure)

# 链接目标文件
target_link_libraries(test azure)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)