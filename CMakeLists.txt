cmake_minimum_required(VERSION 4.2)
project(log-view)

find_package(fmt REQUIRED)
set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt cpp-httplib)
