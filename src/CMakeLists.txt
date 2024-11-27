file(
  GLOB DIRS
  LIST_DIRECTORIES true
  "${CMAKE_CURRENT_SOURCE_DIR}/*")

set(ALL_SOURCES)

foreach(DIR ${DIRS})
  if(IS_DIRECTORY ${DIR})
    file(GLOB_RECURSE SOURCES "${DIR}/*.cpp")
    list(APPEND ALL_SOURCES ${SOURCES})
    message(STATUS "sources in: ${DIR}, files: ${SOURCES}")
  endif()
endforeach()

list(FILTER ALL_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

if(BUILD_SHARED_LIBS)
  add_library(ppdynamic SHARED ${ALL_SOURCES})
  target_include_directories(ppdynamic PRIVATE "${CMAKE_SOURCE_DIR}/includes")
  set(PP_LIB ppdynamic)
else()
  add_library(ppstatic STATIC ${ALL_SOURCES})
  target_include_directories(ppstatic PRIVATE "${CMAKE_SOURCE_DIR}/includes")
  set(PP_LIB ppstatic)
endif()

add_executable(pp "${CMAKE_CURRENT_SOURCE_DIR}/cli/main.cpp")
target_include_directories(pp PRIVATE "${CMAKE_SOURCE_DIR}/includes")
target_link_libraries(pp PRIVATE ${PP_LIB})

set_target_properties(${PP_LIB} pp PROPERTIES CXX_STANDARD 23
                                              CXX_STANDARD_REQUIRED ON)
