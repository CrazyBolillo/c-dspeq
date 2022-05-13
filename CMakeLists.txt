cmake_minimum_required(VERSION 3.22)
project(c_dspeq C)

set(CMAKE_C_STANDARD 99)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(SndFile REQUIRED)
find_package(PortAudio REQUIRED)
add_executable(c_dspeq main.c)
target_link_libraries(c_dspeq SndFile::sndfile portaudio)
