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

