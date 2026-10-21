cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_22)

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
  "msg/PoseGraphInput.msg"
  "msg/OptimizationStats.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(graph_input_simulator src/graph_input_simulator.cpp)
ament_target_dependencies(graph_input_simulator geometry_msgs nav_msgs rclcpp)
target_compile_features(graph_input_simulator PUBLIC cxx_std_17)
target_link_libraries(graph_input_simulator "${cpp_typesupport_target}")

add_executable(bounded_switchable_optimizer src/bounded_switchable_optimizer.cpp)
ament_target_dependencies(bounded_switchable_optimizer geometry_msgs nav_msgs rclcpp)
target_compile_features(bounded_switchable_optimizer PUBLIC cxx_std_17)
target_link_libraries(bounded_switchable_optimizer "${cpp_typesupport_target}")

add_executable(robust_slam_auditor src/robust_slam_auditor.cpp)
ament_target_dependencies(robust_slam_auditor geometry_msgs nav_msgs rclcpp std_msgs)
target_compile_features(robust_slam_auditor PUBLIC cxx_std_17)
target_link_libraries(robust_slam_auditor "${cpp_typesupport_target}")

install(TARGETS
  graph_input_simulator
  bounded_switchable_optimizer
  robust_slam_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
