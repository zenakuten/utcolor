cmake_minimum_required(VERSION 3.20)
project(utcolor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(utcolor WIN32 src/main.cpp)
target_link_libraries(utcolor PRIVATE SDL3::SDL3 imgui::imgui)
