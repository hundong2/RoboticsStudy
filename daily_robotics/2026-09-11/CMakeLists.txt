cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_11)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package-index 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, Executor 같은 ROS 2 C++ 핵심 API를 제공한다.
find_package(rclcpp REQUIRED)
# nav_msgs/Path는 시간순 로봇 pose 열을 표준 형식으로 전달하므로 pose graph 입력/출력에 사용한다.
find_package(nav_msgs REQUIRED)
# geometry_msgs/PoseStamped는 Path 내부의 각 SE(2) keyframe pose를 표현한다.
find_package(geometry_msgs REQUIRED)
# std_msgs/String은 최적화 및 감사 결과를 사람이 읽는 저주기 진단 topic으로 보낸다.
find_package(std_msgs REQUIRED)

add_executable(path_simulator src/path_simulator.cpp)
ament_target_dependencies(path_simulator rclcpp nav_msgs geometry_msgs)

add_executable(bounded_pose_graph_optimizer src/bounded_pose_graph_optimizer.cpp)
ament_target_dependencies(bounded_pose_graph_optimizer rclcpp nav_msgs geometry_msgs std_msgs)

add_executable(path_auditor src/path_auditor.cpp)
ament_target_dependencies(path_auditor rclcpp nav_msgs std_msgs)

# std::array, std::clamp, 구조적 초기화 등 예제에서 쓰는 문법을 C++17로 고정한다.
foreach(target path_simulator bounded_pose_graph_optimizer path_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  path_simulator
  bounded_pose_graph_optimizer
  path_auditor
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch와 설치된 패키지 문서에서 launch/security 자료를 찾을 수 있게 복사한다.
install(DIRECTORY launch security DESTINATION share/${PROJECT_NAME})

ament_package()
