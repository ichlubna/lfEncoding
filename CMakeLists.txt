cmake_minimum_required(VERSION 3.1)
project(lfCodec)
set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")

set(SOURCES 
src/arguments/arguments.hpp
src/encoder.cpp
src/decoder.cpp
src/analyzer.cpp
src/main.cpp
)

find_package(FFMPEG REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})

add_custom_target(run 	COMMAND ${PROJECT_NAME}
						DEPENDS ${PROJECT_NAME}
						WORKING_DIRECTORY ${CMAKE_PROJECT_DIR})

target_link_libraries(${PROJECT_NAME} ${FFMPEG_LIBRARIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
