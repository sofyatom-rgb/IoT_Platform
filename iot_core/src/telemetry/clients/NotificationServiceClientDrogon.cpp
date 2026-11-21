#include "telemetry/clients/NotificationServiceClient.h"

#include "common/http/DrogonHttpTransport.h"

NotificationServiceClient::NotificationServiceClient(std::string baseUrl)
    : NotificationServiceClient(
          baseUrl, std::make_shared<iot::http::DrogonHttpTransport>(baseUrl)) {}
