cmake_minimum_required(VERSION 2.8...9999.9)
project (phylo_tools)
# The version number.
set (phylo_tools_VERSION_MAJOR 1)
set (phylo_tools_VERSION_MINOR 0)

#if(POLICY CMP0012)
#  CMAKE_POLICY(SET CMP0012 NEW)
#endif()

option (GRAPHITE "build with graphite loop analysis" OFF)
option (DEBUG "debug mode (0=none, 9=max)" OFF)
option (STATISTICS "collect statistics" OFF)
option (STATIC "enable static build" OFF)
option (FATAL "enable fatal errors" OFF)
option (ASAN "enable address sanitizer" ON)

IF(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(DEBUG 5 CACHE STRING "enable DEBUG mode" FORCE)
ENDIF()

message("Options: (enable with -D<option>=ON & disable with -D<option>=OFF)")
message("use graphite (GRAPHITE)         " ${GRAPHITE} )
message("debug mode (DEBUG)              " ${DEBUG} )
message("build with address sanitizer    " ${ASAN} )
message("collect statistics (STATISTICS) " ${STATISTICS} )
message("static build (STATIC)           " ${STATIC} )
message("fatal errors (FATAL)            " ${FATAL} )
message("Build type: " ${CMAKE_BUILD_TYPE} )

set (source_files examples CMakeLists.txt doc io solv utils AUTHORS.md BUGS.md COPYING.md INSTALL.md README.md THANKS.md)

add_custom_target ( archive tar -cjf phylo_tools.tar.bz2 ${source_files} )
add_custom_target ( doc doxygen Doxyfile )

include_directories ("${PROJECT_SOURCE_DIR}")

if(${GRAPHITE})
  message("setting graphite flags...")
  #set(GRAPHITE_CXX_FLAGS "-fgraphite-identity -floop-flatten -floop-parallelize-all -floop-block -fweb" )
  set(GRAPHITE_CXX_FLAGS "-fgraphite-identity -funroll-loops -floop-flatten -floop-parallelize-all -floop-block -fweb -ftree-loop-if-convert -frename-registers -floop-interchange -floop-strip-mine -ftree-loop-distribution -ftree-loop-distribute-patterns -ftree-loop-ivcanon -fivopts -ftracer" )
endif()

# static linking
if(${STATIC})
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  SET(BUILD_SHARED_LIBRARIES OFF)
  SET(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

# suppress clangs silly "unused parameter" warnings...
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -Wno-unknown-warning")
endif()


if(${FATAL})
  set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -Wfatal-errors")
endif()

if(${STATISTICS})
  MESSAGE("adding support for collecting statistics...")
  set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DSTATISTICS")
endif()

if(${ASAN})
  MESSAGE("enabling address sanitizer...")
  set(PROTOTYPE_SANITIZER "-fsanitize=address -fsanitize-address-use-after-scope")
endif()

set(PROTOTYPE_FLAGS "-fmax-errors=3 -ftemplate-backtrace-limit=0 -fconcepts-diagnostics-depth=200 -ftemplate-backtrace-limit=200 -fno-omit-frame-pointer")
set(PROTOTYPE_WARNINGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast -Wno-unused-parameter -Wno-misleading-indentation -Werror -Wno-error=cpp -Wno-error=effc++ -Wshadow -Wnon-virtual-dtor -Wno-ignored-attributes -Wno-unused-function -Wshadow")

set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -pthread -std=c++20 -fcoroutines -Wno-range-loop-construct -ftemplate-depth=100")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_COMMON_FLAGS} -O3 -march=native -Wno-cpp -DNDEBUG -pipe -m64 -fno-stack-protector ${GRAPHITE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_COMMON_FLAGS} ${PROTOTYPE_WARNINGS} ${PROTOTYPE_SANITIZER} ${PROTOTYPE_FLAGS}")


if(${DEBUG})
  MESSAGE("building DEBUG version...")
  set(CMAKE_BUILD_TYPE DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Ddebuglevel=${DEBUG}")
  message("CXX flags are: ${CMAKE_CXX_FLAGS_DEBUG}")
else()
  MESSAGE("building RELEASE version...")
  set(CMAKE_BUILD_TYPE RELEASE)
  message("CXX flags are: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()


ADD_EXECUTABLE( iso examples/iso.cpp )
ADD_EXECUTABLE( tc examples/tc.cpp )
ADD_EXECUTABLE( gen examples/gen.cpp )
ADD_EXECUTABLE( node_data examples/node_data.cpp )
ADD_EXECUTABLE( branch_len examples/branch_len.cpp )
ADD_EXECUTABLE( cuts examples/cuts.cpp )
ADD_EXECUTABLE( scanwidth examples/scanwidth.cpp )




