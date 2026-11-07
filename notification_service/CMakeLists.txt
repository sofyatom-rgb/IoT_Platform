find_package(TgBot CONFIG REQUIRED)

add_executable(notification_service
    src/main.cpp
    src/controllers/NotificationController.cpp
    src/services/NotificationService.cpp
    src/clients/TelegramBotClient.cpp
    src/clients/TgBotApi.cpp
    src/clients/WorkerThreadPool.cpp
    src/notifications/EmailConfig.cpp
    src/notifications/EmailTemplate.cpp
    src/notifications/EmailService.cpp
)

target_include_directories(notification_service PRIVATE
    src
    ${PROJECT_SOURCE_DIR}
    ${JSONCPP_INCLUDE_DIR}
)
target_compile_definitions(notification_service PRIVATE
    EMAIL_TEMPLATE_DIR="${CMAKE_BINARY_DIR}/resources/email"
)
set_target_properties(notification_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(notification_service PRIVATE
    Drogon::Drogon
    ${JSONCPP_LIBRARY}
    iot_drogon_http_transport
    TgBot::TgBot
    CURL::libcurl
)

if(BUILD_TESTING)
    add_executable(notification_service_tests
        tests/notification_service_tests.cpp
        src/notifications/EmailConfig.cpp
        src/notifications/EmailTemplate.cpp
        src/notifications/EmailService.cpp
    )
    target_include_directories(notification_service_tests PRIVATE src)
    target_compile_definitions(notification_service_tests PRIVATE
        EMAIL_TEMPLATE_DIR="${CMAKE_BINARY_DIR}/resources/email"
    )
    target_link_libraries(notification_service_tests PRIVATE CURL::libcurl)
    add_test(NAME notification_service_tests COMMAND notification_service_tests)
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/resources/email")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/email/alert.html
    ${CMAKE_BINARY_DIR}/resources/email/alert.html
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/email/alert.txt
    ${CMAKE_BINARY_DIR}/resources/email/alert.txt
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/HTTPConfig.json
    ${CMAKE_BINARY_DIR}/config/NotificationHTTPConfig.json
    COPYONLY
)
