


#include <chrono>
#include <iostream>

namespace mstd {
  using namespace std::chrono;
  using Point = steady_clock::time_point;
  using Duration = steady_clock::duration;

  Point get_time() { return steady_clock::now(); }
  double ms_between(const Point& t1, const Point& t2) { return duration<double, std::milli>(t2-t1).count(); }

  template<class Storage>
  struct TimedCallResult {
    Storage s;
    double ms;
    
    template<class Call, class... Args>
    TimedCallResult(Call&& callable, Point p, Point q, Args&&...args):
      s(((p = get_time()) != Point()) ? callable(std::forward<Args>(args)...) : callable(std::forward<Args>(args)...)),
      ms(((q = get_time()) != Point()) ? ms_between(p, q) : 0.0)
    {}
    
    template<class Call, class First, class... Args> requires (!std::is_same_v<std::remove_cvref_t<First>, Point>)
    TimedCallResult(Call&& callable, First&& first, Args&&...args):
      TimedCallResult(std::forward<Call>(callable), Point(), Point(), std::forward<First>(first), std::forward<Args>(args)...)
    {}

    friend std::ostream& operator<<(std::ostream& os, const TimedCallResult& r) { return os << r.s << " [" << r.ms <<"ms]"; }

    operator double() const { return ms; }
  };

  template<class Call, class... Args> requires (std::is_void_v<std::invoke_result_t<Call, Args&&...>>)
  double timed_call(Call&& callable, Args&&... args) {
    auto p = get_time();
    callable(std::forward<Args>(args)...);
    auto q = get_time();
    return ms_between(p, q);
  }

  template<class Call, class... Args> requires (!std::is_void_v<std::invoke_result_t<Call, Args&&...>>)
  auto timed_call(Call&& callable, Args&&... args) {
    return TimedCallResult<std::invoke_result_t<Call, Args&&...>>(callable, std::forward<Args>(args)...);
  }

}
