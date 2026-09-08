cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_09)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package index 등록을 맡는다.
find_package(ament_cmake REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, Executor 등 ROS 2 C++ 핵심 API를 제공한다.
find_package(rclcpp REQUIRED)
# sensor_msgs는 관절 위치·속도·토크를 표준 JointState 메시지로 표현할 때 사용한다.
find_package(sensor_msgs REQUIRED)
# std_msgs는 목표 각도, 토크 명령, 저주기 진단 문자열처럼 단순한 값을 전달한다.
find_package(std_msgs REQUIRED)

add_executable(reference_generator src/reference_generator.cpp)
ament_target_dependencies(reference_generator rclcpp std_msgs)

add_executable(constrained_mpc_controller src/constrained_mpc_controller.cpp)
ament_target_dependencies(constrained_mpc_controller rclcpp sensor_msgs std_msgs)

add_executable(safe_joint_plant src/safe_joint_plant.cpp)
ament_target_dependencies(safe_joint_plant rclcpp sensor_msgs std_msgs)

# std::array, std::clamp, structured initialization의 동작을 명확히 하도록 C++17로 통일한다.
foreach(target reference_generator constrained_mpc_controller safe_joint_plant)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  reference_generator
  constrained_mpc_controller
  safe_joint_plant
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch가 install 공간에서도 launch 파일을 찾을 수 있게 복사한다.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
