cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_24)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/PreintegrationStats.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(vio_sensor_simulator src/vio_sensor_simulator.cpp)
ament_target_dependencies(vio_sensor_simulator geometry_msgs rclcpp sensor_msgs)
target_compile_features(vio_sensor_simulator PUBLIC cxx_std_17)

add_executable(bounded_preintegration_smoother src/bounded_preintegration_smoother.cpp)
ament_target_dependencies(bounded_preintegration_smoother geometry_msgs nav_msgs rclcpp sensor_msgs)
target_compile_features(bounded_preintegration_smoother PUBLIC cxx_std_17)
target_link_libraries(bounded_preintegration_smoother "${cpp_typesupport_target}")

add_executable(vio_auditor src/vio_auditor.cpp)
ament_target_dependencies(vio_auditor geometry_msgs nav_msgs rclcpp std_msgs)
target_compile_features(vio_auditor PUBLIC cxx_std_17)
target_link_libraries(vio_auditor "${cpp_typesupport_target}")

install(TARGETS
  vio_sensor_simulator
  bounded_preintegration_smoother
  vio_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
