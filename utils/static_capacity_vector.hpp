
#pragma once
#include "optional.hpp"
#include "tight_int.hpp"

namespace mstd {

  // thanks to David Stone: https://www.youtube.com/watch?v=I8QJLGI0GOE
  template<class T, size_t capacity>
  struct uninitialized_array {
    using size_type = mstd::uint_tight<capacity>;
    using iterator = T*;
    using const_iterator = const T*;
 
    static constexpr bool is_sufficiently_trivial =
      std::is_trivially_default_constructible_v<T> and std::is_trivially_destructible_v<T>;
  
    using storage_type = std::conditional_t<is_sufficiently_trivial,
          std::array<T, capacity>,
          std::aligned_storage_t<sizeof(T) * capacity, alignof(T)>>;

  protected:
    [[no_unique_address]] storage_type storage;
  public:

    constexpr auto data() noexcept {
      if constexpr (is_sufficiently_trivial)
        return static_cast<iterator>(storage.data());
      else return reinterpret_cast<iterator>(std::addressof(storage));
    }
    constexpr auto data() const noexcept {
      if constexpr (is_sufficiently_trivial)
        return static_cast<const_iterator>(storage.data());
      else return reinterpret_cast<const_iterator>(std::addressof(storage));
    }

    constexpr decltype(auto) operator[](const size_type& key) noexcept {
      assert(key < capacity);
      return *(data() + key);
    }
    constexpr decltype(auto) operator[](const size_type& key) const noexcept {
      assert(key < capacity);
      return *(data() + key);
    }
  };


  // NOTE: static_capacity_vector with size by optional DOES NOT SUPPORT O(1) PUSH_BACK since it has to find the spot to push to first
  template<Optional T, size_t _capacity>
  struct size_counter {
    static constexpr size_t capacity = _capacity;
    using Data = std::array<T, capacity>;
    using size_type = typename Data::size_type;

    static constexpr bool empty(const Data& _data) noexcept { return !_data[0].has_value(); }

    // find the last used item by binary search
    static constexpr size_type size(const Data& _data) noexcept {
      // check first if it's maybe empty
      if constexpr (_capacity < 4u) {
        if(!_data[0u].has_value()) return 0u;
        if(!_data[1u].has_value()) return 1u;
        if(!_data[2u].has_value()) return 2u;
        if(!_data[3u].has_value()) return 3u;
        return 4u;
      } else {
        const auto iter = _data.data();
        const auto first_empty = std::upper_bound(iter, iter + _capacity, false, [](const auto& o){ return !o.has_value(); });
        return std::distance(iter, first_empty);
      }
    }

    static constexpr void clear(Data& _data) noexcept (std::is_nothrow_destructible_v<T>) {
      for(size_t i = 0; i < _capacity; ++i) _data[i].reset();
    }
    static constexpr void increase_by(size_t nr) noexcept { }
    static constexpr void zero() noexcept { }
    static constexpr void erase_at(Data& _data, const size_t i) noexcept (std::is_nothrow_destructible_v<T>) { _data[i].reset(); }

    template<class... Args>
    static constexpr void emplace_back(Data& data, Args&&... args) {
      const size_t i = size(data);
      if(i < _capacity) {
        data[i].emplace(std::forward<Args>(args)...);
      } else throw std::bad_alloc("trying to add item to full static-capacity vector");
    }
    template<class... Args>
    static constexpr bool emplace_back_if_possible(Data& data, Args&&... args)
      noexcept (std::is_nothrow_constructible_v<T, Args&&...>)
    {
      const size_t i = size(data);
      if(i < _capacity) {
        data[i].emplace(std::forward<Args>(args)...);
        return true;
      } else return false;
    }

    template<class _Data> requires (std::is_same_v<std::remove_cvref_t<_Data>, Data>)
    static constexpr auto get_begin(_Data&& _data) noexcept { return forward<_Data>(_data).begin(); }
    static constexpr auto get_end(const Data&) noexcept { return GenericEndIterator{}; }

    friend bool operator==(const typename uninitialized_array<T, _capacity>::iterator it, const mstd::GenericEndIterator)
    { return !it->has_value(); }
  };


  template<class T, size_t _capacity>
  class size_storer {
  public:
    using Data = uninitialized_array<T, _capacity>;
    using size_type = mstd::uint_tight<_capacity>;
  protected:
    size_type _size = 0u;
  public:
    using iterator = typename Data::iterator;

    constexpr bool empty(const Data&) const noexcept { return _size == 0; }
    constexpr size_t size(const Data&) const noexcept { return _size; }
    constexpr void clear(Data& _data) noexcept (std::is_nothrow_destructible_v<T>) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for(size_type i = 0; i < _size; ++i)
          std::destroy_at(_data.data() + i);
      }
      _size = 0;
    }
    constexpr void increase_by(size_t nr) noexcept { _size += nr; }
    constexpr void zero() noexcept { _size = 0; }

    static constexpr void erase_at(Data& _data, const size_type i) noexcept {
      std::destroy_at(_data.data() + i);
    }

    template<class... Args>
    constexpr void emplace_back(Data& data, Args&&... args) {
      if(_size < _capacity) {
        std::construct_at(data.data() + _size, std::forward<Args>(args)...);
        ++_size;
      } else throw std::bad_alloc("trying to add item to full static-capacity vector");
    }
    template<class... Args>
    constexpr bool emplace_back_if_possible(Data& data, Args&&... args) noexcept{
      if(_size < _capacity) {
        std::construct_at(data.data() + _size, std::forward<Args>(args)...);
        ++_size;
        return true;
      } else return false;
    }

    template<class _Data> requires (std::is_same_v<std::remove_reference_t<_Data>, Data>)
    static constexpr auto get_begin(T&& _data) noexcept { return forward<_Data>(_data).begin(); }
    template<class _Data> requires (std::is_same_v<std::remove_reference_t<_Data>, Data>)
    static constexpr auto get_end(_Data&& _data) noexcept { return forward<_Data>(_data).end(); }
  };


  template<class T, size_t cap> struct _choose_size { using type = size_storer<T, cap>; };
  template<Optional T, size_t cap> struct _choose_size<T,cap> { using type = size_counter<T, cap>; };
  template<class T, size_t cap> using choose_size = typename _choose_size<T, cap>::type;


  // if T is an Optional type (std::optional or std::optional_by_invalid)
  // then we will not store the size but loop over these, unless force_store_size == true
  template<class T, size_t _capacity, bool force_store_size = false>
  class _static_capacity_vector: public std::conditional_t<force_store_size, size_storer<T, _capacity>, choose_size<T, _capacity>> {
    using Size = std::conditional_t<force_store_size, size_storer<T, _capacity>, choose_size<T, _capacity>>;
    using Data = typename Size::Data;
    Data _data;
    [[no_unique_address]] Size _size;
  public:
    using value_type      = typename Data::value_type;
    using reference       = typename Data::reference;
    using const_reference = typename Data::const_reference;
    using iterator       = typename Data::iterator;
    using const_iterator = typename Data::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type = typename Size::size_type;
    
    constexpr _static_capacity_vector() = default;
    constexpr _static_capacity_vector(const _static_capacity_vector& other): _data(other._data) {}
    constexpr _static_capacity_vector(_static_capacity_vector&&) requires std::is_trivially_move_constructible_v<T> = default;
    constexpr _static_capacity_vector(_static_capacity_vector&& other) requires (!std::is_trivially_move_constructible_v<T>) {
      std::uninitialized_move(other.begin(), other.end(), begin());
      _size.increaese_by(other.size());
    }

    friend constexpr void swap(_static_capacity_vector& lhs, _static_capacity_vector& rhs) {
      if(&lhs != &rhs){
        std::swap_ranges(lhs.begin(), lhs.end(), rhs.begin());
        std::swap(lhs._size, rhs._size);
      }
    }

    static constexpr size_type capacity() { return _capacity; }

    constexpr void pop_back() {
      assert(!empty());
      _size.erase_at(_data, size() - 1);
      _size.increase_by(-1);
    }
    bool empty() const { return _size.empty(_data); }
    size_t size() const { return _size.size(_data); }
    void clear() { return _size.clear(_data); }

    template<class... Args>
    void push_back(Args&&... args) { _size.emplace_back(_data, std::forward<Args>(args)...); }
    template<class... Args>
    void emplace_back(Args&&... args) { _size.emplace_back(_data, std::forward<Args>(args)...); }
    // add a value if possible, or not if the container is full; return success
    template<class... Args>
    bool emplace_back_if_possible(Args&&... args) { return _size.emplace_back_if_possible(_data, std::forward<Args>(args)...); }

    auto begin() { return _size.get_begin(_data); }
    auto end() { return _size.get_end(_data); }
    auto begin() const { return _size.get_begin(_data); }
    auto end() const { return _size.get_end(_data); }
  };


  template<class T, size_t _capacity, bool force_store_size = false>
  using static_capacity_vector = std::conditional_t<Optional<T> && _capacity == 1,
        singleton_set<T>,
        _static_capacity_vector<T, _capacity, force_store_size>>;


}
