
#pragma once
// this is a null-terminated character pointer with small string optimization
// if the last 'reserved_bytes' bytes of it are all 0, then it's assumed to be a short-string
// NOTE: this means we cannot assign memory addresses that end in 8 zeros; we'd have to get new memory in that case...
#include <memory>
#include <cstring>
#include <string_view>

#include "config.hpp"
#include "utils.hpp"

namespace mstd {

  template<int _small_bytes = -2>
  union charp {
    static constexpr int _size = sizeof(char*);
    static constexpr int small_bytes = (_small_bytes < 0) ? (std::max(_size + _small_bytes, 0))  : (std::min(_small_bytes, _size - 1));
    static constexpr int reserved_bytes = _size - small_bytes;
    static constexpr int reserved_bits = CHAR_BIT * small_bytes;

    char* _data;
    char internal[_size];

    bool valid_ptr() const {
      if constexpr (small_bytes != 0) {
        for(int i = small_bytes; i < _size; ++i)
          if(internal[i] != 0) return true;
        return false;
      } else return _data != 0;
    }

    void make_new_data(const size_t new_size) {
      size_t tries_left = config::charp_allocation_timeout;
      while(1) {
        _data = reinterpret_cast<char*>(std::malloc(new_size));
        if(!valid_ptr()) {
          std::free(_data);
          if(!(--tries_left)) throw std::bad_alloc();
        } else break;
      } 
    }

    charp(std::string_view s) {
      size_t new_size = s.size();
      if(new_size > small_bytes) {
        const bool null_term = (s.back() == 0);
        // see if we need to 0-terminate our new string
        new_size += !null_term;
        // offset the memory acquisition slightly
        if(new_size % (1 << reserved_bits) == 0) ++new_size;
        make_new_data(new_size);
        std::copy_n(s.data(), s.size(), _data);
        if(!null_term) _data[s.size()] = 0;
      } else {
        _data = 0; // zero-out the char-array
        std::copy_n(s.data(), s.size(), internal);
      }
    }

    charp(charp&& other): _data{other._data} { other._data = 0; }

    charp(const charp& other) {
      if(other.valid_ptr()) {
        const size_t n = std::strlen(other._data);
        make_new_data(n + 1);
        std::copy_n(other._data, n + 1, _data);
      } else _data = other._data;
    }

    char* c_str() {
      if(valid_ptr()) 
        return _data;
      else return internal;
    }

    const char* c_str() const {
      if(valid_ptr()) 
        return _data;
      else return internal;
    }

    bool empty() const { return _data == 0; }

    void swap(charp& other) { std::swap(_data, other._data); } // this should work also when we're using the unique_ptr

    charp& operator=(const charp& other) { charp tmp(other); swap(tmp); return *this; }
    charp& operator=(charp&&) = default;

    size_t size() const { return length(); }
    size_t length() const { return std::string_view(*this).size(); }
//      if(valid_ptr()) {
//        return std::strlen(_data.get());
//      } else return std::strlen(internal);
//    }

    explicit operator std::string_view() const {
      if(valid_ptr()) {
        return std::string_view(_data);
      } else return std::string_view{internal};
    }

    ~charp() {
      if(valid_ptr()) std::free(_data);
    }
  };

  template<int small_bytes>
  auto& operator<<(std::ostream& os, const charp<small_bytes>& c) {
    return os << std::string_view(c);
  }
}
