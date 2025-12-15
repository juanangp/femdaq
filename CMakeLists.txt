cmake_minimum_required(VERSION 3.17)

project(
    femdaq
    LANGUAGES CXX
    VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Threads REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree ROOTNTuple)
find_package(yaml-cpp REQUIRED)
find_package(Readline REQUIRED)

include(FetchContent)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2)
FetchContent_MakeAvailable(CLI11)

file(
    GLOB_RECURSE
    SOURCE_FILES
    daq/CommandFetcher.cxx
    daq/FEMSocket.cxx
    daq/RunConfig.cxx
    daq/FEMDAQ.cxx
    daq/FEMDAQARCFEM.cxx
    daq/DCCPacket.cxx
    daq/FEMINOSPacket.cxx
    daq/ARCPacket.cxx)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC ${ROOT_INCLUDE_DIRS}
            daq)


target_link_libraries(
    ${PROJECT_NAME} CLI11::CLI11 yaml-cpp readline ${ROOT_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION lib)

#target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
add_executable(femclient main.cxx)
target_link_libraries(femclient LINK_PUBLIC ${PROJECT_NAME} CLI11::CLI11 yaml-cpp readline ${ROOT_LIBRARIES})
# Install the binary and the viewer script
install(TARGETS femclient DESTINATION bin)


