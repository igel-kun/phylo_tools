//
// platform specific code
//

#pragma once

// windows
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   //define something for Windows (32-bit and 64-bit, this part is common)
   #ifdef _WIN64
      //define something for Windows (64-bit only)
   #else
      //define something for Windows (32-bit only)
   #endif
#endif

// macOS
#if __APPLE__
#else
#endif
  
// android
#if __ANDROID__
    // Below __linux__ check should be enough to handle Android,
    // but something may be unique to Android.
#endif

// linux
#if __linux__
    // linux
#endif

// unix
#if __unix__ // all unices not caught above
    // Unix
#endif

// posix compliant
#if defined(_POSIX_VERSION)
    // POSIX
#endif


// we will make a number of checks against versions of GCC and clang under which certain things do and don't work...
// for example, clang-12 does not have 'from_chars' :(
#ifdef __GNUC__
#   define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#   define GCC_VERSION 0
#endif

#ifdef __clang__
#   define CLANG_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevelL__)
#else
#   define CLANG_VERSION 0
#endif

