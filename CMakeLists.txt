cmake_minimum_required(VERSION 3.20)
project(MDLib)

add_library(MDLib STATIC
    src/InjectionManager/InjectionManager.cpp
    src/ProcessManager/ProcessManager.cpp
)

target_include_directories(MDLib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)