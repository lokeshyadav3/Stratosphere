cmake_minimum_required(VERSION 3.20)
project(Sample LANGUAGES CXX)

add_executable(SampleApp src/main.cpp)
target_link_libraries(SampleApp PRIVATE Engine)
target_include_directories(SampleApp PRIVATE ${CMAKE_SOURCE_DIR}/Engine/include)

file(GLOB SHADER_SPV ${CMAKE_SOURCE_DIR}/Engine/Shaders/*.spv)

foreach(SPIRV ${SHADER_SPV})
    add_custom_command(TARGET SampleApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SampleApp>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV} $<TARGET_FILE_DIR:SampleApp>/shaders/
    )
endforeach()
