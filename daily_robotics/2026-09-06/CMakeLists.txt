cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_06)

# ament_cmake는 ROS 2 C++ 패키지의 빌드, 설치, 패키지 인덱스 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# geometry_msgs는 속도, 위치 측정, 공분산을 포함한 EKF 자세 메시지를 제공한다.
find_package(geometry_msgs REQUIRED)
# lifecycle_msgs는 configure/activate 전이를 요청하는 표준 Service 타입을 제공한다.
find_package(lifecycle_msgs REQUIRED)
# rclcpp는 Node, QoS, Service, Timer, Executor 등 ROS 2 C++ 핵심 API를 제공한다.
find_package(rclcpp REQUIRED)
# rclcpp_lifecycle은 상태가 명시적인 LifecycleNode와 LifecyclePublisher를 제공한다.
find_package(rclcpp_lifecycle REQUIRED)
# rosidl_default_generators는 srv/ResetPose.srv를 C++ 헤더와 타입 지원 코드로 변환한다.
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "srv/ResetPose.srv"
)
ament_export_dependencies(rosidl_default_runtime)

add_executable(sensor_simulator src/sensor_simulator.cpp)
ament_target_dependencies(sensor_simulator geometry_msgs rclcpp)

add_executable(lifecycle_ekf_node src/lifecycle_ekf_node.cpp)
ament_target_dependencies(
  lifecycle_ekf_node geometry_msgs lifecycle_msgs rclcpp rclcpp_lifecycle
)

add_executable(lifecycle_manager src/lifecycle_manager.cpp)
ament_target_dependencies(lifecycle_manager lifecycle_msgs rclcpp)

# 같은 패키지에서 생성한 ResetPose Service 타입 지원 라이브러리를 서버/클라이언트에 연결한다.
rosidl_get_typesupport_target(
  cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp"
)
target_link_libraries(lifecycle_ekf_node "${cpp_typesupport_target}")
target_link_libraries(lifecycle_manager "${cpp_typesupport_target}")

# std::array, std::clamp, 구조화된 상태 관리 등 명시적인 C++17 기능 집합을 사용한다.
set(study_targets sensor_simulator lifecycle_ekf_node lifecycle_manager)
foreach(target IN LISTS study_targets)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS ${study_targets} DESTINATION lib/${PROJECT_NAME})
# ros2 launch가 설치 공간에서도 launch 파일을 찾을 수 있도록 함께 설치한다.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
