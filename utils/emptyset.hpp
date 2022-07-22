
// a dummy structure with a set interface that does not contain anything (even if you try to insert things)
#pragma once

namespace mstd {

  template<class T>
  struct empty_set: public iter_traits_from_reference<T&> {
    using Parent = iter_traits_from_reference<T&>;
    using iterator = typename Parent::pointer;
    using const_iterator = typename Parent::const_pointer;

    static size_t size() { return 0; }
    static bool empty() { return true; }
    static size_t count(const T&) { return 0; }
    static iterator find(const T&) { return nullptr; }
    static iterator begin() { return nullptr; }
    static iterator end() { return nullptr; }
    template<class... Args>
    static emplace_result<empty_set> emplace(Args&&... args) { return {nullptr, false}; }
    static void insert(const T&) {}
    static void clear() {}
    bool operator==(const empty_set<T>&) const { return true; }
    empty_set& operator=(const empty_set<T>&) { return *this; }
  };

  template<class Key, class Value>
  struct empty_map: public iter_traits_from_reference<std::pair<Key, Value>&> {
    using Parent = iter_traits_from_reference<std::pair<Key, Value>&>;
    using iterator = typename Parent::pointer;
    using const_iterator = typename Parent::const_pointer;
    using key_type = Key;
    using mapped_type = Value;

    static size_t size() { return 0; }
    static bool empty() { return true; }
    static size_t count(const Key&) { return 0; }
    static iterator find(const Key&) { return nullptr; }
    static iterator begin() { return nullptr; }
    static iterator end() { return nullptr; }
    template<class... Args>
    static emplace_result<empty_map> emplace(Args&&... args) { return {nullptr, false}; }
    template<class... Args>
    static emplace_result<empty_map> try_emplace(Args&&... args) { return {nullptr, false}; }
    static void insert(const auto&) {}
    static void clear() {}
    Value& operator[](const Key&) const { throw std::logic_error("trying to access items of an empty map"); }
    bool operator==(const empty_map<Key, Value>&) const { return true; }
    empty_map& operator=(const empty_map<Key, Value>&) { return *this; }
  };

}
