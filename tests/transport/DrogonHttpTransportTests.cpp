#include "TestFramework.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <drogon/drogon.h>

#include "common/http/DrogonHttpTransport.h"
#include "ingest/clients/IoTClient.h"
#include "ingest/services/AdapterService.h"

namespace {
class OneShotHttpServer {
 public:
  explicit OneShotHttpServer(
      int statusCode = 200,
      std::string responseBody = R"({"accepted":true})")
      : statusCode_(statusCode), responseBody_(std::move(responseBody)) {
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) throw std::runtime_error("socket() failed");

    int reuse = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(listenFd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
      ::close(listenFd_);
      throw std::runtime_error("bind() failed");
    }
    if (::listen(listenFd_, 1) < 0) {
      ::close(listenFd_);
      throw std::runtime_error("listen() failed");
    }

    socklen_t length = sizeof(address);
    if (::getsockname(listenFd_, reinterpret_cast<sockaddr *>(&address), &length) < 0) {
      ::close(listenFd_);
      throw std::runtime_error("getsockname() failed");
    }
    port_ = ntohs(address.sin_port);
    worker_ = std::thread([this] { serveOneRequest(); });
  }

  ~OneShotHttpServer() {
    // serveOneRequest() uses poll() with a finite timeout, so a failed client
    // cannot leave the test process blocked forever in accept().
    if (worker_.joinable()) worker_.join();
    if (listenFd_ >= 0) ::close(listenFd_);
  }

  unsigned short port() const noexcept { return port_; }

  std::string requestText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return requestText_;
  }

 private:
  void serveOneRequest() {
    pollfd listener{listenFd_, POLLIN, 0};
    const int ready = ::poll(&listener, 1, 5000);
    if (ready <= 0 || (listener.revents & POLLIN) == 0) return;

    const int client = ::accept(listenFd_, nullptr, nullptr);
    if (client < 0) return;
    timeval receiveTimeout{5, 0};
    (void)::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                       &receiveTimeout, sizeof(receiveTimeout));

    std::string received;
    char buffer[4096];
    std::size_t expectedBody = 0;
    std::size_t headerEnd = std::string::npos;
    while (true) {
      const auto count = ::recv(client, buffer, sizeof(buffer), 0);
      if (count <= 0) break;
      received.append(buffer, static_cast<std::size_t>(count));

      if (headerEnd == std::string::npos) {
        headerEnd = received.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
          std::string headers = received.substr(0, headerEnd);
          std::string lowered = headers;
          std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                         [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
          const auto contentLengthPos = lowered.find("content-length:");
          if (contentLengthPos != std::string::npos) {
            const auto start = contentLengthPos + std::string("content-length:").size();
            const auto end = lowered.find("\r\n", start);
            expectedBody = static_cast<std::size_t>(
                std::stoul(lowered.substr(start, end - start)));
          }
        }
      }
      if (headerEnd != std::string::npos &&
          received.size() >= headerEnd + 4 + expectedBody) {
        break;
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      requestText_ = received;
    }

    const std::string reason = statusCode_ == 200 ? "OK" : "Service Unavailable";
    const std::string response =
        "HTTP/1.1 " + std::to_string(statusCode_) + " " + reason +
        "\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(responseBody_.size()) +
        "\r\nConnection: close\r\n\r\n" + responseBody_;
    std::size_t sent = 0;
    while (sent < response.size()) {
      const auto count = ::send(client, response.data() + sent,
                                response.size() - sent, MSG_NOSIGNAL);
      if (count <= 0) break;
      sent += static_cast<std::size_t>(count);
    }
    ::shutdown(client, SHUT_RDWR);
    ::close(client);
  }

  int listenFd_{-1};
  unsigned short port_{0};
  int statusCode_{200};
  std::string responseBody_;
  std::thread worker_;
  mutable std::mutex mutex_;
  std::string requestText_;
};

void expectHttpMethod(iot::http::Method method, const std::string &expectedLine) {
  OneShotHttpServer server;
  iot::http::DrogonHttpTransport transport(
      "http://127.0.0.1:" + std::to_string(server.port()));
  iot::http::Request request;
  request.method = method;
  request.path = "/method-check";
  request.jsonBody = {{"value", std::int64_t(1)}};

  auto promise = std::make_shared<std::promise<bool>>();
  auto future = promise->get_future();
  transport.send(request, [promise](iot::http::Result result,
                                    std::optional<iot::http::Response> response) {
    promise->set_value(result == iot::http::Result::Ok && response &&
                       response->statusCode == 200);
  });
  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    throw std::runtime_error("Drogon HTTP method check timed out");
  }
  EXPECT_TRUE(future.get());
  EXPECT_TRUE(server.requestText().find(expectedLine) == 0);
}
}  // namespace

TEST_CASE(drogon_http_transport_sends_real_post_and_parses_json_response) {
  OneShotHttpServer server;
  iot::http::DrogonHttpTransport transport(
      "http://127.0.0.1:" + std::to_string(server.port()));

  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/transport-test";
  request.jsonBody = {
      {"device_id", std::int64_t(7)},
      {"temperature", 21.5},
      {"active", true},
      {"label", std::string("lab")}
  };

  auto promise = std::make_shared<std::promise<std::pair<iot::http::Result,
                                                        std::optional<iot::http::Response>>>>();
  auto future = promise->get_future();
  transport.send(request, [promise](iot::http::Result result,
                                    std::optional<iot::http::Response> response) {
    promise->set_value({result, std::move(response)});
  });

  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    throw std::runtime_error("Drogon HTTP request timed out");
  }
  auto [result, response] = future.get();
  if (server.requestText().empty()) {
    // The callback fires after the server sends its response; give the server
    // thread a moment to finish copying the captured request.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (server.requestText().empty()) {
    throw std::runtime_error("Local test server did not capture the HTTP request");
  }
  const std::string rawRequest = server.requestText();

  EXPECT_EQ(iot::http::Result::Ok, result);
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(200, response->statusCode);
  EXPECT_TRUE(response->jsonParsed);
  EXPECT_TRUE(iot::http::boolField(response->jsonBody, "accepted") != nullptr);
  EXPECT_TRUE(*iot::http::boolField(response->jsonBody, "accepted"));
  EXPECT_TRUE(rawRequest.find("POST /transport-test HTTP/1.1") != std::string::npos);
  EXPECT_TRUE(rawRequest.find("content-type: application/json") != std::string::npos ||
              rawRequest.find("Content-Type: application/json") != std::string::npos);
  EXPECT_TRUE(rawRequest.find("\"device_id\":7") != std::string::npos);
  EXPECT_TRUE(rawRequest.find("\"temperature\":21.5") != std::string::npos);
}


TEST_CASE(drogon_http_transport_maps_get_method_to_wire_request) {
  expectHttpMethod(iot::http::Method::Get, "GET /method-check HTTP/1.1");
}

TEST_CASE(drogon_http_transport_maps_put_method_to_wire_request) {
  expectHttpMethod(iot::http::Method::Put, "PUT /method-check HTTP/1.1");
}

TEST_CASE(drogon_http_transport_maps_delete_method_to_wire_request) {
  expectHttpMethod(iot::http::Method::Delete, "DELETE /method-check HTTP/1.1");
}


TEST_CASE(drogon_http_transport_preserves_non_200_http_status) {
  OneShotHttpServer server(503, R"({"error":"temporarily unavailable"})");
  iot::http::DrogonHttpTransport transport(
      "http://127.0.0.1:" + std::to_string(server.port()));
  iot::http::Request request;
  request.method = iot::http::Method::Get;
  request.path = "/status-check";

  auto promise = std::make_shared<std::promise<std::pair<iot::http::Result,
                                                        std::optional<iot::http::Response>>>>();
  auto future = promise->get_future();
  transport.send(request, [promise](iot::http::Result result,
                                    std::optional<iot::http::Response> response) {
    promise->set_value({result, std::move(response)});
  });
  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    throw std::runtime_error("Drogon HTTP status check timed out");
  }
  auto [result, response] = future.get();
  EXPECT_EQ(iot::http::Result::Ok, result);
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(503, response->statusCode);
  EXPECT_TRUE(response->jsonParsed);
  const auto *message = iot::http::stringField(response->jsonBody, "error");
  EXPECT_TRUE(message != nullptr);
  EXPECT_EQ(std::string("temporarily unavailable"), *message);
}

TEST_CASE(drogon_http_transport_keeps_body_when_response_is_not_json) {
  OneShotHttpServer server(200, "not-json");
  iot::http::DrogonHttpTransport transport(
      "http://127.0.0.1:" + std::to_string(server.port()));
  iot::http::Request request;
  request.method = iot::http::Method::Get;
  request.path = "/plain-response";

  auto promise = std::make_shared<std::promise<std::optional<iot::http::Response>>>();
  auto future = promise->get_future();
  transport.send(request, [promise](iot::http::Result,
                                    std::optional<iot::http::Response> response) {
    promise->set_value(std::move(response));
  });
  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    throw std::runtime_error("Drogon non-JSON response check timed out");
  }
  auto response = future.get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(std::string("not-json"), response->body);
  EXPECT_FALSE(response->jsonParsed);
  EXPECT_FALSE(response->jsonParseError.empty());
}

TEST_CASE(drogon_http_transport_reports_empty_base_url_without_network_call) {
  iot::http::DrogonHttpTransport transport("");
  iot::http::Request request;
  request.method = iot::http::Method::Get;
  request.path = "/should-not-be-sent";

  bool called = false;
  transport.send(request, [&](iot::http::Result result,
                              std::optional<iot::http::Response> response) {
    called = true;
    EXPECT_EQ(iot::http::Result::NetworkError, result);
    EXPECT_FALSE(response.has_value());
  });
  EXPECT_TRUE(called);
}


TEST_CASE(adapter_service_iot_client_and_drogon_transport_reach_local_http_server) {
  OneShotHttpServer server(200, R"({"accepted":true})");
  auto transport = std::make_shared<iot::http::DrogonHttpTransport>(
      "http://127.0.0.1:" + std::to_string(server.port()));
  auto client = std::make_shared<IoTClient>(transport);
  AdapterService service(client);

  auto completion = std::make_shared<std::promise<std::pair<bool, std::string>>>();
  auto future = completion->get_future();
  service.processTelemetry(
      31, 18.25,
      [completion] { completion->set_value({true, {}}); },
      [completion](const std::string &error) {
        completion->set_value({false, error});
      });

  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    throw std::runtime_error("Adapter-to-HTTP pipeline timed out");
  }

  const auto [success, error] = future.get();
  const auto rawRequest = server.requestText();
  EXPECT_TRUE(success);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(rawRequest.find("POST /telemetry HTTP/1.1") != std::string::npos);
  EXPECT_TRUE(rawRequest.find("\"device_id\":31") != std::string::npos);
  EXPECT_TRUE(rawRequest.find("\"temperature\":18.25") != std::string::npos);
}
