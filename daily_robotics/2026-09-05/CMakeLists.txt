cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_05)

# ament_cmake는 ROS 2 C++ 패키지의 빌드·설치·패키지 인덱스 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# geometry_msgs/Twist는 이동 베이스 속도 명령, Pose2D는 ICP 상대 자세 결과에 사용한다.
find_package(geometry_msgs REQUIRED)
# nav_msgs/Odometry는 모의 이동 베이스의 위치 피드백에 사용한다.
find_package(nav_msgs REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, spin 등 ROS 2 C++ 핵심 API를 제공한다.
find_package(rclcpp REQUIRED)
# rclcpp_action은 Goal·Feedback·Result·Cancel을 갖는 장시간 작업 API를 제공한다.
find_package(rclcpp_action REQUIRED)
# rosidl_default_generators는 아래 DriveDistance.action을 C++ 타입으로 생성한다.
find_package(rosidl_default_generators REQUIRED)
# sensor_msgs/LaserScan은 2D 라이다 스캔 입력을 표현한다.
find_package(sensor_msgs REQUIRED)
# std::thread와 pthread_setschedparam을 이식성 있게 링크한다.
find_package(Threads REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "action/DriveDistance.action"
)
ament_export_dependencies(rosidl_default_runtime)

add_executable(drive_action_server src/drive_action_server.cpp)
ament_target_dependencies(
  drive_action_server geometry_msgs nav_msgs rclcpp rclcpp_action
)
target_link_libraries(drive_action_server Threads::Threads)

add_executable(drive_action_client src/drive_action_client.cpp)
ament_target_dependencies(drive_action_client rclcpp rclcpp_action)

add_executable(mock_mobile_base src/mock_mobile_base.cpp)
ament_target_dependencies(mock_mobile_base geometry_msgs nav_msgs rclcpp)

add_executable(scan_simulator src/scan_simulator.cpp)
ament_target_dependencies(scan_simulator rclcpp sensor_msgs)

add_executable(icp_scan_matcher src/icp_scan_matcher.cpp)
ament_target_dependencies(icp_scan_matcher geometry_msgs rclcpp sensor_msgs)

# 같은 패키지에서 생성한 Action 헤더와 타입 지원 라이브러리를 두 Action 노드에 연결한다.
rosidl_get_typesupport_target(
  cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp"
)
target_link_libraries(drive_action_server "${cpp_typesupport_target}")
target_link_libraries(drive_action_client "${cpp_typesupport_target}")

# std::array, std::atomic, 구조화 바인딩 등 명시적인 C++17 기능 집합을 사용한다.
set(study_targets
  drive_action_server
  drive_action_client
  mock_mobile_base
  scan_simulator
  icp_scan_matcher
)
foreach(target IN LISTS study_targets)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS ${study_targets} DESTINATION lib/${PROJECT_NAME})
# ros2 launch와 ros2 interface show가 설치 공간에서 파일을 찾도록 함께 설치한다.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
