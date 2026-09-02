cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_03)

# ROS 2 빌드 시스템과 오늘 실습에서 사용하는 메시지/TF 패키지를 찾는다.
find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)

add_executable(tf_goal_source src/tf_goal_source.cpp)
ament_target_dependencies(
  tf_goal_source
  geometry_msgs
  rclcpp
  tf2
  tf2_ros
)

add_executable(deterministic_astar_planner src/deterministic_astar_planner.cpp)
ament_target_dependencies(
  deterministic_astar_planner
  geometry_msgs
  nav_msgs
  rclcpp
  std_msgs
  tf2
  tf2_geometry_msgs
  tf2_ros
)

# std::array, std::optional, 구조적 바인딩 없이도 일관된 최신 컴파일 환경을 쓰기 위해 C++17을 지정한다.
target_compile_features(tf_goal_source PUBLIC cxx_std_17)
target_compile_features(deterministic_astar_planner PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(tf_goal_source PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(deterministic_astar_planner PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(
  TARGETS
    tf_goal_source
    deterministic_astar_planner
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch가 설치 공간에서 launch 파일을 찾을 수 있도록 함께 설치한다.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
