cmake_minimum_required(VERSION 3.10)
project(mwe_glfw_input_replay)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)


file(GLOB_RECURSE SOURCES "src/*.cpp")
# Add the main executable
add_executable(${PROJECT_NAME} ${SOURCES})
        

find_package(glad)
find_package(glfw3)
find_package(spdlog)
find_package(fmt)
target_link_libraries(${PROJECT_NAME} glad::glad glfw spdlog::spdlog fmt::fmt)
