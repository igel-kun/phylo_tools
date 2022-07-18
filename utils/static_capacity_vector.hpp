
#pragma once
#include "optional.hpp"

namespace mstd {
  // NOTE: all static_capacity vectors have their empty elements in the end!

  template<Optional T, size_t _capacity>
  struct size_counter {
    using Data = std::array<T, _capacity>;
    
    static bool empty(const Data& _data) { return !_data[0].has_value(); }

    static size_t size(const Data& _data) {
      for(size_t i = 0; i < _capacity; ++i)
        if(!_data[i].has_value())
          return i;
      return _capacity;
    }

    static void clear(Data& _data) {
      for(size_t i = 0; i < _capacity; ++i) _data[i].reset();
    }
    static void increase_by(size_t nr) { }
    static void erase_at(Data& _data, const size_t i) { _data[i].reset(); }

    template<class... Args>
    static void emplace_back(Data& data, Args&&... args) {
      const size_t i = size(data);
      if(i < _capacity) {
        data[i].emplace(std::forward<Args>(args)...);
      } else throw std::out_of_range("trying to add item to full static-capacity vector");
    }
    template<class _Data> requires (std::is_same_v<std::remove_cvref_t<_Data>, Data>)
    static auto get_begin(_Data&& _data) { return forward<_Data>(_data).begin(); }
    static auto get_end(const Data&) { return GenericEndIterator{}; }
  };

  template<class T, size_t _capacity>
  class size_storer {
    using Data = std::array<T, _capacity>;
    size_t _size = 0;
  public:
    using iterator = typename Data::iterator;

    bool empty(const Data&) const { return _size == 0; }
    size_t size(const Data&) const { return _size; }
    void clear(Data& _data) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for(size_t i = 0; i < _size; ++i)
          _data[i] = T{};
      }
      _size = 0;
    }
    void increase_by(size_t nr) { _size += nr; }
    static void erase_at(Data& _data, const size_t i) { _data[i] = T{}; }

    template<class... Args>
    void emplace_back(Data& data, Args&&... args) {
      if(_size < _capacity) {
        data[_size++] = T(std::forward<Args>(args)...);
      } else throw std::out_of_range("trying to add item to full static-capacity vector");
    }
    template<class... Args>
    bool emplace_back_if_possible(Data& data, Args&&... args) {
      if(_size < _capacity) {
        data[_size++] = T(std::forward<Args>(args)...);
        return true;
      } else return false;
    }


    template<class _Data> requires (std::is_same_v<std::remove_reference_t<_Data>, Data>)
    static auto get_begin(T&& _data) { return forward<_Data>(_data).begin(); }
    template<class _Data> requires (std::is_same_v<std::remove_reference_t<_Data>, Data>)
    static auto get_end(_Data&& _data) { return forward<_Data>(_data).end(); }
  };

  template<Optional T, size_t _capacity>
  bool operator==(const typename std::array<T, _capacity>::iterator it, const mstd::GenericEndIterator) { return !it->has_value(); }


  template<class T, size_t cap> struct _choose_size { using type = size_storer<T, cap>; };
  template<Optional T, size_t cap> struct _choose_size<T,cap> { using type = size_counter<T, cap>; };
  template<class T, size_t cap> using choose_size = typename _choose_size<T, cap>::type;


  // if T is an Optional type (std::optional or std::optional_by_invalid) then we will not store the size but loop over these, unless force_store_size == true
  template<class T, size_t _capacity, bool force_store_size = false>
  class _static_capacity_vector: public std::conditional_t<force_store_size, size_storer<T, _capacity>, choose_size<T, _capacity>> {
    using Size = std::conditional_t<force_store_size, size_storer<T, _capacity>, choose_size<T, _capacity>>;
    using Data = std::array<T, _capacity>;
    Data _data;
  public:
    using value_type      = typename Data::value_type;
    using reference       = typename Data::reference;
    using const_reference = typename Data::const_reference;
    using iterator       = typename Data::iterator;
    using const_iterator = typename Data::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    
    _static_capacity_vector() = default;
    _static_capacity_vector(const _static_capacity_vector& other): _data(other._data) {}
    _static_capacity_vector(_static_capacity_vector&&) = default;

    static constexpr size_t capacity() { return _capacity; }

    bool empty() const { return Size::empty(_data); }
    size_t size() const { return Size::size(_data); }
    void clear() { return Size::clear(_data); }

    template<class... Args>
    void push_back(Args&&... args) { Size::emplace_back(_data, std::forward<Args>(args)...); }
    template<class... Args>
    void emplace_back(Args&&... args) { Size::emplace_back(_data, std::forward<Args>(args)...); }
    // add a value if possible, or not if the container is full; return success
    template<class... Args>
    bool emplace_back_if_possible(Args&&... args) { return Size::emplace_back_if_possible(_data, std::forward<Args>(args)...); }

    auto begin() { return Size::get_begin(_data); }
    auto end() { return Size::get_end(_data); }
    auto begin() const { return Size::get_begin(_data); }
    auto end() const { return Size::get_end(_data); }
  };


  template<class T, size_t _capacity, bool force_store_size = false>
  using static_capacity_vector = std::conditional_t<Optional<T> && _capacity == 1,
        singleton_set<T>,
        _static_capacity_vector<T, _capacity, force_store_size>>;


}
