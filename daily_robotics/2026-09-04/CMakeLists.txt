cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_04)

# ament_cmake는 ROS 2 C++ 패키지의 빌드·설치·패키지 인덱스 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# 목표점과 순기구학 검증점은 geometry_msgs/PointStamped로 교환한다.
find_package(geometry_msgs REQUIRED)
# 노드, Publisher/Subscription, WaitSet API를 제공한다.
find_package(rclcpp REQUIRED)
# robot_state_publisher가 소비하는 sensor_msgs/JointState를 사용한다.
find_package(sensor_msgs REQUIRED)

add_executable(circular_target_publisher src/circular_target_publisher.cpp)
ament_target_dependencies(circular_target_publisher geometry_msgs rclcpp)

add_executable(ik_waitset_controller src/ik_waitset_controller.cpp)
ament_target_dependencies(ik_waitset_controller geometry_msgs rclcpp sensor_msgs)

# std::optional과 구조화된 최신 타입 사용을 위해 C++17을 명시한다.
target_compile_features(circular_target_publisher PUBLIC cxx_std_17)
target_compile_features(ik_waitset_controller PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(circular_target_publisher PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(ik_waitset_controller PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(
  TARGETS circular_target_publisher ik_waitset_controller
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch와 robot_state_publisher가 설치 공간에서도 파일을 찾도록 함께 설치한다.
install(DIRECTORY launch urdf DESTINATION share/${PROJECT_NAME})

ament_package()
