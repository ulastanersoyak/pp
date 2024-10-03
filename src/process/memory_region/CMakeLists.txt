file(GLOB MEM_REGION_FILES *.cpp)
add_library(memory_region STATIC ${MEM_REGION_FILES})
target_include_directories(memory_region PRIVATE "${CMAKE_SOURCE_DIR}/includes")
