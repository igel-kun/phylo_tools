
#pragma once

// on debuglevel 3 all DEBUG1, DEBUG2, and DEBUG3 statements are evaluated
#ifndef NDEBUG
  #ifndef debuglevel
    #define debuglevel 5
  #endif
#else
  #define debuglevel 0
#endif


#if debuglevel > 0
#define DEBUG1(x) x
#else
#define DEBUG1(x)
#endif

#if debuglevel > 1
#define DEBUG2(x) x
#else
#define DEBUG2(x)
#endif

#if debuglevel > 2
#define DEBUG3(x) x
#else
#define DEBUG3(x)
#endif

#if debuglevel > 3
#define DEBUG4(x) x
#else
#define DEBUG4(x)
#endif

#if debuglevel > 4
#define DEBUG5(x) x
#else
#define DEBUG5(x)
#endif

#if debuglevel > 5
#define DEBUG6(x) x
#else
#define DEBUG6(x)
#endif

#ifdef STATISTICS
#define STAT(x) x
#else
#define STAT(x) 
#endif


