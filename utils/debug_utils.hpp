
#pragma once

// on debuglevel 3 all DEBUG1, DEBUG2, and DEBUG3 statements are evaluated
#ifndef NDEBUG
#warning "compiling with debug output"
  #ifndef debuglevel
    #define debuglevel 5
  #endif
#else
  #define debuglevel 0
#endif

#define DEBUG0(x) x

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
#warning "compiling with statistics gathering/output"
#define STAT(x) x
#else
#define STAT(x) 
#endif

// these functions "turn off" the optimizer
// thanks to Chandler Carruth (https://www.youtube.com/watch?v=nXaxk27zwlk)
static void escape(void* p) { 
  // the first one "observes an object in memory" without instructions
  // this will bar the optimizer from optimizing out *p
  asm volatile("" : : "g"(p) : "memory");
}
static void clobber() { 
  // the second one "reads & writes all memory" without instructions
  asm volatile("" : : : "memory");
}


struct verbose_construct {
  int i;
  verbose_construct(const int j = -1): i(j) {
    std::cout << "==========================================\nconstructed debug sentinel #"<<i<<"\n=========================================\n";
  }
  ~verbose_construct() {
    std::cout << "==========================================\ndestructed debug sentinel #"<<i<<"\n=========================================\n";
  }
};


#include <string>
namespace mstd {
  // thanks @ https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c/56766138#56766138
  template <typename T>
  constexpr std::string_view type_name() {
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "std::string_view mstd::type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr std::string_view mstd::type_name() [with T = ";
    suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl mstd::type_name<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
  }
}
