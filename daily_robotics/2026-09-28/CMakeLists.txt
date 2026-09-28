cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_28)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ControlStats.msg"
  DEPENDENCIES std_msgs
)

add_executable(arm_plant src/arm_plant.cpp)
target_include_directories(arm_plant PRIVATE include)
ament_target_dependencies(arm_plant rclcpp sensor_msgs std_msgs)

add_executable(operational_space_controller src/operational_space_controller.cpp)
target_include_directories(operational_space_controller PRIVATE include)
ament_target_dependencies(operational_space_controller rclcpp sensor_msgs std_msgs)
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(operational_space_controller "${cpp_typesupport_target}")

add_executable(control_auditor src/control_auditor.cpp)
target_include_directories(control_auditor PRIVATE include)
ament_target_dependencies(control_auditor rclcpp sensor_msgs std_msgs)
target_link_libraries(control_auditor "${cpp_typesupport_target}")

install(TARGETS
  arm_plant
  operational_space_controller
  control_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
