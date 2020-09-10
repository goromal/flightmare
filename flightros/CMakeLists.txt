project(flightros)

cmake_minimum_required(VERSION 3.0.0)

find_package(OpenCV REQUIRED)

find_package(catkin REQUIRED COMPONENTS
    flightlib
    roscpp
    nav_msgs
)

catkin_package(
    INCLUDE_DIRS include
    CATKIN_DEPENDS flightlib roscpp nav_msgs
)

find_package(flightlib REQUIRED)

include_directories(
    include
    ${flightlib_INCLUDE_DIRS}
    ${catkin_INCLUDE_DIRS}
)

# Setup Default Build Type as Release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif ()

# Setup Architcture-specific Flags
if ("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
    message(STATUS "Using ARMv7 optimized flags!")
    set(CMAKE_CXX_ARCH_FLAGS " -Wno-psabi -march=armv7-a -mfpu=neon -mfloat-abi=hard -funsafe-math-optimizations")
elseif ("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
    message(STATUS "Using ARM aarch64 optimized flags!")
    set(CMAKE_CXX_ARCH_FLAGS " -Wno-psabi -march=armv8-a+crypto -mcpu=cortex-a57+crypto")
else ()
    set(CMAKE_CXX_ARCH_FLAGS " -march=native")
endif ()

# Setup General C++ Flags
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DEIGEN_STACK_ALLOCATION_LIMIT=1048576")

# Setup Release and Debug flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_ARCH_FLAGS} -Wall -DNDEBUG -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -Wall -g")

add_library(flight_pilot
    src/flight_pilot.cpp
)
add_dependencies(flight_pilot ${catkin_EXPORTED_TARGETS})
target_link_libraries(flight_pilot
  ${catkin_LIBRARIES}
  ${OpenCV_LIBRARIES}
)

add_executable(flight_pilot_node
    src/flight_pilot_node.cpp
)
target_link_libraries(flight_pilot_node
  flight_pilot
  ${OpenCV_LIBRARIES}
)
