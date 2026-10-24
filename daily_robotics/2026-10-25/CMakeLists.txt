cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_25)

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
  "msg/FeatureTrackStats.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(camera_stream_simulator src/camera_stream_simulator.cpp)
ament_target_dependencies(camera_stream_simulator geometry_msgs rclcpp sensor_msgs)
target_compile_features(camera_stream_simulator PUBLIC cxx_std_17)

add_executable(bounded_feature_tracker src/bounded_feature_tracker.cpp)
ament_target_dependencies(bounded_feature_tracker nav_msgs rclcpp sensor_msgs)
target_compile_features(bounded_feature_tracker PUBLIC cxx_std_17)
target_link_libraries(bounded_feature_tracker "${cpp_typesupport_target}")

add_executable(visual_frontend_auditor src/visual_frontend_auditor.cpp)
ament_target_dependencies(visual_frontend_auditor geometry_msgs nav_msgs rclcpp std_msgs)
target_compile_features(visual_frontend_auditor PUBLIC cxx_std_17)
target_link_libraries(visual_frontend_auditor "${cpp_typesupport_target}")

install(TARGETS
  camera_stream_simulator
  bounded_feature_tracker
  visual_frontend_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
