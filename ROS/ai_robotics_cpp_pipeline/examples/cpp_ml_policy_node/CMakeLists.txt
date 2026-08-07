cmake_minimum_required(VERSION 3.16)

# project는 CMake 프로젝트 이름을 정합니다.
project(cpp_ml_policy_node)

# C++17은 ROS 2 C++ 예제에서 무난한 기준입니다.
set(CMAKE_CXX_STANDARD 17)

# C++17을 반드시 사용하도록 요구합니다.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ament_cmake는 ROS 2 C++ 패키지 빌드에 필요한 CMake helper를 제공합니다.
find_package(ament_cmake REQUIRED)

# rclcpp는 ROS 2 C++ node/publisher/subscriber API를 제공합니다.
find_package(rclcpp REQUIRED)

# sensor_msgs는 LaserScan 메시지를 제공합니다.
find_package(sensor_msgs REQUIRED)

# geometry_msgs는 Twist 메시지를 제공합니다.
find_package(geometry_msgs REQUIRED)

# std_msgs는 Bool 같은 기본 메시지를 제공합니다.
find_package(std_msgs REQUIRED)

# ml_policy_node 실행 파일을 src/ml_policy_node.cpp에서 빌드합니다.
add_executable(ml_policy_node src/ml_policy_node.cpp)

# ml_policy_node가 필요한 ROS 2 패키지를 링크합니다.
ament_target_dependencies(
  ml_policy_node
  rclcpp
  sensor_msgs
  geometry_msgs
)

# safety_filter 실행 파일을 src/safety_filter.cpp에서 빌드합니다.
add_executable(safety_filter src/safety_filter.cpp)

# safety_filter가 필요한 ROS 2 패키지를 링크합니다.
ament_target_dependencies(
  safety_filter
  rclcpp
  geometry_msgs
  sensor_msgs
  std_msgs
)

# install은 빌드된 실행 파일을 install/lib/<package> 아래에 배치합니다.
install(
  TARGETS ml_policy_node safety_filter
  DESTINATION lib/${PROJECT_NAME}
)

# launch 파일을 install/share/<package>/launch로 복사합니다.
install(
  DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

# config 파일을 install/share/<package>/config로 복사합니다.
install(
  DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
)

# ament_package는 ROS 2 패키지 메타데이터 생성을 마무리합니다.
ament_package()
