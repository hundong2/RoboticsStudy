cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_25)

# ament_cmake는 colcon이 이 ROS 2 C++ 패키지를 빌드·설치하도록 연결한다.
find_package(ament_cmake REQUIRED)
# Duration은 Action의 실행 시간을 ROS 표준 sec/nanosec 형식으로 전달한다.
find_package(builtin_interfaces REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, executor API를 제공한다.
find_package(rclcpp REQUIRED)
# rclcpp_action은 Goal/Feedback/Result/Cancel 수명주기를 제공한다.
find_package(rclcpp_action REQUIRED)
# custom action/message에서 C++ 타입 지원 코드를 생성한다.
find_package(rosidl_default_generators REQUIRED)
# Header와 최종 PASS/FAIL 감사 문자열에 사용한다.
find_package(std_msgs REQUIRED)
# POSIX 제어 스레드를 표준 방식으로 링크한다.
find_package(Threads REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "action/ExecuteJointTrajectory.action"
  "msg/TrackingFrame.msg"
  DEPENDENCIES builtin_interfaces std_msgs
)
ament_export_dependencies(rosidl_default_runtime)

add_executable(bounded_trajectory_server src/bounded_trajectory_server.cpp)
ament_target_dependencies(bounded_trajectory_server builtin_interfaces rclcpp rclcpp_action std_msgs)
target_link_libraries(bounded_trajectory_server Threads::Threads)

add_executable(trajectory_goal_client src/trajectory_goal_client.cpp)
ament_target_dependencies(trajectory_goal_client builtin_interfaces rclcpp rclcpp_action)

add_executable(tracking_guard src/tracking_guard.cpp)
ament_target_dependencies(tracking_guard rclcpp std_msgs)

# 같은 패키지에서 생성한 Action/Message 헤더와 타입 지원 라이브러리를 실행 파일에 연결한다.
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
foreach(target bounded_trajectory_server trajectory_goal_client tracking_guard)
  target_link_libraries(${target} "${cpp_typesupport_target}")
  # std::array, std::atomic, structured initialization을 명시적으로 C++17에 고정한다.
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS bounded_trajectory_server trajectory_goal_client tracking_guard
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_package()
