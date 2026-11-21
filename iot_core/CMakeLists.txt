find_package(OpenSSL REQUIRED)

add_executable(iot_core
    src/main.cpp
    src/telemetry/controllers/TelemetryController.cpp
    src/telemetry/services/TelemetryService.cpp
    src/telemetry/repositories/TelemetryRepository.cpp
    src/telemetry/clients/NotificationServiceClient.cpp
    src/users/controllers/UsersController.cpp
    src/users/services/UsersService.cpp
    src/users/repositories/UsersRepository.cpp
    src/devices/controllers/DevicesController.cpp
    src/devices/services/DevicesService.cpp
    src/devices/repositories/DevicesRepository.cpp
    src/services/PasswordHasher.cpp
    src/auth/controllers/AuthController.cpp
    src/auth/services/AuthService.cpp
    src/auth/services/TokenService.cpp
    src/auth/repositories/AuthRepository.cpp
    src/auth/filters/AuthFilter.cpp
)
target_include_directories(iot_core PRIVATE
    src
    /usr/include/jsoncpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jwt-cpp-include
)
set_target_properties(iot_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(iot_core PRIVATE Drogon::Drogon OpenSSL::Crypto)

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