cmake_minimum_required(VERSION 3.5)
project(http_adapter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(http_adapter main.cc)

target_link_libraries(http_adapter PRIVATE 
    Drogon::Drogon
    httplib::httplib
    OpenSSL::SSL 
    OpenSSL::Crypto
)
