cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_27)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/EventPacket.msg"
  "msg/EventMotionStats.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(event_packet_simulator src/event_packet_simulator.cpp)
ament_target_dependencies(event_packet_simulator geometry_msgs rclcpp)
target_compile_features(event_packet_simulator PUBLIC cxx_std_17)
target_link_libraries(event_packet_simulator "${cpp_typesupport_target}")

add_executable(bounded_contrast_flow src/bounded_contrast_flow.cpp)
ament_target_dependencies(bounded_contrast_flow rclcpp)
target_compile_features(bounded_contrast_flow PUBLIC cxx_std_17)
target_link_libraries(bounded_contrast_flow "${cpp_typesupport_target}")

add_executable(event_flow_auditor src/event_flow_auditor.cpp)
ament_target_dependencies(event_flow_auditor geometry_msgs rclcpp std_msgs)
target_compile_features(event_flow_auditor PUBLIC cxx_std_17)
target_link_libraries(event_flow_auditor "${cpp_typesupport_target}")

install(TARGETS
  event_packet_simulator
  bounded_contrast_flow
  event_flow_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
