cmake_minimum_required(VERSION 3.15)
project(bi VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(bi src/main.cpp)

find_package(Threads REQUIRED)
target_link_libraries(bi PRIVATE Threads::Threads)

if(UNIX)
  target_compile_options(bi PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

install(TARGETS bi RUNTIME DESTINATION bin)