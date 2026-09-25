cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_26)

# ament_cmake는 colcon이 이 ROS 2 C++ 패키지를 빌드하고 설치하도록 연결한다.
find_package(ament_cmake REQUIRED)
# geometry_msgs는 힘/토크를 좌표계와 시각과 함께 전달하는 WrenchStamped를 제공한다.
find_package(geometry_msgs REQUIRED)
# rclcpp는 Node, Publisher, Subscription, Timer, Executor API를 제공한다.
find_package(rclcpp REQUIRED)
# custom ContactSolution 메시지의 C++ 타입 지원 코드를 생성한다.
find_package(rosidl_default_generators REQUIRED)
# sensor_msgs/JointState는 Jacobian transpose로 계산한 관절 effort를 표현한다.
find_package(sensor_msgs REQUIRED)
# Header와 감사 결과 Bool/String 메시지를 사용한다.
find_package(std_msgs REQUIRED)
# 고정 주기 계산 커널을 실행하는 std::thread를 플랫폼 스레드 라이브러리와 링크한다.
find_package(Threads REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ContactSolution.msg"
  DEPENDENCIES std_msgs
)
ament_export_dependencies(rosidl_default_runtime)

add_executable(wrench_command_publisher src/wrench_command_publisher.cpp)
ament_target_dependencies(wrench_command_publisher geometry_msgs rclcpp)

add_executable(bounded_contact_allocator src/bounded_contact_allocator.cpp)
ament_target_dependencies(bounded_contact_allocator geometry_msgs rclcpp std_msgs)
target_link_libraries(bounded_contact_allocator Threads::Threads)

add_executable(whole_body_auditor src/whole_body_auditor.cpp)
ament_target_dependencies(whole_body_auditor rclcpp sensor_msgs std_msgs)

# 같은 패키지에서 생성한 ContactSolution 헤더와 타입 지원 라이브러리를 실행 파일에 연결한다.
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
foreach(target wrench_command_publisher bounded_contact_allocator whole_body_auditor)
  target_link_libraries(${target} "${cpp_typesupport_target}")
  # std::array, std::atomic, structured binding을 명시적으로 C++17에 고정한다.
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

install(TARGETS wrench_command_publisher bounded_contact_allocator whole_body_auditor
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch scripts DESTINATION share/${PROJECT_NAME})

ament_package()
