file(GLOB THREAD_FILES *.cpp)
add_library(thread STATIC ${THREAD_FILES})
target_include_directories(thread PRIVATE "${CMAKE_SOURCE_DIR}/includes")
