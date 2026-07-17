cmake_minimum_required(VERSION 3.10)

project(lns)

set(CMAKE_CXX_STANDARD 14)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "RELEASE")
ENDIF()

include_directories("inc" "inc/CBS" "inc/PIBT")
file(GLOB SOURCES "src/*.cpp" "src/CBS/*.cpp" "src/PIBT/*.cpp")
add_executable(lns ${SOURCES})

# Find Boost
set(Boost_NO_BOOST_CMAKE ON)

find_package(Boost REQUIRED COMPONENTS
    program_options
    filesystem
)

# Find Eigen3 for PIBT
find_package (Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Threads REQUIRED)


include_directories( ${Boost_INCLUDE_DIRS} )
target_link_libraries(lns ${Boost_LIBRARIES} Eigen3::Eigen Threads::Threads)
