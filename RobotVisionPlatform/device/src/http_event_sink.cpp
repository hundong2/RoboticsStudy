#include "rv/interfaces.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <sstream>
#include <utility>

namespace rv {
namespace {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::string Escape(std::string_view value) {
  // 이 MVP JSON writer가 사용하는 문자열 필드에서 최소한의 escaping을 합니다.
  // production에서는 검증된 JSON serializer를 사용해야 합니다.
  std::string result;
  for (unsigned char ch : value) {
    switch (ch) {
      case '"':  result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n";  break;
      case '\r': result += "\\r";  break;
      case '\t': result += "\\t";  break;
      default:
        if (ch < 0x20) continue;  // drop other control characters
        result.push_back(static_cast<char>(ch));
    }
  }
  return result;
}

class HttpEventSink final : public IEventSink {
 public:
  HttpEventSink(std::string host, std::string port, std::string target)
      : host_(std::move(host)), port_(std::move(port)), target_(std::move(target)) {}

  bool Publish(const DetectionEvent& event) override {
    try {
      // MVP는 호출마다 연결합니다. production 구현은 keep-alive/HTTP2/gRPC를 재사용합니다.
      net::io_context context;
      tcp::resolver resolver(context);
      beast::tcp_stream stream(context);
      stream.expires_after(std::chrono::seconds(3));
      stream.connect(resolver.resolve(host_, port_));

      // DetectionEvent를 서버의 DetectionEventDto camelCase JSON 계약으로 변환합니다.
      std::ostringstream body;
      body << "{\"deviceId\":\"" << Escape(event.device_id) << "\",\"sequence\":"
           << event.sequence << ",\"modelVersion\":\"" << Escape(event.model_version)
           << "\",\"detections\":[";
      for (std::size_t index = 0; index < event.detections.size(); ++index) {
        const auto& item = event.detections[index];
        if (index) body << ',';
        body << "{\"label\":\"" << Escape(item.label) << "\",\"confidence\":"
             << item.confidence << ",\"x\":" << item.x << ",\"y\":" << item.y
             << ",\"width\":" << item.width << ",\"height\":" << item.height << '}';
      }
      body << "]}";

      http::request<http::string_body> request{http::verb::post, target_, 11};
      request.set(http::field::host, host_);
      request.set(http::field::user_agent, "robot-vision-device/0.1");
      request.set(http::field::content_type, "application/json");
      request.body() = body.str();
      request.prepare_payload();
      http::write(stream, request);
      beast::flat_buffer buffer;
      http::response<http::string_body> response;
      http::read(stream, buffer, response);
      beast::error_code ignored;
      stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
      // 2xx만 성공으로 계산하여 pipeline의 published counter 의미를 일관되게 유지합니다.
      return response.result_int() >= 200 && response.result_int() < 300;
    } catch (...) {
      return false;  // Production adapter adds bounded disk spool + retry/backoff.
    }
  }

 private:
  std::string host_;
  std::string port_;
  std::string target_;
};
}  // namespace

std::unique_ptr<IEventSink> MakeHttpEventSink(std::string host, std::string port,
                                              std::string target) {
  // unique_ptr로 반환해 sink 수명과 정리를 Pipeline 한 곳에서 관리합니다.
  return std::make_unique<HttpEventSink>(std::move(host), std::move(port), std::move(target));
}
}  // namespace rv
