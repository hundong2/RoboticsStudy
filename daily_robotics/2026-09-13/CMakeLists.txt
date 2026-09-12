cmake_minimum_required(VERSION 3.8)
project(daily_robotics_2026_09_13)

# ament_cmake는 ROS 2 C++ 패키지의 configure/build/install/package-index 등록을 담당한다.
find_package(ament_cmake REQUIRED)
# hardware_interface와 controller_interface는 ros2_control plugin의 기반 추상화를 제공한다.
find_package(hardware_interface REQUIRED)
find_package(controller_interface REQUIRED)
# pluginlib는 Controller Manager가 문자열 class name으로 공유 라이브러리를 동적 로드하게 한다.
find_package(pluginlib REQUIRED)
# rclcpp/rclcpp_lifecycle는 검증 노드와 configure/activate 상태 전이를 제공한다.
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
# realtime_tools는 non-RT subscriber와 RT update 사이 buffer 및 RT-safe publish handoff를 제공한다.
find_package(realtime_tools REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_library(mock_arm_system SHARED src/mock_arm_system.cpp)
target_include_directories(mock_arm_system PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(mock_arm_system
  hardware_interface pluginlib rclcpp rclcpp_lifecycle)

add_library(dls_ik_controller SHARED src/dls_ik_controller.cpp)
ament_target_dependencies(dls_ik_controller
  controller_interface hardware_interface pluginlib rclcpp rclcpp_lifecycle
  realtime_tools geometry_msgs std_msgs)

add_executable(target_auditor src/target_auditor.cpp)
ament_target_dependencies(target_auditor rclcpp geometry_msgs sensor_msgs std_msgs)

# std::clamp, std::optional 등 사용 문법을 C++17로 고정하고 경고를 학습 오류로 드러낸다.
foreach(target mock_arm_system dls_ik_controller target_auditor)
  target_compile_features(${target} PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

# 첫 인자는 plugin을 소비하는 기반 package이며 XML의 base_class_type과 일치해야 한다.
pluginlib_export_plugin_description_file(hardware_interface hardware_plugins.xml)
pluginlib_export_plugin_description_file(controller_interface controller_plugins.xml)

install(TARGETS mock_arm_system dls_ik_controller target_auditor
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config description launch scripts DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_libraries(mock_arm_system dls_ik_controller)
ament_export_dependencies(hardware_interface controller_interface pluginlib)
ament_package()
