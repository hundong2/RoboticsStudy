cmake_minimum_required(VERSION 3.14)

project(OpenSourceAiCpp17Guide LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tiny_rag src/tiny_rag.cpp)
target_compile_features(tiny_rag PRIVATE cxx_std_17)

add_executable(sampling_demo src/sampling_demo.cpp)
target_compile_features(sampling_demo PRIVATE cxx_std_17)

foreach(target_name IN ITEMS tiny_rag sampling_demo)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
