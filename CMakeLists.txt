cmake_minimum_required(VERSION 3.20)

project(disableton VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable testing
enable_testing()

# Find ALSA
find_package(ALSA REQUIRED)

# Fetch Google Test if not found
include(FetchContent)
fetchcontent_declare(
  googletest
  URL https://github.com/google/googletest/archive/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
fetchcontent_makeavailable(googletest)

add_executable(disableton
    src/main.cc
    src/io/audio_endpoint.cc
)


target_link_libraries(disableton
  PRIVATE
    ALSA::ALSA
)

target_include_directories(disableton
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(disableton PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Audio endpoint tests
add_executable(audio_endpoint_tests
  src/io/audio_endpoint.cc
  tests/audio_endpoint_test.cc
)

target_include_directories(audio_endpoint_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

target_link_libraries(audio_endpoint_tests
  PRIVATE
    ALSA::ALSA
    gmock_main
)

# Add linker flags to wrap ALSA functions for mocking
target_link_options(
  audio_endpoint_tests
  PRIVATE
    -Wl,--wrap=snd_pcm_open
    -Wl,--wrap=snd_pcm_close
    -Wl,--wrap=snd_pcm_hw_params_any
    -Wl,--wrap=snd_pcm_hw_params_test_access
    -Wl,--wrap=snd_pcm_hw_params_test_format
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(
    audio_endpoint_tests
    PRIVATE
    -Wall -Wextra -Wpedantic
  )
endif()

add_test(NAME AudioEndpointTests COMMAND audio_endpoint_tests)
