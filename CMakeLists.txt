cmake_minimum_required(VERSION 3.0.0)
project(azure)

#包含了一些函数，比如可以修改__FILE__宏定义的函数
include(cmake/utils.cmake)

# 启用Makefile构建过程中的详细输出
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_BUILD_TYPE Debug)

# -Wno-忽略警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -O0 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function \
                        -Wno-builtin-macro-redefined")

# find_library(YAMLCPP yaml-cpp)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(LIB_SRC 
    src/log.cpp
    src/fiber.cpp
    src/util.cpp
    src/config.cpp
    src/thread.cpp
    src/mutex.cpp
    src/scheduler.cpp
    src/iomanager.cpp
    src/timer.cpp
    src/hook.cpp
    src/fdmanager.cpp
    src/address.cpp
)

# 添加动态库(SHARED)，库的源文件指定为LIB_SRC
add_library(azure SHARED ${LIB_SRC})

# 修改__FILE__
force_redefine_file_macro_for_sources(azure)

# add_library(azure_static STATIC ${LIB_SRC})
# SET_TARGET_PROPERTIES(azure_static PROPERTIES OUTPUT_NAME "azure")

set(LIB_LIB
    azure
    pthread
    yaml-cpp
    dl
)

# test_log
add_executable(test_log tests/test_log.cpp)
add_dependencies(test_log azure)                        # test依赖于动态库azure，先生成azure再生成test
force_redefine_file_macro_for_sources(test_log)         # 修改__FILE__
target_link_libraries(test_log ${LIB_LIB})              # 链接目标文件

# test_config
add_executable(test_config tests/test_config.cpp)
add_dependencies(test_config azure)
force_redefine_file_macro_for_sources(test_config)      # 修改__FILE__
target_link_libraries(test_config ${LIB_LIB})

# test_thread
add_executable(test_thread tests/test_thread.cpp)
add_dependencies(test_thread azure)
force_redefine_file_macro_for_sources(test_thread)      # 修改__FILE__
target_link_libraries(test_thread ${LIB_LIB})

# test_util
add_executable(test_util tests/test_util.cpp)
add_dependencies(test_util azure)
force_redefine_file_macro_for_sources(test_util)        # 修改__FILE__
target_link_libraries(test_util ${LIB_LIB})

# test_fiber
add_executable(test_fiber tests/test_fiber.cpp)
add_dependencies(test_fiber azure)
force_redefine_file_macro_for_sources(test_fiber)       # 修改__FILE__
target_link_libraries(test_fiber ${LIB_LIB})

# test_scheduler
add_executable(test_scheduler tests/test_scheduler.cpp)
add_dependencies(test_scheduler azure)
force_redefine_file_macro_for_sources(test_scheduler)     # 修改__FILE__
target_link_libraries(test_scheduler ${LIB_LIB})

# test_iomanager
add_executable(test_iomanager tests/test_iomanager.cpp)
add_dependencies(test_iomanager azure)
force_redefine_file_macro_for_sources(test_iomanager)     # 修改__FILE__
target_link_libraries(test_iomanager ${LIB_LIB})

# test_hook
add_executable(test_hook tests/test_hook.cpp)
add_dependencies(test_hook azure)
force_redefine_file_macro_for_sources(test_hook)     # 修改__FILE__
target_link_libraries(test_hook ${LIB_LIB})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)