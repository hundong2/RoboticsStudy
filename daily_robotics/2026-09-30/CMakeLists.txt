cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_30)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ContactEstimate.msg"
  "msg/ContactTruth.msg"
  "msg/SafetyStatus.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(contact_sensor_simulator src/contact_sensor_simulator.cpp)
ament_target_dependencies(contact_sensor_simulator geometry_msgs rclcpp)
target_link_libraries(contact_sensor_simulator "${cpp_typesupport_target}")

add_executable(contact_covariance_estimator src/contact_covariance_estimator.cpp)
ament_target_dependencies(contact_covariance_estimator geometry_msgs rclcpp)
target_link_libraries(contact_covariance_estimator "${cpp_typesupport_target}")

add_executable(friction_fallback_supervisor src/friction_fallback_supervisor.cpp)
ament_target_dependencies(friction_fallback_supervisor geometry_msgs rclcpp)
target_link_libraries(friction_fallback_supervisor "${cpp_typesupport_target}")

add_executable(contact_safety_auditor src/contact_safety_auditor.cpp)
ament_target_dependencies(contact_safety_auditor rclcpp std_msgs)
target_link_libraries(contact_safety_auditor "${cpp_typesupport_target}")

install(TARGETS
  contact_sensor_simulator
  contact_covariance_estimator
  friction_fallback_supervisor
  contact_safety_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
