cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_07)

# ament_cmake는 ROS 2 C++ 패키지의 빌드, 설치, 패키지 인덱스 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# geometry_msgs는 DWA의 원시/안전 속도 명령과 모의 Odometry twist를 제공한다.
find_package(geometry_msgs REQUIRED)
# nav_msgs는 현재 선속도·각속도를 담는 Odometry 메시지를 제공한다.
find_package(nav_msgs REQUIRED)
# rcl_interfaces는 동적 파라미터 검증 결과(SetParametersResult) 타입을 제공한다.
find_package(rcl_interfaces REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Parameter, Timer 등 ROS 2 C++ 핵심 API다.
find_package(rclcpp REQUIRED)
# rclcpp_components는 노드를 공유 라이브러리로 등록하고 한 프로세스에 조합한다.
find_package(rclcpp_components REQUIRED)
# sensor_msgs는 2D Lidar 스캔을 표현하는 LaserScan 타입을 제공한다.
find_package(sensor_msgs REQUIRED)

# 세 노드를 하나의 공유 라이브러리로 만들면 component_container가 런타임에 로드할 수 있다.
add_library(robotics_components SHARED
  src/sensor_simulator_component.cpp
  src/dwa_planner_component.cpp
  src/command_guard_component.cpp
)
ament_target_dependencies(
  robotics_components
  geometry_msgs
  nav_msgs
  rcl_interfaces
  rclcpp
  rclcpp_components
  sensor_msgs
)

# std::array, std::clamp, 구조화 바인딩 대신 명시적 C++17 기능 집합을 사용한다.
target_compile_features(robotics_components PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(robotics_components PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 각 클래스 이름을 ROS 2 plugin index에 등록한다. launch의 plugin 문자열과 정확히 같아야 한다.
rclcpp_components_register_nodes(robotics_components
  "daily_robotics_2026_09_07::SensorSimulatorComponent"
  "daily_robotics_2026_09_07::DwaPlannerComponent"
  "daily_robotics_2026_09_07::CommandGuardComponent"
)

install(
  TARGETS robotics_components
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
# ros2 launch가 설치 공간에서도 launch 파일을 찾도록 함께 설치한다.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
