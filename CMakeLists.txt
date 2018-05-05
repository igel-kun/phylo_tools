cmake_minimum_required(VERSION 2.6)
project (phylo_tools)
# The version number.
set (phylo_tools_VERSION_MAJOR 1)
set (phylo_tools_VERSION_MINOR 0)

if(POLICY CMP0012)
  CMAKE_POLICY(SET CMP0012 NEW)
endif(POLICY CMP0012)


option (GRAPHITE "build with graphite loop analysis" OFF)
option (DEBUG "debug mode (0=none, 9=max)" OFF)
option (STATISTICS "collect statistics" OFF)
option (STATIC "enable static build" OFF)

message("Options: (enable with -D<option>=ON & disable with -D<option>=OFF)")
message("use graphite (GRAPHITE)         " ${GRAPHITE} )
message("debug mode (DEBUG)              " ${DEBUG} )
message("collect statistics (STATISTICS) " ${STATISTICS} )
message("static build (STATIC)           " ${STATIC} )

set (source_files examples CMakeLists.txt doc io solv utils AUTHORS.md BUGS.md COPYING.md INSTALL.md README.md THANKS.md)

add_custom_target ( archive tar -cjf phylo_tools.tar.bz2 ${source_files} )
add_custom_target ( doc doxygen Doxyfile )

include_directories ("${PROJECT_SOURCE_DIR}")

if(${GRAPHITE} STREQUAL ON)
  message("setting graphite flags...")
  #set(GRAPHITE_CXX_FLAGS "-fgraphite-identity -floop-flatten -floop-parallelize-all -floop-block -fweb" )
  set(GRAPHITE_CXX_FLAGS "-fgraphite-identity -floop-flatten -floop-parallelize-all -floop-block -fweb -ftree-loop-if-convert -frename-registers -floop-interchange -floop-strip-mine -ftree-loop-distribution -ftree-loop-distribute-patterns -ftree-loop-ivcanon -fivopts -ftracer" )
endif(${GRAPHITE} STREQUAL ON)

message("GRAPHITE_CXX_FLAGS is ${GRAPHITE_CXX_FLAGS}")

# static linking
if(${STATIC} STREQUAL ON)
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  SET(BUILD_SHARED_LIBRARIES OFF)
  SET(CMAKE_EXE_LINKER_FLAGS "-static")
endif(${STATIC} STREQUAL ON)

# suppress clangs silly "unused parameter" warnings...
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -Qunused-arguments -Wno-unknown-warning")
endif()

set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -pthread -Wall -Wno-ignored-attributes -fmax-errors=2 -ftemplate-backtrace-limit=0 -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_COMMON_FLAGS} -O3 -march=native -Wno-cpp -DNDEBUG -pipe -m64 -fno-stack-protector ${GRAPHITE_CXX_FLAGS}")

if((${STATISTICS} EQUAL 1) OR (${STATISTICS} STREQUAL ON))
  MESSAGE("adding support for collecting statistics...")
  set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DSTATISTICS")
endif((${STATISTICS} EQUAL 1) OR (${STATISTICS} STREQUAL ON))

if((${DEBUG} EQUAL 0) OR (${DEBUG} STREQUAL OFF))
  set(CMAKE_BUILD_TYPE RELEASE)
else((${DEBUG} EQUAL 0) OR (${DEBUG} STREQUAL OFF))
  MESSAGE("building DEBUG version...")
  set(CMAKE_BUILD_TYPE DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_COMMON_FLAGS} -Ddebuglevel=${DEBUG}")
endif((${DEBUG} EQUAL 0) OR (${DEBUG} STREQUAL OFF))

ADD_EXECUTABLE( iso examples/iso.cpp )
ADD_EXECUTABLE( tc examples/tc.cpp )
ADD_EXECUTABLE( gen examples/gen.cpp )
ADD_EXECUTABLE( node_data examples/node_data.cpp )




