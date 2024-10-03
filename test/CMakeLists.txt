add_executable(test test.cpp)
target_include_directories(test PRIVATE "${CMAKE_SOURCE_DIR}/includes")
target_link_libraries(test process)
