#include "ingest/clients/IoTClient.h"

#include "common/http/DrogonHttpTransport.h"

IoTClient::IoTClient(const std::string &baseUrl)
    : IoTClient(std::make_shared<iot::http::DrogonHttpTransport>(baseUrl)) {}
