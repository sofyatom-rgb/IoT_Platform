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
    ${PROJECT_SOURCE_DIR}/http_adapter/src/ingest/services/AdapterService.cpp
    ${PROJECT_SOURCE_DIR}/notification_service/src/services/NotificationService.cpp
)
target_include_directories(service_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/iot_core/src
    ${PROJECT_SOURCE_DIR}/http_adapter/src
    ${PROJECT_SOURCE_DIR}/notification_service/src
)
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
)
target_include_directories(client_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/iot_core/src
    ${PROJECT_SOURCE_DIR}/http_adapter/src
    ${PROJECT_SOURCE_DIR}/notification_service/src
)
add_test(NAME client_unit_tests COMMAND client_unit_tests)
