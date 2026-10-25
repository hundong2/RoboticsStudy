cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_26)

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
  "msg/StereoCorrespondences.msg"
  "msg/StereoRansacStats.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(stereo_correspondence_simulator src/stereo_correspondence_simulator.cpp)
ament_target_dependencies(stereo_correspondence_simulator geometry_msgs rclcpp sensor_msgs)
target_compile_features(stereo_correspondence_simulator PUBLIC cxx_std_17)
target_link_libraries(stereo_correspondence_simulator "${cpp_typesupport_target}")

add_executable(bounded_stereo_odometry src/bounded_stereo_odometry.cpp)
ament_target_dependencies(bounded_stereo_odometry nav_msgs rclcpp sensor_msgs)
target_compile_features(bounded_stereo_odometry PUBLIC cxx_std_17)
target_link_libraries(bounded_stereo_odometry "${cpp_typesupport_target}")

add_executable(stereo_odometry_auditor src/stereo_odometry_auditor.cpp)
ament_target_dependencies(stereo_odometry_auditor geometry_msgs nav_msgs rclcpp std_msgs)
target_compile_features(stereo_odometry_auditor PUBLIC cxx_std_17)
target_link_libraries(stereo_odometry_auditor "${cpp_typesupport_target}")

install(TARGETS
  stereo_correspondence_simulator
  bounded_stereo_odometry
  stereo_odometry_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
