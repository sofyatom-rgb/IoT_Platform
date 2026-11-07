cmake_minimum_required(VERSION 3.30)
project(IoT_Platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
option(BUILD_APPLICATION "Build the Drogon microservices" ON)
option(BUILD_CONTROLLER_TESTING
       "Build tests that call the real Drogon controller handlers" ${BUILD_APPLICATION})
option(BUILD_POSTGRES_INTEGRATION_TESTING
       "Build repository integration tests (requires a running test PostgreSQL)" OFF)
option(ENABLE_COVERAGE "Compile project and tests with GCC coverage instrumentation" OFF)

if(BUILD_APPLICATION OR (BUILD_TESTING AND BUILD_CONTROLLER_TESTING))
    find_package(Boost 1.81 REQUIRED)
    find_package(OpenSSL REQUIRED COMPONENTS SSL)
    find_package(CURL REQUIRED)
    if(BUILD_APPLICATION)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/config")
    endif()
endif()

if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "ENABLE_COVERAGE currently supports GCC only")
    endif()
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
endif()

# Drogon/JsonCpp are needed by the application and by the optional controller /
# repository integration test targets. The ordinary unit tests remain standalone.
if(BUILD_APPLICATION OR BUILD_CONTROLLER_TESTING OR BUILD_POSTGRES_INTEGRATION_TESTING)
    find_package(Drogon CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATH_SUFFIXES jsoncpp)
    find_library(JSONCPP_LIBRARY NAMES jsoncpp)
    if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
        message(FATAL_ERROR "JsonCpp development headers and library are required")
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
endif()

if(BUILD_APPLICATION)
    add_subdirectory(iot_core)
    add_subdirectory(http_adapter)
    add_subdirectory(notification_service)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(ENABLE_COVERAGE)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        add_custom_target(coverage_report
            COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure
            COMMAND ${LCOV_EXECUTABLE} --capture --directory ${CMAKE_BINARY_DIR}
                    --output-file ${CMAKE_BINARY_DIR}/coverage.info
            COMMAND ${LCOV_EXECUTABLE} --remove ${CMAKE_BINARY_DIR}/coverage.info
                    "/usr/*" "*/tests/*" "*/build-*/*"
                    --output-file ${CMAKE_BINARY_DIR}/coverage.filtered.info
            COMMAND ${GENHTML_EXECUTABLE} ${CMAKE_BINARY_DIR}/coverage.filtered.info
                    --output-directory ${CMAKE_BINARY_DIR}/coverage-html
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMENT "Running tests and generating an HTML coverage report"
            VERBATIM
        )
    else()
        message(WARNING "lcov and genhtml were not found; coverage flags are enabled, but coverage_report target is unavailable")
    endif()
endif()
