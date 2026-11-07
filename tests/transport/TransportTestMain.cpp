#include "TestFramework.h"

#include <chrono>
#include <future>
#include <thread>

#include <drogon/drogon.h>

int main() {
  auto &application = drogon::app();
  // Keep expected/benign framework DEBUG diagnostics out of test output.
  // Warnings and errors remain visible.
  application.setLogLevel(trantor::Logger::kWarn);

  std::promise<void> loopStarted;
  auto started = loopStarted.get_future();

  // HttpClient uses Drogon's application event loop by default. Run it on a
  // background thread while the test runner waits for transport callbacks.
  application.getLoop()->queueInLoop([&loopStarted] { loopStarted.set_value(); });
  std::thread eventLoop([&application] { application.run(); });

  if (started.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    application.quit();
    eventLoop.join();
    return 2;
  }

  const int result = test::runAll();
  application.quit();
  eventLoop.join();
  return result;
}
