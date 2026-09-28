cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_29)

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
  "msg/FusionStats.msg"
  DEPENDENCIES std_msgs
)

add_executable(sensor_simulator src/sensor_simulator.cpp)
ament_target_dependencies(sensor_simulator geometry_msgs nav_msgs rclcpp sensor_msgs)

add_executable(fixed_lag_smoother src/fixed_lag_smoother.cpp)
target_include_directories(fixed_lag_smoother PRIVATE include)
ament_target_dependencies(fixed_lag_smoother nav_msgs rclcpp sensor_msgs std_msgs)
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(fixed_lag_smoother "${cpp_typesupport_target}")

add_executable(fusion_auditor src/fusion_auditor.cpp)
ament_target_dependencies(fusion_auditor geometry_msgs nav_msgs rclcpp std_msgs)
target_link_libraries(fusion_auditor "${cpp_typesupport_target}")

install(TARGETS
  sensor_simulator
  fixed_lag_smoother
  fusion_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
