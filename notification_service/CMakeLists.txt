cmake_minimum_required(VERSION 3.16)
project(notification_service CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(CURL REQUIRED)

add_executable(notification_service
    src/main.cpp
    src/controllers/NotificationController.cpp
    src/clients/TelegramBotClient.cpp
    src/clients/TelegramBotClientDrogon.cpp
    src/clients/WorkerThreadPool.cpp
    src/notifications/EmailConfig.cpp
    src/notifications/EmailService.cpp
    src/notifications/EmailTemplate.cpp
    src/notifications/TelegramMessageTemplates.cpp
    src/services/NotificationService.cpp
)
target_include_directories(notification_service PRIVATE src ${CMAKE_SOURCE_DIR} ${JSONCPP_INCLUDE_DIR})
target_link_libraries(notification_service PRIVATE
    Drogon::Drogon ${JSONCPP_LIBRARY} CURL::libcurl iot_drogon_http_transport
)
