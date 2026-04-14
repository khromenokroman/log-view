cmake_minimum_required(VERSION 4.2)
project(pretty_log)
find_package(fmt REQUIRED)
set(CMAKE_CXX_STANDARD 20)

add_executable(pretty_log main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt cpp-httplib)
