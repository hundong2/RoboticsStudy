cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_27)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/DeskewStats.msg"
  DEPENDENCIES builtin_interfaces std_msgs
)

add_executable(lidar_imu_simulator src/lidar_imu_simulator.cpp)
ament_target_dependencies(lidar_imu_simulator rclcpp sensor_msgs std_msgs)

add_executable(imu_deskew_node src/imu_deskew_node.cpp)
ament_target_dependencies(imu_deskew_node rclcpp sensor_msgs std_msgs)
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(imu_deskew_node "${cpp_typesupport_target}")

add_executable(deskew_auditor src/deskew_auditor.cpp)
ament_target_dependencies(deskew_auditor rclcpp sensor_msgs std_msgs)
target_link_libraries(deskew_auditor "${cpp_typesupport_target}")

install(TARGETS
  lidar_imu_simulator
  imu_deskew_node
  deskew_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
