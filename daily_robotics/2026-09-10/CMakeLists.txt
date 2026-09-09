cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_10)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package-index 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, Executor 같은 ROS 2 C++ 핵심 API를 제공한다.
find_package(rclcpp REQUIRED)
# nav_msgs의 OccupancyGrid는 SLAM/탐사에서 사용하는 2차원 점유 격자 표준 메시지다.
find_package(nav_msgs REQUIRED)
# geometry_msgs의 PoseStamped로 선택된 frontier의 지도 좌표와 기준 시각을 전달한다.
find_package(geometry_msgs REQUIRED)
# std_msgs의 String은 계산 hot path 밖에서 사람이 읽는 진단 상태를 전달하는 데 사용한다.
find_package(std_msgs REQUIRED)

add_executable(map_simulator src/map_simulator.cpp)
ament_target_dependencies(map_simulator rclcpp nav_msgs)

add_executable(bounded_frontier_explorer src/bounded_frontier_explorer.cpp)
ament_target_dependencies(bounded_frontier_explorer rclcpp nav_msgs geometry_msgs std_msgs)

add_executable(fleet_coordinator src/fleet_coordinator.cpp)
ament_target_dependencies(fleet_coordinator rclcpp geometry_msgs std_msgs)

# std::array, std::clamp, lambda capture 등 예제에서 사용하는 문법을 명확히 C++17로 고정한다.
foreach(target map_simulator bounded_frontier_explorer fleet_coordinator)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  map_simulator
  bounded_frontier_explorer
  fleet_coordinator
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch가 install 공간에서도 Python launch 파일을 찾을 수 있게 복사한다.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
