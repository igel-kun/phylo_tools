
// a dummy structure with a set interface that does not contain anything (even if you try to insert things)
#pragma once

namespace std {

  template<class T>
  struct emptyset: public iter_traits_from_reference<T&> {
    using Parent = iter_traits_from_reference<T&>;
    using iterator = typename Parent::pointer;

    static size_t size() { return 0; }
    static bool empty() { return true; }
    static size_t count(const T&) { return 0; }
    static iterator find(const T&) { return nullptr; }
    static iterator begin() { return nullptr; }
    static iterator end() { return nullptr; }
    static emplace_result<emptyset> emplace(const T&) { return {nullptr, false}; }
    void insert(const T&) {}
    static void clear() {}
    bool operator==(const emptyset<T>&) const { return true; }
    emptyset& operator=(const emptyset<T>&) { return *this; }
  };
}
