cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_18)

# ament_cmake가 ROS 2 패키지를 colcon에 등록하고 실행 파일을 설치한다.
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(scan_simulator src/scan_simulator.cpp)
ament_target_dependencies(scan_simulator rclcpp sensor_msgs)
add_executable(bounded_grid_mapper src/bounded_grid_mapper.cpp)
ament_target_dependencies(bounded_grid_mapper rclcpp sensor_msgs nav_msgs std_msgs)
add_executable(map_auditor src/map_auditor.cpp)
ament_target_dependencies(map_auditor rclcpp nav_msgs std_msgs)

foreach(target scan_simulator bounded_grid_mapper map_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS scan_simulator bounded_grid_mapper map_auditor DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})
ament_package()
