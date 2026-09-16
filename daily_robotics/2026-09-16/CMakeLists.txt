cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_16)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package-index 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# rclcpp는 Node, Topic, Timer, QoS, Executor API를 제공한다.
find_package(rclcpp REQUIRED)
# rosidl은 오늘 사용할 CalibrationStatus 사용자 정의 메시지를 C++ 타입으로 생성한다.
find_package(rosidl_default_generators REQUIRED)
# TwistWithCovarianceStamped는 측정 시각, 각속도, 분산을 한 메시지로 운반한다.
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/CalibrationStatus.msg"
  DEPENDENCIES std_msgs
)

add_executable(yaw_rate_sensor src/yaw_rate_sensor.cpp)
ament_target_dependencies(yaw_rate_sensor rclcpp geometry_msgs)

add_executable(time_offset_fusion src/time_offset_fusion.cpp)
ament_target_dependencies(time_offset_fusion rclcpp geometry_msgs std_msgs)

add_executable(calibration_auditor src/calibration_auditor.cpp)
ament_target_dependencies(calibration_auditor rclcpp std_msgs)

# 같은 패키지에서 생성한 CalibrationStatus 타입 지원 라이브러리를 두 소비자에 연결한다.
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(time_offset_fusion "${cpp_typesupport_target}")
target_link_libraries(calibration_auditor "${cpp_typesupport_target}")

# std::array와 수치 알고리즘을 사용하며, Jazzy 기본 컴파일러에서 재현되도록 C++17을 고정한다.
foreach(target yaw_rate_sensor time_offset_fusion calibration_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  yaw_rate_sensor
  time_offset_fusion
  calibration_auditor
  DESTINATION lib/${PROJECT_NAME}
)

# 설치된 패키지에서도 ros2 launch와 smoke test를 찾을 수 있게 복사한다.
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
