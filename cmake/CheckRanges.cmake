include(CheckCXXSourceCompiles)
include(CheckIncludeFileCXX)

message(STATUS "C++ compiler     : ${CMAKE_CXX_COMPILER}")
message(STATUS "Compiler ID      : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version : ${CMAKE_CXX_COMPILER_VERSION}")

set(CMAKE_REQUIRED_FLAGS "-std=c++20")

check_include_file_cxx(ranges HAVE_RANGES_HEADER)

if(NOT HAVE_RANGES_HEADER)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
      message(FATAL_ERROR
        "\n"
        "This project requires C++20 <ranges> support and some features that crash g++ before version 13.\n"
        "\n"
        "Detected GCC ${CMAKE_CXX_COMPILER_VERSION}, which is too old.\n"
        "\n"
        "Try one of:\n"
        "  module load gcc/13\n"
        "  export CXX=g++-13\n"
      )
    endif()
  endif()

  message(FATAL_ERROR
    "\n"
    "Compiler cannot find <ranges>.\n"
    "Please use a newer C++20 toolchain.\n"
  )
endif()

check_cxx_source_compiles("
  #include <ranges>
  #include <vector>

  int main() {
    std::vector<int> v{1,2,3};

    auto r = v | std::ranges::views::filter([](int x) {
      return x % 2 == 1;
    });

    return 0;
  }
" HAVE_WORKING_RANGES)

if(NOT HAVE_WORKING_RANGES)
  message(FATAL_ERROR
    "\n"
    "<ranges> exists but functional ranges support failed.\n"
    "This is usually caused by an outdated libstdc++.\n"
  )
endif()

message(STATUS "Working C++20 ranges support detected.")
