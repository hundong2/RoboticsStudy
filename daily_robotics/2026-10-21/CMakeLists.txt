cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_21)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ContactSample.msg"
  "msg/EstimatorStats.msg"
  DEPENDENCIES builtin_interfaces std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(contact_sensor_simulator src/contact_sensor_simulator.cpp)
ament_target_dependencies(contact_sensor_simulator geometry_msgs rclcpp sensor_msgs)
target_compile_features(contact_sensor_simulator PUBLIC cxx_std_17)
target_link_libraries(contact_sensor_simulator "${cpp_typesupport_target}")

add_executable(bounded_contact_ekf src/bounded_contact_ekf.cpp)
ament_target_dependencies(bounded_contact_ekf geometry_msgs rclcpp)
target_compile_features(bounded_contact_ekf PUBLIC cxx_std_17)
target_link_libraries(bounded_contact_ekf "${cpp_typesupport_target}")

add_executable(contact_estimator_auditor src/contact_estimator_auditor.cpp)
ament_target_dependencies(contact_estimator_auditor geometry_msgs rclcpp std_msgs)
target_compile_features(contact_estimator_auditor PUBLIC cxx_std_17)
target_link_libraries(contact_estimator_auditor "${cpp_typesupport_target}")

install(TARGETS
  contact_sensor_simulator
  bounded_contact_ekf
  contact_estimator_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
