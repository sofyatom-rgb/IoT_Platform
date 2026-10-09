cmake_minimum_required(VERSION 3.16)
project(iot_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

include(FetchContent)

FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.16.0
)

FetchContent_MakeAvailable(cpp-httplib)

add_subdirectory(http_adapter)

add_executable(iot_core src/main.cpp)
target_link_libraries(iot_core PRIVATE Drogon::Drogon)
