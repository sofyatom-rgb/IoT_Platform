cmake_minimum_required(VERSION 3.30)
project(IoT_Platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Drogon CONFIG REQUIRED)
find_package(TgBot CONFIG REQUIRED)
find_package(Git REQUIRED)
find_package(Boost 1.81 REQUIRED COMPONENTS date_time regex)
find_package(OpenSSL REQUIRED COMPONENTS SSL)

include(FetchContent)
include(CTest)

set(MAILIO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(MAILIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MAILIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    mailio
    GIT_REPOSITORY https://github.com/karastojko/mailio.git
    GIT_TAG 0.26.1
    PATCH_COMMAND
        ${GIT_EXECUTABLE} apply --whitespace=nowarn
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/mailio-system-ca.patch
)
FetchContent_MakeAvailable(mailio)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/config")

add_subdirectory(iot_core)
add_subdirectory(http_adapter)
add_subdirectory(notification_service)
