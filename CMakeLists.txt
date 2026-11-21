cmake_minimum_required(VERSION 3.16)
project(IoT_Platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(BUILD_APPLICATION "Build the Drogon microservices" ON)
option(BUILD_CODEC_TESTING "Build encoder/decoder tests (OpenSSL + JsonCpp required)" ON)
option(BUILD_CONTROLLER_TESTING "Build HTTP controller and Drogon transport tests" ${BUILD_APPLICATION})
option(BUILD_POSTGRES_INTEGRATION_TESTING "Build tests that connect to a real PostgreSQL test database" OFF)
option(BUILD_TRANSPORT_INTEGRATION_TESTING "Build tests that exercise the real Drogon HTTP transport" ${BUILD_CONTROLLER_TESTING})

set(IOT_NEEDS_DROGON OFF)
if(BUILD_APPLICATION OR BUILD_CONTROLLER_TESTING OR
   BUILD_POSTGRES_INTEGRATION_TESTING OR BUILD_TRANSPORT_INTEGRATION_TESTING)
    set(IOT_NEEDS_DROGON ON)
endif()

if(IOT_NEEDS_DROGON)
    find_package(Drogon CONFIG REQUIRED)
    add_library(iot_drogon_http_transport STATIC
        common/http/DrogonHttpTransport.cpp
    )
    target_include_directories(iot_drogon_http_transport PUBLIC ${PROJECT_SOURCE_DIR})
    target_link_libraries(iot_drogon_http_transport PUBLIC Drogon::Drogon)
endif()

if(BUILD_CODEC_TESTING OR BUILD_APPLICATION OR BUILD_CONTROLLER_TESTING)
    find_package(OpenSSL REQUIRED)
endif()

if(BUILD_APPLICATION OR BUILD_CODEC_TESTING OR BUILD_CONTROLLER_TESTING OR BUILD_POSTGRES_INTEGRATION_TESTING)
    find_package(jsoncpp CONFIG QUIET)
    if(TARGET JsonCpp::JsonCpp)
        set(IOT_JSONCPP_TARGET JsonCpp::JsonCpp)
    elseif(TARGET jsoncpp_lib)
        set(IOT_JSONCPP_TARGET jsoncpp_lib)
    else()
        find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATH_SUFFIXES jsoncpp)
        find_library(JSONCPP_LIBRARY NAMES jsoncpp)
        if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
            message(FATAL_ERROR
                "JsonCpp development headers and library are required by the selected targets. "
                "Install the JsonCpp development package or disable the corresponding test option.")
        endif()
        add_library(iot_jsoncpp UNKNOWN IMPORTED)
        set_target_properties(iot_jsoncpp PROPERTIES
            IMPORTED_LOCATION "${JSONCPP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIR}"
        )
        set(IOT_JSONCPP_TARGET iot_jsoncpp)
    endif()
endif()

if(BUILD_APPLICATION)
    add_subdirectory(iot_core)
    add_subdirectory(http_adapter)
    add_subdirectory(notification_service)
    add_subdirectory(rule_engine)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
