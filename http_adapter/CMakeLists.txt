find_package(OpenSSL REQUIRED)

add_executable(http_adapter
    src/main.cpp
    src/ingest/controllers/IngestController.cpp
    src/ingest/services/AdapterService.cpp
    src/ingest/clients/IoTClient.cpp
    src/codec/Base64.cpp
    src/codec/Checksum.cpp
    src/codec/Encoder.cpp
    src/codec/Decoder.cpp
)
target_include_directories(http_adapter PRIVATE src)
set_target_properties(http_adapter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(http_adapter PRIVATE Drogon::Drogon OpenSSL::Crypto)
# Ищем библиотеку jsoncpp и её заголовки
find_library(JSONCPP_LIBRARY NAMES jsoncpp)
find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATH_SUFFIXES jsoncpp)

# Тестовая утилита
add_executable(codec_cli
    tools/codec_cli.cpp
    src/codec/Base64.cpp
    src/codec/Checksum.cpp
    src/codec/Encoder.cpp
    src/codec/Decoder.cpp
)
target_include_directories(codec_cli PRIVATE 
    src
    ${JSONCPP_INCLUDE_DIR} # <-- Добавляем путь к jsoncpp
)
set_target_properties(codec_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Линкуем с OpenSSL и найденной библиотекой jsoncpp
target_link_libraries(codec_cli PRIVATE OpenSSL::Crypto ${JSONCPP_LIBRARY})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/AdapterConfig.json
    ${CMAKE_BINARY_DIR}/bin/AdapterConfig.json
    COPYONLY
)