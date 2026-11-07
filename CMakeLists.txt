cmake_minimum_required(VERSION 3.16)
project(IoT_Platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
option(BUILD_APPLICATION "Build the Drogon microservices" ON)

if(BUILD_APPLICATION)
    find_package(Drogon CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATH_SUFFIXES jsoncpp)
    find_library(JSONCPP_LIBRARY NAMES jsoncpp)
    if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
        message(FATAL_ERROR "JsonCpp development headers and library are required to build the application")
    endif()

    add_library(iot_drogon_http_transport STATIC
        common/http/DrogonHttpTransport.cpp
    )
    target_include_directories(iot_drogon_http_transport PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${JSONCPP_INCLUDE_DIR}
    )
    target_link_libraries(iot_drogon_http_transport PUBLIC
        Drogon::Drogon
        ${JSONCPP_LIBRARY}
    )

    add_subdirectory(iot_core)
    add_subdirectory(http_adapter)
    add_subdirectory(notification_service)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
