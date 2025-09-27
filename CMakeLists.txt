cmake_minimum_required(VERSION 3.21)
project(OOP_Pro2_v3)

set(CMAKE_CXX_STANDARD 17)

# Add Winsock library for Windows
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lws2_32")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lws2_32")
endif()

add_executable(OOP_Pro2_v3 main.cpp)

# Link libraries
target_link_libraries(OOP_Pro2_v3
        mingw32
        SDL2main
        SDL2
        SDL2_image
        SDL2_ttf
        SDL2_mixer
        )

# For Windows, explicitly link ws2_32
if(WIN32)
    target_link_libraries(OOP_Pro2_v3 ws2_32)
endif()