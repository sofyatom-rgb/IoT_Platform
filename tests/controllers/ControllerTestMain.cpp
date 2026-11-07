#include "TestFramework.h"

#include <drogon/drogon.h>

int main() {
    // Controller tests intentionally submit malformed JSON in several cases.
    // Drogon logs the expected parser diagnostics at DEBUG level; hide those
    // messages while preserving warnings and real errors in the test output.
    drogon::app().setLogLevel(trantor::Logger::kWarn);

    return test::runAll();
}
