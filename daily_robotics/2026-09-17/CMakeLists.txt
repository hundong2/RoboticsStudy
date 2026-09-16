cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_17)

# ament_cmake는 colcon이 패키지를 찾고 실행 파일을 설치하도록 돕는다.
find_package(ament_cmake REQUIRED)
# rclcpp는 ROS 2 노드·Topic·QoS·Executor API를 제공한다.
find_package(rclcpp REQUIRED)
find_package(rosgraph_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(imu_simulator src/imu_simulator.cpp)
ament_target_dependencies(imu_simulator rclcpp rosgraph_msgs sensor_msgs)
add_executable(imu_preintegrator src/imu_preintegrator.cpp)
ament_target_dependencies(imu_preintegrator rclcpp sensor_msgs nav_msgs std_msgs)
add_executable(integration_auditor src/integration_auditor.cpp)
ament_target_dependencies(integration_auditor rclcpp nav_msgs std_msgs)

foreach(target imu_simulator imu_preintegrator integration_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS imu_simulator imu_preintegrator integration_auditor
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})
ament_package()
