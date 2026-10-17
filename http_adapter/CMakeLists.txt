cmake_minimum_required(VERSION 3.16)

add_executable(http_adapter src/main.cc)
set_target_properties(http_adapter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(http_adapter PRIVATE Drogon::Drogon)
