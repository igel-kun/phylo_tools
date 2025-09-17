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
option (DFSCORO "enable corouting-based DFS implementation" ON)
option (MAXERR "max errors in debug mode" 3)
option (ASAN "enable address sanitizer" ON)
option (STATISTICS "collect statistics" OFF)
option (STATIC "enable static build" OFF)
option (FATAL "enable fatal errors" OFF)

IF(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(DEBUG 5 CACHE STRING "enable DEBUG mode" FORCE)
ENDIF()

message("Options: (enable with -D<option>=ON & disable with -D<option>=OFF)")
message("use graphite (GRAPHITE)          " ${GRAPHITE} )
message("debug mode (0=none) (DEBUG)      " ${DEBUG} )
message("coroutine-based DFS (DFSCORO)    " ${DFSCORO} )
message("max errors in debug mode (MAXERR)" ${MAXERR} )
message("build with address sanitizer     " ${ASAN} )
message("collect statistics (STATISTICS)  " ${STATISTICS} )
message("static build (STATIC)            " ${STATIC} )
message("fatal errors (FATAL)             " ${FATAL} )
message("Build type: " ${CMAKE_BUILD_TYPE} )

set (source_files examples CMakeLists.txt doc io solv utils AUTHORS.md BUGS.md COPYING.md INSTALL.md README.md THANKS.md)

add_custom_target ( archive tar -cjf phylo_tools.tar.bz2 ${source_files} )
add_custom_target ( doc doxygen Doxyfile )

include_directories ("${PROJECT_SOURCE_DIR}")

# Determine proper c++ standard flag based on compiler and version
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    set(STD_FLAG "-std=c++2a")
  else()
    set(STD_FLAG "-std=c++20")
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(STD_FLAG "-std=c++20")
  set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -Wno-unknown-warning-option")
else()
  set(STD_FLAG "-std=c++20") # fallback
endif()

# Only use certain modern warning flags if GCC ≥ 10
set(LEGACY_WARNINGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Wformat=2 -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
  message(STATUS "Restricting warnings to legacy due to old GCC version...")
  set(FLAG_WARNINGS "${LEGACY_WARNINGS}")
else()
  message(STATUS "Enabling warnings...")
  set(FLAG_WARNINGS "-Wdouble-promotion -Wno-unused-parameter -Wno-misleading-indentation -Wno-ignored-attributes -Wno-unused-function -Werror -Wno-error=cpp -Wno-error=effc++")
  # -Wnull-dereference
  set(FLAG_DIAGNOSTICS "-ftemplate-depth=100 -fdiagnostics-all-candidates -ftemplate-backtrace-limit=0 -fconcepts-diagnostics-depth=200 -ftemplate-backtrace-limit=200")
  if (GRAPHITE)
    message(STATUS "Enabling Graphite optimization flags...")
    set(GRAPHITE_CXX_FLAGS "-fgraphite-identity -funroll-loops -floop-flatten -floop-parallelize-all -floop-block -fweb -ftree-loop-if-convert -frename-registers -floop-interchange -floop-strip-mine -ftree-loop-distribution -ftree-loop-distribute-patterns -ftree-loop-ivcanon -fivopts -ftracer" )
  endif()
endif()


if(DEBUG)
  message(STATUS "building DEBUG version...")

  if (ASAN)
    message(STATUS "Enabling address sanitation flags...")
    set(SANITIZER_FLAGS "-fsanitize=address -fsanitize-address-use-after-scope")
  endif()
  if(FATAL)
    set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -Wfatal-errors")
  endif()
  if(STATISTICS)
    message(STATUS "adding support for collecting statistics...")
    set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DSTATISTICS")
  endif()

  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Ddebuglevel=${DEBUG}")
  message("CXX flags are: ${CMAKE_CXX_FLAGS_DEBUG}")
else()
  message(STATUS "building RELEASE version...")
  set(CMAKE_BUILD_TYPE RELEASE)
  message("CXX flags are: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# 5. Compose common flags
set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -pthread ${STD_FLAG} ${FLAG_WARNINGS} ${GRAPHITE_CXX_FLAGS}")

if(DFSCORO)
  message(STATUS "enabling coroutine-based DFS implementation...")
  set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DDFSCORO")
endif()

# static linking
if(STATIC)
  message(STATUS "enabling static linking...")
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  SET(BUILD_SHARED_LIBRARIES OFF)
  SET(CMAKE_EXE_LINKER_FLAGS "-static")
  set(CMAKE_COMMON_FLAGS_RELEASE "${CMAKE_COMMON_FLAGS} -O2 -s")
else()
  set(CMAKE_COMMON_FLAGS_RELEASE "${CMAKE_COMMON_FLAGS} -Ofast -march=native -m64 -fno-stack-protector")
endif()


set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_COMMON_FLAGS_RELEASE} -Wno-cpp -DNDEBUG -pipe -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_COMMON_FLAGS} ${SANITIZER_FLAGS} -fmax-errors=${MAXERR}")

ADD_EXECUTABLE( tests examples/tests.cpp )
ADD_EXECUTABLE( iso examples/iso.cpp )
ADD_EXECUTABLE( tc examples/tc.cpp )
ADD_EXECUTABLE( gen examples/gen.cpp )
ADD_EXECUTABLE( node_data examples/node_data.cpp )
ADD_EXECUTABLE( branch_len examples/branch_len.cpp )
ADD_EXECUTABLE( cuts examples/cuts.cpp )
ADD_EXECUTABLE( scanwidth examples/scanwidth.cpp )
ADD_EXECUTABLE( parsimony examples/parsimony.cpp )
ADD_EXECUTABLE( undir examples/undir.cpp )
ADD_EXECUTABLE( diversity examples/diversity.cpp )




