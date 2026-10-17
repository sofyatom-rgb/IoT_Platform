cmake_minimum_required(VERSION 3.16)

add_executable(iot_core
    src/main.cpp
    src/controllers/telemetry/TelemetryController.cpp
    src/services/telemetry/TelemetryService.cpp
    src/repositories/telemetry/TelemetryRepository.cpp
)
target_include_directories(iot_core PRIVATE src)
set_target_properties(iot_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(iot_core PRIVATE Drogon::Drogon)
