cmake_minimum_required(VERSION 3.22)
project(robot_vision_device VERSION 0.1.0 LANGUAGES CXX)

option(RV_BUILD_TESTS "Build device unit tests" ON)
option(RV_ENABLE_BOOST_HTTP "Enable Boost.Beast HTTP event sink when Boost is available" ON)

# rv_core contains portable pipeline logic; hardware/network adapters are optional sources.
add_library(rv_core
    src/pipeline.cpp
    src/synthetic_adapters.cpp
)
target_include_directories(rv_core PUBLIC include)
target_compile_features(rv_core PUBLIC cxx_std_20)
set_target_properties(rv_core PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
  target_compile_options(rv_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(rv_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RV_ENABLE_BOOST_HTTP)
  # Keep the core build usable when Boost is absent, but add the HTTP smoke-test sink when found.
  find_package(Boost 1.74 QUIET COMPONENTS system)
  if(Boost_FOUND)
    target_sources(rv_core PRIVATE src/http_event_sink.cpp)
    target_link_libraries(rv_core PUBLIC Boost::system)
    target_compile_definitions(rv_core PUBLIC RV_HAS_BOOST_HTTP=1)
  else()
    message(STATUS "Boost.System not found; HTTP sink disabled")
  endif()
endif()

add_executable(robot_vision_device src/main.cpp)
target_link_libraries(robot_vision_device PRIVATE rv_core)

include(GNUInstallDirs)
install(TARGETS robot_vision_device RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/device.example.toml DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/robot-vision)

if(RV_BUILD_TESTS)
  # CTest runs this dependency-free executable on developer PCs and CI.
  enable_testing()
  add_executable(rv_core_tests tests/core_tests.cpp)
  target_link_libraries(rv_core_tests PRIVATE rv_core)
  add_test(NAME rv_core_tests COMMAND rv_core_tests)
endif()
