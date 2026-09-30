cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_01)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/JointVelocityCommand.msg"
  "msg/SnsStatus.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(redundant_arm_simulator src/redundant_arm_simulator.cpp)
ament_target_dependencies(redundant_arm_simulator geometry_msgs rclcpp sensor_msgs)
target_link_libraries(redundant_arm_simulator "${cpp_typesupport_target}")

add_executable(sns_joint_velocity_controller src/sns_joint_velocity_controller.cpp)
ament_target_dependencies(sns_joint_velocity_controller geometry_msgs rclcpp sensor_msgs)
target_link_libraries(sns_joint_velocity_controller "${cpp_typesupport_target}")

add_executable(sns_safety_auditor src/sns_safety_auditor.cpp)
ament_target_dependencies(sns_safety_auditor rclcpp std_msgs)
target_link_libraries(sns_safety_auditor "${cpp_typesupport_target}")

install(TARGETS
  redundant_arm_simulator
  sns_joint_velocity_controller
  sns_safety_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
