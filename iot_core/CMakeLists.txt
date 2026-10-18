cmake_minimum_required(VERSION 3.16)
project(iot_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(Drogon CONFIG REQUIRED)

add_subdirectory(../http_adapter ${CMAKE_BINARY_DIR}/cmake/http_adapter)

add_executable(iot_core src/main.cpp)
target_link_libraries(iot_core PRIVATE Drogon::Drogon)
