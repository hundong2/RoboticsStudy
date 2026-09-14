cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_15)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package-index 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# rclcpp는 Node, Topic, Timer, QoS, CallbackGroup, Executor API를 제공한다.
find_package(rclcpp REQUIRED)
# rosidl은 오늘 사용할 JointSpaceWorld 사용자 정의 메시지를 C++ 타입으로 생성한다.
find_package(rosidl_default_generators REQUIRED)
# 표준 관절 상태와 시간 매개변수화된 관절 궤적 메시지를 사용한다.
find_package(sensor_msgs REQUIRED)
find_package(trajectory_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
# controller_demo의 두 executor를 std::jthread로 실행할 때 pthread 연결을 이식성 있게 처리한다.
find_package(Threads REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/JointSpaceWorld.msg"
  DEPENDENCIES geometry_msgs std_msgs
)

add_executable(joint_space_world src/joint_space_world.cpp)
ament_target_dependencies(joint_space_world rclcpp geometry_msgs std_msgs)

add_executable(bounded_rrt_connect src/bounded_rrt_connect.cpp)
ament_target_dependencies(
  bounded_rrt_connect rclcpp trajectory_msgs geometry_msgs std_msgs)

add_executable(priority_safe_servo src/priority_safe_servo.cpp)
ament_target_dependencies(
  priority_safe_servo rclcpp sensor_msgs trajectory_msgs geometry_msgs std_msgs)
target_link_libraries(priority_safe_servo Threads::Threads)

add_executable(trajectory_auditor src/trajectory_auditor.cpp)
ament_target_dependencies(trajectory_auditor rclcpp std_msgs)

# 같은 패키지에서 생성한 JointSpaceWorld 타입 지원 라이브러리를 사용하는 실행 파일에 연결한다.
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(joint_space_world "${cpp_typesupport_target}")
target_link_libraries(bounded_rrt_connect "${cpp_typesupport_target}")
target_link_libraries(priority_safe_servo "${cpp_typesupport_target}")

# std::array, std::atomic, std::jthread를 쓰므로 C++20을 명시하고 경고를 엄격히 본다.
foreach(target joint_space_world bounded_rrt_connect priority_safe_servo trajectory_auditor)
  target_compile_features(${target} PUBLIC cxx_std_20)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  joint_space_world
  bounded_rrt_connect
  priority_safe_servo
  trajectory_auditor
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch 및 설치된 패키지 share 경로에서 launch/검증 스크립트를 찾을 수 있게 복사한다.
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
