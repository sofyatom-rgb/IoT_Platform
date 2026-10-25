cmake_minimum_required(VERSION 3.16)
project(IoT_Platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Drogon CONFIG REQUIRED)

add_subdirectory(iot_core)
add_subdirectory(http_adapter)
add_subdirectory(notification_service)
