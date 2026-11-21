# All mocks in unit and component tests are created with FakeIt.
set(IOT_TEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/iot_core/src
    ${PROJECT_SOURCE_DIR}/http_adapter/src
    ${PROJECT_SOURCE_DIR}/notification_service/src
    ${PROJECT_SOURCE_DIR}/tests/third_party/fakeit
)

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

if(BUILD_CODEC_TESTING)
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
    target_link_libraries(codec_unit_tests PRIVATE OpenSSL::Crypto ${IOT_JSONCPP_TARGET})
    add_test(NAME codec_unit_tests COMMAND codec_unit_tests)
endif()

add_executable(service_unit_tests
    test_main.cpp
    services/UsersServiceTests.cpp
    services/DevicesServiceTests.cpp
    services/TelemetryServiceTests.cpp
    services/AdapterServiceTests.cpp
    services/NotificationServiceTests.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/users/services/UsersService.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/devices/services/DevicesService.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/services/TelemetryService.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/clients/NotificationServiceClient.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
    ${PROJECT_SOURCE_DIR}/notification_service/src/services/NotificationService.cpp
    ${PROJECT_SOURCE_DIR}/notification_service/src/clients/TelegramBotClient.cpp
)
target_include_directories(service_unit_tests PRIVATE ${IOT_TEST_INCLUDE_DIRS})
add_test(NAME service_unit_tests COMMAND service_unit_tests)

add_executable(client_unit_tests
    test_main.cpp
    clients/ClientTests.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/clients/NotificationServiceClient.cpp
    ${PROJECT_SOURCE_DIR}/notification_service/src/clients/TelegramBotClient.cpp
)
target_include_directories(client_unit_tests PRIVATE ${IOT_TEST_INCLUDE_DIRS})
add_test(NAME client_unit_tests COMMAND client_unit_tests)

add_executable(controller_validation_tests
    test_main.cpp
    controllers/ControllerValidationTests.cpp
)
target_include_directories(controller_validation_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)
add_test(NAME controller_validation_tests COMMAND controller_validation_tests)

add_executable(adapter_pipeline_tests
    test_main.cpp
    integration/AdapterPipelineTests.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
)
target_include_directories(adapter_pipeline_tests PRIVATE ${IOT_TEST_INCLUDE_DIRS})
add_test(NAME adapter_pipeline_tests COMMAND adapter_pipeline_tests)

add_executable(telemetry_notification_pipeline_tests
    test_main.cpp
    integration/TelemetryNotificationPipelineTests.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/services/TelemetryService.cpp
    ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/clients/NotificationServiceClient.cpp
)
target_include_directories(telemetry_notification_pipeline_tests PRIVATE ${IOT_TEST_INCLUDE_DIRS})
add_test(NAME telemetry_notification_pipeline_tests
         COMMAND telemetry_notification_pipeline_tests)

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
        ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/services/TelemetryService.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Base64.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Checksum.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Encoder.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/codec/Decoder.cpp
    )
    target_include_directories(controller_unit_tests PRIVATE ${IOT_TEST_INCLUDE_DIRS})
    target_link_libraries(controller_unit_tests PRIVATE
        Drogon::Drogon OpenSSL::Crypto ${IOT_JSONCPP_TARGET}
        iot_drogon_http_transport
    )
    add_test(NAME controller_unit_tests COMMAND controller_unit_tests)
    set_tests_properties(controller_unit_tests PROPERTIES TIMEOUT 45)
endif()

if(BUILD_POSTGRES_INTEGRATION_TESTING)
    add_executable(repository_integration_tests
        test_main.cpp
        integration/RepositoryIntegrationTests.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/users/repositories/UsersRepository.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/devices/repositories/DevicesRepository.cpp
        ${PROJECT_SOURCE_DIR}/iot_core/src/telemetry/repositories/TelemetryRepository.cpp
    )
    target_include_directories(repository_integration_tests PRIVATE ${IOT_TEST_INCLUDE_DIRS})
    target_link_libraries(repository_integration_tests PRIVATE
        Drogon::Drogon ${IOT_JSONCPP_TARGET}
    )
    add_test(NAME repository_integration_tests COMMAND repository_integration_tests)
    set_tests_properties(repository_integration_tests PROPERTIES TIMEOUT 120)
endif()

if(BUILD_TRANSPORT_INTEGRATION_TESTING)
    add_executable(drogon_transport_integration_tests
        transport/TransportTestMain.cpp
        transport/DrogonHttpTransportTests.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
        ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/clients/IoTClient.cpp
    )
    target_include_directories(drogon_transport_integration_tests PRIVATE ${IOT_TEST_INCLUDE_DIRS})
    target_link_libraries(drogon_transport_integration_tests PRIVATE
        Drogon::Drogon iot_drogon_http_transport
    )
    add_test(NAME drogon_transport_integration_tests
             COMMAND drogon_transport_integration_tests)
    set_tests_properties(drogon_transport_integration_tests PROPERTIES TIMEOUT 90)
endif()

# CTest captures test output, so the test process is not attached to a TTY.
# Force ANSI colors so `ctest --verbose` shows each passed/failed case in color.
get_property(IOT_REGISTERED_TESTS DIRECTORY PROPERTY TESTS)
if(IOT_REGISTERED_TESTS)
    set_tests_properties(${IOT_REGISTERED_TESTS} PROPERTIES ENVIRONMENT "FORCE_COLOR=1")
endif()
