add_executable(iot_core
    src/main.cpp
    src/telemetry/controllers/TelemetryController.cpp
    src/telemetry/services/TelemetryService.cpp
    src/telemetry/repositories/TelemetryRepository.cpp
    src/telemetry/clients/RuleEngineClient.cpp
    src/users/controllers/UsersController.cpp
    src/users/services/UsersService.cpp
    src/users/repositories/UsersRepository.cpp
    src/devices/controllers/DevicesController.cpp
    src/devices/services/DevicesService.cpp
    src/devices/repositories/DevicesRepository.cpp
)
target_include_directories(iot_core PRIVATE src ${PROJECT_SOURCE_DIR} ${JSONCPP_INCLUDE_DIR})
set_target_properties(iot_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(iot_core PRIVATE Drogon::Drogon ${JSONCPP_LIBRARY} iot_drogon_http_transport)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/HTTPConfig.json
    ${CMAKE_BINARY_DIR}/config/HTTPConfig.json
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/PostgreSQLConfig.json
    ${CMAKE_BINARY_DIR}/config/PostgreSQLConfig.json
    COPYONLY
)