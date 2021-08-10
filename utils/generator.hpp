
#pragma once

#include <coroutine>

namespace std {

  template<class T>
  class generator {
  public:
    struct promise_type;
    using coro_handle = std::coroutine_handle<promise_type>;

    struct promise_type {
      T current_value;
      auto get_return_object() { return generator{coro_handle::from_promise(*this)}; }
      auto initial_suspend() const { return std::suspend_always{}; }
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

    bool is_valid() { return coro; }
    bool next() { return is_valid ? (coro.resume(), !coro.done()) : false; }
    generator& operator++() { if(!coro.done()) coro.resume(); return *this; }
    T& operator*() { return coro.promise().current_value; }
    const T& operator*() const { return coro.promise().current_value; }

    generator(const generator&) = delete;
    generator(generator &&other): coro(other.coro) { other.coro = nullptr; }
    ~generator() { if(coro) coro.destroy(); }
  };

}


using MyStack = stack<pair<Node*, typename vector<Node*>::iterator>>;
generator<Node*> dfs(Node* x){
    MyStack nodes;
    nodes.push({x, x->children.begin()});
    while(!nodes.empty()){
        auto& [np, it] = nodes.top();
        if(!(np->children.empty())) {
            ++it;
            nodes.push({np, np->children.begin()});
        } else {
            co_yield np;
            nodes.pop();
        }
    }
}



