#include "ingest/services/AdapterService.h"

#include "ingest/clients/IoTClient.h"

AdapterService::AdapterService(const std::string &iotCoreUrl)
    : AdapterService(std::make_shared<IoTClient>(iotCoreUrl)) {}
