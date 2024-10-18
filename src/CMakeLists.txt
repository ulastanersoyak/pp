file(GLOB DIRS LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")

set(ALL_SOURCES)

foreach(DIR ${DIRS})
  if(IS_DIRECTORY ${DIR})
    file(GLOB_RECURSE SOURCES "${DIR}/*.cpp")
    list(APPEND ALL_SOURCES ${SOURCES})
    message(STATUS "sources in: ${DIR}, files: ${SOURCES}")
  endif()
endforeach()

if(BUILD_SHARED_LIBS)
  add_library(ppdynamic SHARED ${ALL_SOURCES})
  target_include_directories(ppdynamic PRIVATE "${CMAKE_SOURCE_DIR}/includes")
else()
  add_library(ppstatic STATIC ${ALL_SOURCES})
  target_include_directories(ppstatic PRIVATE "${CMAKE_SOURCE_DIR}/includes")
endif()
