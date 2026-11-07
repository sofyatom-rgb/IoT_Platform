add_executable(service_unit_tests
    test_main.cpp
    services/UsersServiceTests.cpp
    services/DevicesServiceTests.cpp
    services/TelemetryServiceTests.cpp
    services/AdapterServiceTests.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/users/services/UsersService.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/devices/services/DevicesService.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/services/TelemetryService.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
)
target_include_directories(service_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/iot_core/src
    ${PROJECT_SOURCE_DIR}/http_adapter/src
    ${PROJECT_SOURCE_DIR}
)
if(BUILD_APPLICATION)
    target_sources(service_unit_tests PRIVATE
        services/NotificationServiceTests.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/services/NotificationService.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/clients/WorkerThreadPool.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/EmailConfig.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/EmailTemplate.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/EmailService.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/TelegramMessageTemplates.cpp
    )
    target_include_directories(service_unit_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/notification_service/src
        ${JSONCPP_INCLUDE_DIR}
    )
    target_compile_definitions(service_unit_tests PRIVATE
        TELEGRAM_TEMPLATES_CONFIG_PATH="${PROJECT_SOURCE_DIR}/notification_service/config/TelegramTemplates.json"
    )
    target_link_libraries(service_unit_tests PRIVATE Drogon::Drogon CURL::libcurl)
endif()
add_test(NAME service_unit_tests COMMAND service_unit_tests)

add_executable(codec_primitive_tests
    test_main.cpp
    codec/CodecPrimitiveTests.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Base64.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Checksum.cpp
)
target_include_directories(codec_primitive_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/http_adapter/src
)
add_test(NAME codec_primitive_tests COMMAND codec_primitive_tests)

option(BUILD_CODEC_TESTING "Build codec unit tests (requires OpenSSL and JsonCpp)" ON)
if(BUILD_CODEC_TESTING)
    find_package(OpenSSL REQUIRED)
    find_package(jsoncpp CONFIG QUIET)

    if(TARGET JsonCpp::JsonCpp)
        set(ARS_JSONCPP_TARGET JsonCpp::JsonCpp)
    elseif(TARGET jsoncpp_lib)
        set(ARS_JSONCPP_TARGET jsoncpp_lib)
    else()
        find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATH_SUFFIXES jsoncpp)
        find_library(JSONCPP_LIBRARY NAMES jsoncpp)
        if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
            message(FATAL_ERROR "BUILD_CODEC_TESTING=ON requires JsonCpp development headers and library")
        endif()
        add_library(ars_jsoncpp UNKNOWN IMPORTED)
        set_target_properties(ars_jsoncpp PROPERTIES
            IMPORTED_LOCATION "${JSONCPP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIR}"
        )
        set(ARS_JSONCPP_TARGET ars_jsoncpp)
    endif()

    add_executable(codec_unit_tests
        test_main.cpp
        codec/CodecTests.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Base64.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Checksum.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Encoder.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Decoder.cpp
    )
    target_include_directories(codec_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/http_adapter/src
    )
    target_link_libraries(codec_unit_tests PRIVATE OpenSSL::Crypto ${ARS_JSONCPP_TARGET})
    add_test(NAME codec_unit_tests COMMAND codec_unit_tests)
endif()

add_executable(client_unit_tests
    test_main.cpp
    clients/ClientTests.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/clients/NotificationServiceClient.cpp
    ${PROJECT_SOURCE_DIR}/notification_service/src/clients/TelegramBotClient.cpp
    ${PROJECT_SOURCE_DIR}/notification_service/src/clients/WorkerThreadPool.cpp
)
target_include_directories(client_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/iot_core/src
    ${PROJECT_SOURCE_DIR}/http_adapter/src
    ${PROJECT_SOURCE_DIR}/notification_service/src
)
add_test(NAME client_unit_tests COMMAND client_unit_tests)

# Pure controller-input validation tests: no Drogon/PostgreSQL required.
add_executable(controller_validation_tests
    test_main.cpp
    controllers/ControllerValidationTests.cpp
)
target_include_directories(controller_validation_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)
add_test(NAME controller_validation_tests COMMAND controller_validation_tests)

# Component integration tests exercise real service + client code together,
# while replacing only the network/database boundary with fakes.
add_executable(adapter_pipeline_tests
    test_main.cpp
    integration/AdapterPipelineTests.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
)
target_include_directories(adapter_pipeline_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/http_adapter/src
)
add_test(NAME adapter_pipeline_tests COMMAND adapter_pipeline_tests)

add_executable(telemetry_notification_pipeline_tests
    test_main.cpp
    integration/TelemetryNotificationPipelineTests.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/services/TelemetryService.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/clients/NotificationServiceClient.cpp
)
target_include_directories(telemetry_notification_pipeline_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/iot_core/src
)
add_test(NAME telemetry_notification_pipeline_tests
         COMMAND telemetry_notification_pipeline_tests)

# These tests call the actual Drogon handlers with synthetic requests and fake
# service dependencies. They need Drogon + JsonCpp, but no running database.
if(BUILD_CONTROLLER_TESTING)
    add_executable(controller_unit_tests
        controllers/ControllerTestMain.cpp
        controllers/ControllerTests.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/users/controllers/UsersController.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/devices/controllers/DevicesController.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/controllers/TelemetryController.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/controllers/IngestController.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/controllers/NotificationController.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/services/NotificationService.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/clients/WorkerThreadPool.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/EmailConfig.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/EmailTemplate.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/EmailService.cpp
        ${PROJECT_SOURCE_DIR}/notification_service/src/notifications/TelegramMessageTemplates.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/services/TelemetryService.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Base64.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Checksum.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Encoder.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Decoder.cpp
    )
    target_include_directories(controller_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/iot_core/src
        ${PROJECT_SOURCE_DIR}/http_adapter/src
        ${PROJECT_SOURCE_DIR}/notification_service/src
        ${JSONCPP_INCLUDE_DIR}
    )
    target_compile_definitions(controller_unit_tests PRIVATE
        EMAIL_TEMPLATE_DIR="${CMAKE_BINARY_DIR}/resources/email"
    )
    target_link_libraries(controller_unit_tests PRIVATE
        Drogon::Drogon
        OpenSSL::Crypto
        ${JSONCPP_LIBRARY}
        iot_drogon_http_transport
        CURL::libcurl
    )
    add_test(NAME controller_unit_tests COMMAND controller_unit_tests)
    set_tests_properties(controller_unit_tests PROPERTIES TIMEOUT 45)
endif()

# Real PostgreSQL repository checks. Opt in explicitly because they require a
# disposable test database initialized with the project's init.sql schema.
if(BUILD_POSTGRES_INTEGRATION_TESTING)
    add_executable(repository_integration_tests
        test_main.cpp
        integration/RepositoryIntegrationTests.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/users/repositories/UsersRepository.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/devices/repositories/DevicesRepository.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/repositories/TelemetryRepository.cpp
    )
    target_include_directories(repository_integration_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/iot_core/src
        ${JSONCPP_INCLUDE_DIR}
    )
    target_link_libraries(repository_integration_tests PRIVATE
        Drogon::Drogon
        ${JSONCPP_LIBRARY}
    )
    add_test(NAME repository_integration_tests COMMAND repository_integration_tests)
    set_tests_properties(repository_integration_tests PROPERTIES TIMEOUT 120)
endif()

# Exercise the concrete Drogon transport against a one-shot local TCP server.
# Unlike the client unit tests, this one uses real sockets and the Drogon loop.
if(BUILD_CONTROLLER_TESTING)
    add_executable(drogon_transport_integration_tests
        transport/TransportTestMain.cpp
        transport/DrogonHttpTransportTests.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
    )
    target_include_directories(drogon_transport_integration_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${JSONCPP_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/http_adapter/src
    )
    target_link_libraries(drogon_transport_integration_tests PRIVATE
        Drogon::Drogon
        ${JSONCPP_LIBRARY}
        iot_drogon_http_transport
    )
    add_test(NAME drogon_transport_integration_tests
             COMMAND drogon_transport_integration_tests)
    set_tests_properties(drogon_transport_integration_tests PROPERTIES TIMEOUT 90)
endif()
