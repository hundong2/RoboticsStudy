cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_10_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ActuatorSample.msg"
  "msg/EstimatorOutput.msg"
  "msg/SafetyStatus.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(actuator_sensor_simulator src/actuator_sensor_simulator.cpp)
ament_target_dependencies(actuator_sensor_simulator rclcpp sensor_msgs std_msgs)
target_compile_features(actuator_sensor_simulator PUBLIC cxx_std_17)
target_link_libraries(actuator_sensor_simulator "${cpp_typesupport_target}")

add_executable(adaptive_covariance_filter src/adaptive_covariance_filter.cpp)
ament_target_dependencies(adaptive_covariance_filter rclcpp)
target_compile_features(adaptive_covariance_filter PUBLIC cxx_std_17)
target_link_libraries(adaptive_covariance_filter "${cpp_typesupport_target}")

add_executable(energy_rate_supervisor src/energy_rate_supervisor.cpp)
ament_target_dependencies(energy_rate_supervisor rclcpp std_msgs)
target_compile_features(energy_rate_supervisor PUBLIC cxx_std_17)
target_link_libraries(energy_rate_supervisor "${cpp_typesupport_target}")

add_executable(pipeline_auditor src/pipeline_auditor.cpp)
ament_target_dependencies(pipeline_auditor rclcpp std_msgs)
target_compile_features(pipeline_auditor PUBLIC cxx_std_17)
target_link_libraries(pipeline_auditor "${cpp_typesupport_target}")

install(TARGETS
  actuator_sensor_simulator
  adaptive_covariance_filter
  energy_rate_supervisor
  pipeline_auditor
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(PROGRAMS test/smoke_test.sh DESTINATION lib/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
