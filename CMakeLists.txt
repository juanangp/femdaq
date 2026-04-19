cmake_minimum_required(VERSION 3.17)

project(
  femdaq
  LANGUAGES CXX
  VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Threads REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)
find_package(yaml-cpp REQUIRED)
find_package(Readline REQUIRED)

include(FetchContent)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.4.2)
FetchContent_MakeAvailable(CLI11)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(THIRDPARTY_FORCE_PIC
    ON
    CACHE BOOL "" FORCE)

set(ENABLE_TESTING
    OFF
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  Prometheus
  GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
  GIT_TAG v1.2.4)

FetchContent_MakeAvailable(Prometheus)

file(
  GLOB_RECURSE
  SOURCE_FILES
  daq/CommandFetcher.cxx
  daq/FEMSocket.cxx
  daq/RunConfig.cxx
  daq/FEMDAQ.cxx
  daq/FEMDAQARCFEM.cxx
  daq/FEMDAQDCC.cxx
  daq/DCCPacket.cxx
  daq/FEMINOSPacket.cxx
  daq/ARCPacket.cxx
  daq/PrometheusManager.cxx)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${ROOT_INCLUDE_DIRS} daq)

target_link_libraries(
  ${PROJECT_NAME}
  CLI11::CLI11
  yaml-cpp
  readline
  prometheus-cpp::core
  prometheus-cpp::pull
  ${ROOT_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION lib)

# target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
add_executable(femclient main.cxx)
target_link_libraries(
  femclient
  LINK_PUBLIC
  ${PROJECT_NAME}
  CLI11::CLI11
  prometheus-cpp::core
  prometheus-cpp::pull
  yaml-cpp
  readline
  ${ROOT_LIBRARIES})
# Install the binary and the viewer script
install(TARGETS femclient DESTINATION bin)

add_executable(FEMDaqRoot femdaqROOT.cxx)
target_link_libraries(FEMDaqRoot LINK_PUBLIC ${PROJECT_NAME} ${ROOT_LIBRARIES})

install(TARGETS FEMDaqRoot DESTINATION bin)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/macros
  DESTINATION .
  COMPONENT install)
include(thisFEMDAQ)
