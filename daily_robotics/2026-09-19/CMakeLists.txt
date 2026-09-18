cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_19)

# ament/colcon이 ROS 2 실행 파일과 의존 메시지 타입을 연결한다.
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(trajectory_msgs REQUIRED)

add_executable(goal_source src/goal_source.cpp)
ament_target_dependencies(goal_source rclcpp std_msgs)
add_executable(jerk_limited_planner src/jerk_limited_planner.cpp)
ament_target_dependencies(jerk_limited_planner rclcpp std_msgs trajectory_msgs)
add_executable(trajectory_auditor src/trajectory_auditor.cpp)
ament_target_dependencies(trajectory_auditor rclcpp std_msgs trajectory_msgs)

foreach(target goal_source jerk_limited_planner trajectory_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS goal_source jerk_limited_planner trajectory_auditor DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})
ament_package()
