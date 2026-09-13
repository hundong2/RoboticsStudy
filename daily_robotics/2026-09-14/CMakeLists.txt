cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_14)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package-index 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# rclcpp는 Node, Topic, Timer, QoS, Executor, ContentFilteredTopic API를 제공한다.
find_package(rclcpp REQUIRED)
# rosidl은 DDS 필터 식에서 직접 참조할 수 있는 top-level 필드의 실습 메시지를 생성한다.
find_package(rosidl_default_generators REQUIRED)
# nav_msgs/Path는 전역 경로와 지역 최적화 궤적을 같은 표준 메시지로 전달한다.
find_package(nav_msgs REQUIRED)
# geometry_msgs/Twist는 navigator의 속도 명령과 supervisor의 안전 출력에 사용한다.
find_package(geometry_msgs REQUIRED)
# std_msgs/Header, String, Bool은 heartbeat·진단·안전 상태에 사용한다.
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ObstacleSample.msg"
  DEPENDENCIES std_msgs
)

add_executable(world_simulator src/world_simulator.cpp)
ament_target_dependencies(world_simulator rclcpp nav_msgs geometry_msgs std_msgs)

add_executable(elastic_band_optimizer src/elastic_band_optimizer.cpp)
ament_target_dependencies(elastic_band_optimizer rclcpp nav_msgs geometry_msgs std_msgs)

add_executable(reactive_bt_navigator src/reactive_bt_navigator.cpp)
ament_target_dependencies(reactive_bt_navigator rclcpp nav_msgs geometry_msgs std_msgs)

add_executable(deadline_supervisor src/deadline_supervisor.cpp)
ament_target_dependencies(deadline_supervisor rclcpp geometry_msgs std_msgs)

add_executable(trajectory_auditor src/trajectory_auditor.cpp)
ament_target_dependencies(trajectory_auditor rclcpp nav_msgs geometry_msgs std_msgs)

# 같은 패키지에서 생성한 ObstacleSample 타입 지원 라이브러리를 사용하는 두 실행 파일에 연결한다.
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(world_simulator "${cpp_typesupport_target}")
target_link_libraries(elastic_band_optimizer "${cpp_typesupport_target}")

# std::array, std::clamp, std::jthread 같은 문법을 쓰므로 C++20을 명시한다.
foreach(target
    world_simulator elastic_band_optimizer reactive_bt_navigator deadline_supervisor
    trajectory_auditor)
  target_compile_features(${target} PUBLIC cxx_std_20)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  world_simulator
  elastic_band_optimizer
  reactive_bt_navigator
  deadline_supervisor
  trajectory_auditor
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch와 설치된 패키지 share 경로에서 계약/검증 자료를 찾을 수 있게 복사한다.
install(DIRECTORY behavior_trees launch scripts DESTINATION share/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
