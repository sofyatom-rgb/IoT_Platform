add_executable(iot_core
    src/main.cpp
    src/telemetry/controllers/TelemetryController.cpp
    src/telemetry/services/TelemetryService.cpp
    src/telemetry/services/TelemetryServiceDrogon.cpp
    src/telemetry/repositories/TelemetryRepository.cpp
    src/telemetry/clients/NotificationServiceClient.cpp
    src/telemetry/clients/NotificationServiceClientDrogon.cpp
    src/users/controllers/UsersController.cpp
    src/users/services/UsersService.cpp
    src/users/services/UsersServiceDrogon.cpp
    src/users/repositories/UsersRepository.cpp
    src/devices/controllers/DevicesController.cpp
    src/devices/services/DevicesService.cpp
    src/devices/services/DevicesServiceDrogon.cpp
    src/devices/repositories/DevicesRepository.cpp
    src/services/PasswordHasher.cpp
)
target_include_directories(iot_core PRIVATE src ${PROJECT_SOURCE_DIR})
set_target_properties(iot_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(iot_core PRIVATE Drogon::Drogon OpenSSL::Crypto iot_drogon_http_transport ${IOT_JSONCPP_TARGET})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/HTTPConfig.json
    ${CMAKE_BINARY_DIR}/bin/HTTPConfig.json
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/PostgreSQLConfig.json
    ${CMAKE_BINARY_DIR}/bin/PostgreSQLConfig.json
    COPYONLY
)