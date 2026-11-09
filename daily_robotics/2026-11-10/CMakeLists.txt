cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_11_10)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/NdtStats.msg"
  "msg/NdtTruth.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(pointcloud_pair_source src/pointcloud_pair_source.cpp)
ament_target_dependencies(pointcloud_pair_source rclcpp sensor_msgs)
target_compile_features(pointcloud_pair_source PUBLIC cxx_std_17)
target_link_libraries(pointcloud_pair_source "${cpp_typesupport_target}")

add_executable(bounded_ndt_matcher src/bounded_ndt_matcher.cpp)
ament_target_dependencies(bounded_ndt_matcher rclcpp sensor_msgs)
target_compile_features(bounded_ndt_matcher PUBLIC cxx_std_17)
target_link_libraries(bounded_ndt_matcher "${cpp_typesupport_target}")

add_executable(ndt_auditor src/ndt_auditor.cpp)
ament_target_dependencies(ndt_auditor rclcpp std_msgs)
target_compile_features(ndt_auditor PUBLIC cxx_std_17)
target_link_libraries(ndt_auditor "${cpp_typesupport_target}")

install(TARGETS
  pointcloud_pair_source
  bounded_ndt_matcher
  ndt_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
