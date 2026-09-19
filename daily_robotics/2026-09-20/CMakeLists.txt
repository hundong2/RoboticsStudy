cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_20)

# ament/colcon과 ROS 2 메시지 생성기를 불러온다.
find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

# 세 메시지는 모두 MCU↔호스트 경계에서 크기와 시퀀스를 명시하기 위한 계약이다.
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/TerrainPatch.msg"
  "msg/TransportStats.msg"
  "msg/TraversabilityGrid.msg"
  DEPENDENCIES builtin_interfaces)

rosidl_get_typesupport_target(cpp_typesupport_target
  ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(mcu_let_source src/mcu_let_source.cpp)
ament_target_dependencies(mcu_let_source rclcpp)
target_link_libraries(mcu_let_source "${cpp_typesupport_target}")

add_executable(transport_budget_monitor src/transport_budget_monitor.cpp)
ament_target_dependencies(transport_budget_monitor rclcpp)
target_link_libraries(transport_budget_monitor "${cpp_typesupport_target}")

add_executable(traversability_estimator src/traversability_estimator.cpp)
ament_target_dependencies(traversability_estimator nav_msgs rclcpp)
target_link_libraries(traversability_estimator "${cpp_typesupport_target}")

add_executable(pipeline_auditor src/pipeline_auditor.cpp)
ament_target_dependencies(pipeline_auditor rclcpp std_msgs)
target_link_libraries(pipeline_auditor "${cpp_typesupport_target}")

foreach(target mcu_let_source transport_budget_monitor traversability_estimator pipeline_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  mcu_let_source
  transport_budget_monitor
  traversability_estimator
  pipeline_auditor
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
