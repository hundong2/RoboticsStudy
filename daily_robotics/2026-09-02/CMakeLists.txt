cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_02)

# ROS 2의 표준 CMake 확장과 오늘 실습에서 사용하는 메시지 패키지를 찾는다.
find_package(ament_cmake REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)

add_executable(noisy_odometry_publisher src/noisy_odometry_publisher.cpp)
ament_target_dependencies(noisy_odometry_publisher rclcpp nav_msgs)

add_executable(rt_kalman_fusion src/rt_kalman_fusion.cpp)
ament_target_dependencies(rt_kalman_fusion rclcpp nav_msgs)

# C++17은 std::array, std::atomic과 chrono 리터럴을 명확히 사용할 수 있는 기준이다.
target_compile_features(noisy_odometry_publisher PUBLIC cxx_std_17)
target_compile_features(rt_kalman_fusion PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(noisy_odometry_publisher PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(rt_kalman_fusion PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS
  noisy_odometry_publisher
  rt_kalman_fusion
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
