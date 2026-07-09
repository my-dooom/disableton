cmake_minimum_required(VERSION 3.20)

project(disableton VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


add_executable(disableton
    src/main.cc
    src/io/audio_endpoint.cc
)

target_include_directories(disableton
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(disableton PRIVATE -Wall -Wextra -Wpedantic)
endif()
