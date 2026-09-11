cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_12)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package-index 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, spin 같은 ROS 2 C++ 핵심 API를 제공한다.
find_package(rclcpp REQUIRED)
# sensor_msgs/PointCloud는 Header stamp와 feature point/channel을 함께 전달하는 학습용 센서 메시지다.
find_package(sensor_msgs REQUIRED)
# nav_msgs/Odometry는 simulator의 기준 pose와 estimator의 누적 pose를 같은 표준 형식으로 전달한다.
find_package(nav_msgs REQUIRED)
# geometry_msgs/Point32와 Quaternion은 PointCloud 및 Odometry 내부 자료형으로 사용한다.
find_package(geometry_msgs REQUIRED)
# std_msgs/String은 저주기 진단과 독립 auditor 판정을 사람이 읽을 수 있게 보낸다.
find_package(std_msgs REQUIRED)
# message_filters는 서로 다른 센서 topic을 Header stamp 기준으로 짝지어 주는 Synchronizer를 제공한다.
find_package(message_filters REQUIRED)

add_executable(sensor_simulator src/sensor_simulator.cpp)
ament_target_dependencies(sensor_simulator rclcpp sensor_msgs nav_msgs geometry_msgs)

add_executable(approximate_time_sync src/approximate_time_sync.cpp)
ament_target_dependencies(approximate_time_sync rclcpp sensor_msgs std_msgs message_filters)

add_executable(bounded_vo_estimator src/bounded_vo_estimator.cpp)
ament_target_dependencies(bounded_vo_estimator rclcpp sensor_msgs nav_msgs geometry_msgs std_msgs)

add_executable(vo_auditor src/vo_auditor.cpp)
ament_target_dependencies(vo_auditor rclcpp nav_msgs std_msgs message_filters)

# std::array, std::clamp, if-with-initializer 등 예제 문법을 C++17로 고정한다.
foreach(target sensor_simulator approximate_time_sync bounded_vo_estimator vo_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS
  sensor_simulator
  approximate_time_sync
  bounded_vo_estimator
  vo_auditor
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch와 설치된 패키지 문서에서 launch/smoke script를 찾을 수 있게 복사한다.
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_package()
