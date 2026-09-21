cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_22)

# 오늘 실습에서 사용하는 ROS 2 표준 메시지와 C++ 클라이언트 라이브러리를 찾는다.
find_package(ament_cmake REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(actuator_simulator src/actuator_simulator.cpp)
ament_target_dependencies(actuator_simulator rclcpp sensor_msgs std_msgs)

add_executable(imm_fault_supervisor src/imm_fault_supervisor.cpp)
ament_target_dependencies(imm_fault_supervisor diagnostic_msgs rclcpp sensor_msgs std_msgs)

add_executable(safety_auditor src/safety_auditor.cpp)
ament_target_dependencies(safety_auditor diagnostic_msgs rclcpp std_msgs)

foreach(target actuator_simulator imm_fault_supervisor safety_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  actuator_simulator
  imm_fault_supervisor
  safety_auditor
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_package()
