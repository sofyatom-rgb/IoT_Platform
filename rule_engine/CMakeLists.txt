cmake_minimum_required(VERSION 3.16)
project(rule_engine)

add_executable(rule_engine
    src/main.cpp
    src/controllers/RuleController.cpp
    src/services/RuleService.cpp
    src/repositories/RuleRepository.cpp
    src/clients/NotificationClient.cpp
)
target_include_directories(rule_engine PRIVATE src /usr/include/jsoncpp)
set_target_properties(rule_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(rule_engine PRIVATE Drogon::Drogon)

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