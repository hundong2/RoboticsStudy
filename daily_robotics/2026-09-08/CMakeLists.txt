cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_08)

# ament_cmake는 ROS 2 C++ 패키지의 configure, build, install, package index 등록을 맡는다.
find_package(ament_cmake REQUIRED)
# geometry_msgs는 차체 속도 명령(Twist)과 추정 자세(PoseWithCovarianceStamped)를 제공한다.
find_package(geometry_msgs REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, Executor 등 ROS 2 C++ 핵심 API다.
find_package(rclcpp REQUIRED)
# sensor_msgs는 바퀴 엔코더(JointState)와 모의 2D Lidar(LaserScan)를 표현한다.
find_package(sensor_msgs REQUIRED)

add_executable(sensor_simulator src/sensor_simulator.cpp)
ament_target_dependencies(sensor_simulator geometry_msgs rclcpp sensor_msgs)

add_executable(priority_motor_controller src/priority_motor_controller.cpp)
ament_target_dependencies(priority_motor_controller geometry_msgs rclcpp sensor_msgs)

add_executable(particle_localizer src/particle_localizer.cpp)
ament_target_dependencies(particle_localizer geometry_msgs rclcpp sensor_msgs)

# std::array, std::atomic<uint64_t>, clamp 등 명확한 표준 동작을 위해 C++17로 고정한다.
foreach(target sensor_simulator priority_motor_controller particle_localizer)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

# pthread_setschedparam을 직접 호출하는 제어 실행 파일에 POSIX thread 라이브러리를 연결한다.
target_link_libraries(priority_motor_controller pthread)

install(TARGETS
  sensor_simulator
  priority_motor_controller
  particle_localizer
  DESTINATION lib/${PROJECT_NAME}
)
# ros2 launch가 install 공간에서도 launch 파일을 찾도록 복사한다.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
