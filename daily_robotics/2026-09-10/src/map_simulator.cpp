#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "nav_msgs/msg/occupancy_grid.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace
{
// 학습용 지도를 작게 고정하면 explorer의 최악 계산량을 손으로 검산할 수 있다.
constexpr std::uint32_t kWidth = 24U;
constexpr std::uint32_t kHeight = 20U;
constexpr double kResolutionM = 0.25;
constexpr double kOriginX = -3.0;
constexpr double kOriginY = -2.5;
}  // namespace

/**
 * @brief 로봇 한 대가 주변을 관측해 unknown(-1)을 free(0)/occupied(100)로 바꾸는 상황을 만든다.
 *
 * 실제 시스템에서는 SLAM 노드가 /map을 발행한다. 여기서는 탐사 알고리즘과 namespace를
 * 별도 센서 없이 재현하려고 결정론적인 작은 OccupancyGrid를 반복 발행한다.
 */
class MapSimulator : public rclcpp::Node
{
public:
  MapSimulator()
  : Node("map_simulator")
  {
    // declare_parameter는 launch/CLI에서 값을 덮어쓸 수 있게 기본값과 타입을 ROS graph에 등록한다.
    robot_id_ = declare_parameter<std::string>("robot_id", "robot");
    robot_x_ = declare_parameter<double>("robot_x", 0.0);
    robot_y_ = declare_parameter<double>("robot_y", 0.0);

    // map은 저주기지만 반드시 전달되어야 하고, 늦게 시작한 explorer도 마지막 지도를 받아야 한다.
    // Reliable + Transient Local(depth 1)은 ROS 1의 latched map과 비슷한 의도를 표현한다.
    auto map_qos = rclcpp::QoS(rclcpp::KeepLast(1));
    map_qos.reliable().transient_local();
    map_publisher_ = create_publisher<nav_msgs::msg::OccupancyGrid>("local_map", map_qos);

    // OccupancyGrid::data는 std::vector이므로 시작 시 딱 한 번 크기를 잡는다.
    // 타이머마다 resize하지 않아 지도 생성 루프에서 불필요한 재할당이 생기지 않게 한다.
    map_.data.resize(static_cast<std::size_t>(kWidth) * kHeight, -1);
    map_.header.frame_id = "map";
    map_.info.resolution = static_cast<float>(kResolutionM);
    map_.info.width = kWidth;
    map_.info.height = kHeight;
    map_.info.origin.position.x = kOriginX;
    map_.info.origin.position.y = kOriginY;
    // 단위 quaternion (x,y,z,w)=(0,0,0,1)은 map 축이 회전되지 않았음을 뜻한다.
    map_.info.origin.orientation.w = 1.0;

    // create_wall_timer는 ROS executor가 750 ms마다 callback을 준비시키는 API다.
    // 센서 시뮬레이터는 hard RT 대상이 아니므로 읽기 쉬운 저주기 값을 사용한다.
    timer_ = create_wall_timer(750ms, std::bind(&MapSimulator::publish_map, this));

    RCLCPP_INFO(
      get_logger(), "%s map simulator: relative topic='local_map', namespace='%s'",
      robot_id_.c_str(), get_namespace());
  }

private:
  /** 로봇 주변의 관측 반경을 조금씩 넓혀 새로운 frontier가 생기는 지도를 발행한다. */
  void publish_map()
  {
    // tick이 6 이상이면 반경을 고정한다. 입력과 반복 횟수가 고정되어 매 실행이 재현 가능하다.
    const double reveal_radius_m = 1.25 + 0.22 * static_cast<double>(std::min(tick_, 6U));

    for (std::uint32_t y = 0U; y < kHeight; ++y) {
      for (std::uint32_t x = 0U; x < kWidth; ++x) {
        // cell 중심의 세계 좌표: p_world = origin + resolution * (cell + 1/2).
        const double world_x = kOriginX + (static_cast<double>(x) + 0.5) * kResolutionM;
        const double world_y = kOriginY + (static_cast<double>(y) + 0.5) * kResolutionM;
        const double distance_m = std::hypot(world_x - robot_x_, world_y - robot_y_);
        const std::size_t index = static_cast<std::size_t>(y) * kWidth + x;

        if (distance_m > reveal_radius_m) {
          // OccupancyGrid에서 -1은 아직 관측하지 못한 unknown 영역이다.
          map_.data[index] = -1;
          continue;
        }

        // 가운데 벽과 두 기둥을 넣되 통로를 남겨, frontier가 여러 cluster로 갈라지게 만든다.
        const bool center_wall = x == 12U && (y < 8U || y > 11U);
        const bool left_pillar = (x == 7U || x == 8U) && (y == 13U || y == 14U);
        const bool right_pillar = (x == 17U || x == 18U) && (y == 5U || y == 6U);
        map_.data[index] = (center_wall || left_pillar || right_pillar) ? 100 : 0;
      }
    }

    // now()는 이 노드가 사용하는 ROS clock의 현재 시각이다. 소비자가 지도 freshness를 판단할 수 있다.
    map_.header.stamp = now();
    // publish는 같은 소스 코드를 namespace별로 띄우면 /robot_1/map, /robot_2/map으로 분리된다.
    map_publisher_->publish(map_);

    if (tick_ % 4U == 0U) {
      RCLCPP_INFO(
        get_logger(), "%s map published: reveal_radius=%.2f m, cells=%u",
        robot_id_.c_str(), reveal_radius_m, kWidth * kHeight);
    }
    ++tick_;
  }

  std::string robot_id_;
  double robot_x_{0.0};
  double robot_y_{0.0};
  std::uint32_t tick_{0U};
  nav_msgs::msg::OccupancyGrid map_;
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr map_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 DDS/RMW, signal handler, ROS argument 처리를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 subscription/timer callback을 실행한다. 이 노드는 callback 하나라 단일 executor로 충분하다.
  rclcpp::spin(std::make_shared<MapSimulator>());
  // shutdown은 ROS context와 middleware 자원을 명시적으로 정리한다.
  rclcpp::shutdown();
  return 0;
}
