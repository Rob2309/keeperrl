cmake_minimum_required(VERSION 3.6)

project(KeeperRL-Android)

set(ENABLE_STEAMWORKS off)
set(CURL_ENABLE_SSL off)
set(BUILD_ALSOFT on)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ ${CMAKE_CURRENT_BINARY_DIR}/keeperrl)
