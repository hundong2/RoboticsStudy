cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_14)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/EstimatorStatus.msg"
  "msg/AuditStatus.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(delayed_sensor_simulator src/delayed_sensor_simulator.cpp)
ament_target_dependencies(delayed_sensor_simulator geometry_msgs nav_msgs rclcpp)

add_executable(bounded_oosm_estimator src/bounded_oosm_estimator.cpp)
ament_target_dependencies(bounded_oosm_estimator geometry_msgs nav_msgs rclcpp)
target_link_libraries(bounded_oosm_estimator "${cpp_typesupport_target}")

add_executable(oosm_safety_auditor src/oosm_safety_auditor.cpp)
ament_target_dependencies(oosm_safety_auditor nav_msgs rclcpp std_msgs)
target_link_libraries(oosm_safety_auditor "${cpp_typesupport_target}")

install(TARGETS
  delayed_sensor_simulator
  bounded_oosm_estimator
  oosm_safety_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
