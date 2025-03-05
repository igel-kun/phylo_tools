
#pragma once

// automatic memoization, thanks to https://stackoverflow.com/a/24641745/6470423

using namespace mstd {

  template<class Sig, class F = Sig*>
  struct memoize_t;
  
  template<typename R, typename Arg, typename F>
  struct memoize_t<R(Arg), F> {
    F f;

    mutable std::unordered_map< Arg, R > results;

    template<typename... Args>
    auto& operator()(Args&&... args) const {
      Arg a{std::forward<Args>(args)... }; // in tuple version, std::tuple<...> a
      const auto it = results.find(a);
      if(it != results.end()) return it->second;
      return *(results.emplace( std::forward<Arg>(a), f(a)).first); // not sure what to do here in tuple version
    }
  };

  template<typename F>
  auto memoize(F* func) { return memoize_t<F>{func}; }

  /* here's how to use this automatic memoizer:
      int foo(int x) {
        static auto mem = memoize(foo); auto&& foo = mem; // <<--- this is the important addition to the code!!! Note that we're overwriting the function name
        std::cout << "processing...\n";                   
        if (x <= 0) return 1;
        if (x <= 2) return 1;
        return foo(x-1) + foo(x-2);;
      }

  */
}
