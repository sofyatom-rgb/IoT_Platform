cmake_minimum_required(VERSION 3.16)

add_executable(iot_core src/main.cpp)
set_target_properties(iot_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(iot_core PRIVATE Drogon::Drogon)
