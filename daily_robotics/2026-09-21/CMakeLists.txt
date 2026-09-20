cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_21)

# ament_cmake와 오늘 사용하는 ROS 2 메시지/TF 패키지를 명시적으로 찾는다.
find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

add_executable(moving_scan_simulator src/moving_scan_simulator.cpp)
ament_target_dependencies(moving_scan_simulator geometry_msgs rclcpp sensor_msgs tf2_ros)

add_executable(motion_compensated_mapper src/motion_compensated_mapper.cpp)
ament_target_dependencies(motion_compensated_mapper geometry_msgs nav_msgs rclcpp sensor_msgs std_msgs tf2 tf2_ros)

add_executable(map_quality_auditor src/map_quality_auditor.cpp)
ament_target_dependencies(map_quality_auditor nav_msgs rclcpp std_msgs)

foreach(target moving_scan_simulator motion_compensated_mapper map_quality_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  moving_scan_simulator
  motion_compensated_mapper
  map_quality_auditor
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_package()
