add_executable(notification_service
    src/main.cpp
    src/controllers/NotificationController.cpp
    src/services/NotificationService.cpp
    src/clients/TelegramBotClient.cpp
)

target_include_directories(notification_service PRIVATE src)
set_target_properties(notification_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(notification_service PRIVATE Drogon::Drogon)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/HTTPConfig.json
    ${CMAKE_BINARY_DIR}/bin/NotificationHTTPConfig.json
    COPYONLY
)
