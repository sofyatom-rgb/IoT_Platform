add_executable(http_adapter
    src/main.cpp
    src/ingest/controllers/IngestController.cpp
    src/ingest/services/AdapterService.cpp
    src/ingest/clients/IoTClient.cpp
)

target_include_directories(http_adapter PRIVATE src)
set_target_properties(http_adapter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(http_adapter PRIVATE Drogon::Drogon)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/AdapterConfig.json 
    ${CMAKE_BINARY_DIR}/bin/AdapterConfig.json 
    COPYONLY
)