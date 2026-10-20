cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/FusionStats.msg"
  DEPENDENCIES builtin_interfaces
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(pose_factor_simulator src/pose_factor_simulator.cpp)
ament_target_dependencies(pose_factor_simulator geometry_msgs rclcpp)
target_compile_features(pose_factor_simulator PUBLIC cxx_std_17)

add_executable(bounded_pose_smoother src/bounded_pose_smoother.cpp)
ament_target_dependencies(bounded_pose_smoother geometry_msgs rclcpp)
target_compile_features(bounded_pose_smoother PUBLIC cxx_std_17)
target_link_libraries(bounded_pose_smoother "${cpp_typesupport_target}")

add_executable(fusion_auditor src/fusion_auditor.cpp)
ament_target_dependencies(fusion_auditor geometry_msgs rclcpp std_msgs)
target_compile_features(fusion_auditor PUBLIC cxx_std_17)
target_link_libraries(fusion_auditor "${cpp_typesupport_target}")

install(TARGETS
  pose_factor_simulator
  bounded_pose_smoother
  fusion_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
