
#pragma once

#include <coroutine>
#include "stl_utils.hpp"

namespace mstd {

  template<class T>
  class generator: public iter_traits_from_reference<T> {
    using Traits = iter_traits_from_reference<T>;
  public:
    using typename Traits::reference;
    using typename Traits::const_reference;
    using typename Traits::pointer;
    using typename Traits::const_pointer;

    struct promise_type;
    using coro_handle = std::coroutine_handle<promise_type>;

    struct promise_type {
      using TempStorage = std::conditional_t<Traits::returning_rvalue, T, std::reference_wrapper<std::remove_reference_t<T>>>;

      TempStorage current_value;
      auto get_return_object() { return generator{coro_handle::from_promise(*this)}; }
      auto initial_suspend() const noexcept { return std::suspend_never{}; }
      auto final_suspend() const noexcept { return std::suspend_always{}; }
      void unhandled_exception() const { std::terminate(); }

      template<class Q>
      auto yield_value(Q&& value) {
        current_value = std::forward<Q>(value);
        return std::suspend_always{};
      }
    };

  private:
    coro_handle coro;
    
    generator(coro_handle h): coro(h) {}
  public:

    bool is_valid() const { return !coro.done(); }
    bool next() { return is_valid() ? (coro.resume(), is_valid()) : false; }
    generator& operator++() { if(is_valid()) coro.resume(); return *this; }
    
    reference operator*() { return coro.promise().current_value; }
    const_reference operator*() const { return coro.promise().current_value; }
    pointer operator->() {
      if constexpr(Traits::returning_rvalue)
        return operator*();
      else return &(operator*().get());
    }
    const_pointer operator->() const {
      if constexpr(Traits::returning_rvalue)
        return operator*();
      else return &(operator*().get());
    }

    generator(generator &&other): coro(move(other.coro)) {
      std::cout << "move constructing generator\n";
      other.coro = nullptr;
    }
    generator& operator=(generator&& other) {
      coro = move(other.coro);
      std::cout << "move assigning generator\n";
      other.coro = nullptr;
      return *this;
    }

    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;
    ~generator() { if(coro) coro.destroy(); }
  };

}


