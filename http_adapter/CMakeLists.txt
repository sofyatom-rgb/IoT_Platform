cmake_minimum_required(VERSION 3.16)
project(http_adapter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(http_adapter src/main.cc)
set_target_properties(http_adapter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(http_adapter PRIVATE Drogon::Drogon)
