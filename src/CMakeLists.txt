cmake_minimum_required(VERSION 3.11)
project(smartwater-server)

include_directories(SYSTEM ${CMAKE_CURRENT_LIST_DIR}/../depend/cpp-httplib)
include_directories(SYSTEM ${CMAKE_CURRENT_LIST_DIR}/../depend/json/include)

add_library(smartwater-server-lib
  server.cpp server.h
  database.cpp database.h
  sensor.h)
target_link_libraries(smartwater-server-lib pthread sqlite3)



add_executable(smartwater-server main.cpp)
target_link_libraries(smartwater-server smartwater-server-lib)


add_executable(add_sensor add_sensor_main.cpp)
target_link_libraries(add_sensor smartwater-server-lib)

add_executable(add_measurement add_measurement_main.cpp)
target_link_libraries(add_measurement smartwater-server-lib)

