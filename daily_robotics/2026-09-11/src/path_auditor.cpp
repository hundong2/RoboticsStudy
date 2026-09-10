#include <cmath>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{
// Path의 첫 pose와 마지막 pose 사이 Euclidean 거리를 loop closure gap으로 정의한다.
double path_gap(const nav_msgs::msg::Path & path)
{
  if (path.poses.size() < 2U) {
    return 0.0;
  }
  const auto & first = path.poses.front().pose.position;
  const auto & last = path.poses.back().pose.position;
  return std::hypot(last.x - first.x, last.y - first.y);
}
}  // namespace

// 이 노드는 raw/optimized Path를 모두 읽어 drift 감소 여부를 독립적으로 검증한다.
// 실제 배포에서는 read-only 보안 enclave로 두어 optimizer가 자기 성적을 조작하지 못하게 분리할 수 있다.
class PathAuditor : public rclcpp::Node
{
public:
  PathAuditor()
  : Node("path_auditor")
  {
    const auto graph_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    using std::placeholders::_1;

    raw_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "slam/raw_path", graph_qos,
      std::bind(&PathAuditor::on_raw_path, this, _1));
    optimized_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "slam/optimized_path", graph_qos,
      std::bind(&PathAuditor::on_optimized_path, this, _1));

    // audit topic은 운영자/CI가 읽는 결과이며 optimizer 입력으로 되돌아가지 않는다.
    audit_publisher_ = create_publisher<std_msgs::msg::String>(
      "slam/audit", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
    RCLCPP_INFO(get_logger(), "independent path auditor ready");
  }

private:
  // raw odometry의 closure gap을 저장하고 두 입력이 모두 준비되면 비교 결과를 발행한다.
  void on_raw_path(const nav_msgs::msg::Path::SharedPtr message)
  {
    raw_gap_m_ = path_gap(*message);
    has_raw_ = true;
    publish_if_ready();
  }

  // graph optimization 이후 closure gap을 저장하고 독립 검증을 시도한다.
  void on_optimized_path(const nav_msgs::msg::Path::SharedPtr message)
  {
    optimized_gap_m_ = path_gap(*message);
    has_optimized_ = true;
    publish_if_ready();
  }

  // 최적화 gap이 raw gap의 20% 미만이면 이 결정론적 데모의 회귀 검사를 통과시킨다.
  void publish_if_ready()
  {
    if (!has_raw_ || !has_optimized_) {
      return;
    }
    const bool improved = optimized_gap_m_ < raw_gap_m_;
    const bool pass = raw_gap_m_ > 1e-9 && optimized_gap_m_ < 0.20 * raw_gap_m_;

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4)
           << (pass ? "PASS" : "FAIL")
           << " raw_gap_m=" << raw_gap_m_
           << " optimized_gap_m=" << optimized_gap_m_
           << " reduction_percent="
           << (raw_gap_m_ > 1e-9 ? 100.0 * (1.0 - optimized_gap_m_ / raw_gap_m_) : 0.0)
           << " improved=" << std::boolalpha << improved;

    std_msgs::msg::String audit;
    audit.data = stream.str();
    audit_publisher_->publish(audit);
    // Auditor는 hard-RT 경로가 아니므로 동일 판정문을 로그에도 남겨 secure launch에서 검증 가능하게 한다.
    RCLCPP_INFO(get_logger(), "%s", audit.data.c_str());
  }

  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr raw_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr optimized_subscription_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_publisher_;
  bool has_raw_{false};
  bool has_optimized_{false};
  double raw_gap_m_{0.0};
  double optimized_gap_m_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PathAuditor>());
  rclcpp::shutdown();
  return 0;
}
