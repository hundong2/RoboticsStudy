cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_23)

# ament/colcon, ROS 2 C++ API, 표준 센서 메시지, 고정 크기 사용자 메시지 생성기를 찾는다.
find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# EffortCommand와 SafetyStatus는 제어 샘플을 동적 배열 없이 전달하는 명시적 계약이다.
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/EffortCommand.msg"
  "msg/SafetyStatus.msg"
  DEPENDENCIES builtin_interfaces)

rosidl_get_typesupport_target(cpp_typesupport_target
  ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(contact_plant src/contact_plant.cpp)
ament_target_dependencies(contact_plant geometry_msgs rclcpp sensor_msgs std_msgs)

add_executable(impedance_controller src/impedance_controller.cpp)
ament_target_dependencies(impedance_controller geometry_msgs rclcpp sensor_msgs std_msgs)
target_link_libraries(impedance_controller "${cpp_typesupport_target}")

add_executable(energy_safety_auditor src/energy_safety_auditor.cpp)
ament_target_dependencies(energy_safety_auditor geometry_msgs rclcpp sensor_msgs std_msgs)
target_link_libraries(energy_safety_auditor "${cpp_typesupport_target}")

foreach(target contact_plant impedance_controller energy_safety_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  contact_plant
  impedance_controller
  energy_safety_auditor
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
