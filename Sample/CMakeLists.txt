cmake_minimum_required(VERSION 3.20)
project(Sample LANGUAGES CXX)

add_executable(SampleApp src/main.cpp)
target_link_libraries(SampleApp PRIVATE Engine)
target_include_directories(SampleApp PRIVATE ${CMAKE_SOURCE_DIR}/Engine/include)