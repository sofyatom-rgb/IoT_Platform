cmake_minimum_required(VERSION 3.16)
project(notification_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)

add_executable(notification_service
    src/main.cpp
    src/controllers/NotificationController.cpp
    src/clients/TelegramBotClient.cpp
    src/services/NotificationService.cpp
)
target_include_directories(notification_service PRIVATE src)
target_link_libraries(notification_service PRIVATE Drogon::Drogon)