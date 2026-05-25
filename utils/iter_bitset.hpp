
#pragma once

#include <cstdint>
#include <set>
#include <initializer_list>
#include "utils.hpp"
#include "runes.hpp"
#include "stl_utils.hpp"
#include "raw_vector_map.hpp"

namespace mstd {

  // ========== iterable_bitset ==========
  // This is a dynamic and iterable bitset,
  // much like vector<bool> but with an underlying unordered_map
  // so if your bitset only contains the value 10^6, we'll only reserve 1 word of memory

  // ------- iterable_bitset: helpers ---------
  using iter_bitset_default_key = size_t;
  using iter_bitset_default_bucket = uint_fast64_t; //uint64_t;
  using iter_bitset_default_bucket_map = mstd::raw_vector_map<iter_bitset_default_key, iter_bitset_default_bucket>;

  // bucket_map_traits provide some common traits for the given bucket-map
  template<MapType bucket_map>
  struct bucket_map_traits  {
    using bucket_type = typename bucket_map::mapped_type;
    using bucket_iter = typename bucket_map::iterator;
    using bucket_const_iter = typename bucket_map::const_iterator;
    using value_type = mapped_type_of_t<bucket_map>;

    static constexpr value_type full_bucket = ~(static_cast<bucket_type>(0ul));
    static constexpr size_t num_bytes_in_bucket = sizeof(bucket_type);
    static constexpr size_t num_bits_in_bucket = CHAR_BIT * sizeof(bucket_type);
    static constexpr size_t log_bits_in_bucket = NUM_TRAILING_ZEROSL(num_bits_in_bucket);
    static constexpr bool bucket_size_is_pow_of_two = ((1ul << log_bits_in_bucket) == num_bits_in_bucket);

    static constexpr size_t bucket_of(const auto x) {
      if constexpr (bucket_size_is_pow_of_two) {
        return x >> log_bits_in_bucket;
      } else {
        return x / num_bits_in_bucket;
      }
    }
    static constexpr size_t pos_of(const auto x) {
      if constexpr (bucket_size_is_pow_of_two) {
        return x & (num_bits_in_bucket - 1);
      } else {
        return x % num_bits_in_bucket;
      }
    }
    // return bucket number and offset in that bucket of the given item
    static constexpr auto bucket_and_pos_of(const auto x) {
      return std::pair{bucket_of(x), pos_of(x)};
    }
  };


  // ------- iterable_bitset: concepts ---------
  template<class T>
  concept StrictIterBitsetType = requires(T t){
    typename T::bucket_map;
    requires MapType<typename T::bucket_map>;
    { t.test(0) } -> std::same_as<bool>;
  };
  template<class T, TypeRune rune = TR_ConstRefOK>
  constexpr bool is_iterable_bitset = apply_rune_v<T, rune> or StrictIterBitsetType<apply_rune_t<T, rune>>;
  
  template<class T, TypeRune rune = TR_ConstRefOK>
  concept IterBitsetType = is_iterable_bitset<T, rune>;


  // ------- iterable_bitset: iterator ---------
  // NOTE: we do not correspond to the official standard since we do not abide by the following condition:
  // "if a and b compare equal then either they are both non-dereferenceable or *a and *b are references bound to the same object"
  // since our *-operation does not return a reference, but an integer
  // however, iteration a la "for(auto i: my_set)" works very well...
  template<MapType bucket_map = iter_bitset_default_bucket_map>
  struct bitset_iterator:
    public iter_traits_from_reference<mapped_type_of_t<bucket_map>>,
    public bucket_map_traits<bucket_map>
  {
    // ------- static stuff --------
    using traits = iter_traits_from_reference<mapped_type_of_t<bucket_map>>;
    using bmap_traits = bucket_map_traits<bucket_map>;

    using typename bmap_traits::bucket_type;
    using bucket_iter = typename bucket_map::const_iterator;

    using typename traits::value_type;
    using typename traits::reference;
    using typename traits::const_reference;
    using typename traits::pointer;
    using typename traits::const_pointer;

    // ------- members --------
  protected:
    const bucket_map* storage;
    bucket_iter index;
    bucket_type buffer;

    // ------- construction & desctruction ---------
  public:
    bitset_iterator(const bucket_map& _storage, const bucket_iter& _index) noexcept:
      storage(&_storage), index(_index)
    { advance_while_empty(); }
    
    bitset_iterator(const bucket_map& _storage) noexcept:
      bitset_iterator(_storage, _storage.begin())
    {}

    // create an iterator at a specific bit inside storage[index]
    bitset_iterator(const bucket_map& _storage, const bucket_iter& _index, const unsigned char sub_index) noexcept:
      bitset_iterator(_storage, _index)
    {
      // if we didn't advance the buffer, advance the sub-index inside the buffer
      if(index == _index)
        buffer &= (bmap_traits::full_bucket << sub_index);
    }

    // ------- operators --------
    explicit operator bool() const { return is_valid(); }
    reference operator*() const { return (*index).first * bmap_traits::num_bits_in_bucket + NUM_TRAILING_ZEROSL(buffer); }

    bitset_iterator& operator++() {
      buffer ^= (1ul << NUM_TRAILING_ZEROSL(buffer));
      if(!buffer) {
        ++index;
        advance_while_empty();
      }
      return *this;
    }

    bitset_iterator operator++(int) { bitset_iterator result(*this); ++(*this); return result; }

    bool operator==(const bitset_iterator& it) const {
      const bool we_at_end = !is_valid();
      const bool they_at_end = !it.is_valid();
      if(we_at_end != they_at_end) return false;
      if(we_at_end and they_at_end) return true;
      return (index == it.index) and (buffer == it.buffer);
    }

    // ------- methods: initialization --------
    // ------- methods: query --------
  public:
    bool is_valid() const { return index != storage->end(); }
    bool is_invalid() const { return not is_valid(); }

    // ------- methods: modification --------
  protected:
    // advance the index while its buffer is empty
    // NOTE: this overwrites the current buffer, so make sure it's zero before
    void advance_while_empty() {
      while(is_valid()) {
        buffer = (*index).second;
        if(buffer) return;
        ++index;
      }
    }
  };


  // ------- iterable_bitset: main class ---------
  // ATTENTION: this does not do error checking if NDEBUG is on (except front())
  // NOTE: bitsets cannot provide meaningful references to their members
  template<MapType _bucket_map = iter_bitset_default_bucket_map>
  struct iterable_bitset:
    public iter_traits_from_reference<mapped_type_of_t<_bucket_map>>,
    public bucket_map_traits<_bucket_map>
  {
#warning "TODO: add small-string optimization!"
    // ------- static stuff --------
    using traits = iter_traits_from_reference<mapped_type_of_t<_bucket_map>>;
    using bmap_traits = bucket_map_traits<_bucket_map>;

    using bucket_map = _bucket_map;
    using typename bmap_traits::bucket_type;
    using typename bmap_traits::bucket_iter;
    using typename bmap_traits::bucket_const_iter;

    using typename traits::value_type;
    using typename traits::reference;
    using typename traits::const_reference;
    using typename traits::pointer;
    using typename traits::const_pointer;

    using iterator = bitset_iterator<bucket_map>;
    using const_iterator = iterator;
    using emplace_result = std::pair<iterator, bool>;

    // ------- members --------
  protected:
    size_t _capacity = 0;
    size_t _count = 0;
    bucket_map storage;

    // ------- construction & desctruction ---------
  public:
    iterable_bitset() noexcept = default;

    explicit iterable_bitset(const size_t _num_bits, const bool _set_all):
      _capacity(_num_bits), storage()
    {
      if(_set_all) set_all();
    }

    explicit iterable_bitset(const size_t _num_bits):
      iterable_bitset(_num_bits, false)
    {}
    
    // construct with some items
    template<IterableType C>
    explicit iterable_bitset(C&& init, const size_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {
      for(const auto& x: init) set(x);
    }

    template<IterableType InitSet_>
    explicit iterable_bitset(const typename InitSet_::const_iterator _begin, const typename InitSet_::const_iterator _end, const size_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {
      for(typename InitSet_::const_iterator i = _begin; i != _end; ++i) set(*i);
    }

    // make from another iterable_bitset (different bucket map)
    template<class BMap> requires (not mstd::is_same_v<BMap, bucket_map>)
    iterable_bitset(const iterable_bitset<BMap>& other) {
      for(const auto& xy: other.data())
        storage.emplace(xy.first, xy.second);      
    }

    iterable_bitset(const iterable_bitset&) = default;
    iterable_bitset(iterable_bitset&&) noexcept = default;

    // ------- operators --------
    iterable_bitset& operator=(iterable_bitset&& bs) noexcept = default;
    iterable_bitset& operator=(const iterable_bitset& bs) = default;

    template<class BMap> requires (not mstd::is_same_v<BMap, bucket_map>)
    iterable_bitset& operator=(const iterable_bitset<BMap>& other) {
      iterable_bitset tmp(other);
      return operator=(std::move(tmp));
    }

    iterable_bitset& operator&=(const iterable_bitset& bs) {
      auto _iter = storage.begin();
      auto _end = storage.cend();
      auto bs_end = bs.storage.cend();
      _count = 0;
      while(_iter != _end){
        bucket_type& my_bucket = (*_iter).second;
        const auto bs_iter = bs.storage.find((*_iter).first);
        if(bs_iter != bs_end){
          my_bucket &= (*bs_iter).second;
          if(my_bucket) {
            _count += NUM_ONES_INL(my_bucket);
            ++_iter;
          } else storage.erase(_iter++);
        } else storage.erase(_iter++);
      }
      return *this;
    }
   
    iterable_bitset& operator^=(const iterable_bitset& bs) {
      // XOR-in all elements of bs
      for(const auto& bs_i: bs.storage){
        const auto& their_bucket = (*bs_i).second;
        const auto _iter = storage.find(bs_i.first);
        if(_iter != storage.end()){
          bucket_type& my_bucket = (*_iter).second;
          // bs_i->first is also in our storage, so XOR them
          _count -= NUM_ONES_INL(my_bucket);
          if(my_bucket != their_bucket){
            my_bucket ^= their_bucket;
            _count += NUM_ONES_INL(my_bucket);
          } else storage.erase(_iter);
        } else {
          storage[(*bs_i).first] = their_bucket;
          _count += NUM_ONES_INL(their_bucket);
        }
      }
      return *this;
    }

    iterable_bitset& operator|=(const iterable_bitset& bs) {
      for(const auto& bs_key_value: bs.storage){
        auto _iter = storage.find(bs_key_value.first);
        if(_iter == storage.end()){
          storage.emplace_hint(_iter, bs_key_value.first, bs_key_value.second);
          _count += NUM_ONES_INL(bs_key_value.second);
        } else {
          bucket_type& my_bucket = (*_iter).second;
          _count -= NUM_ONES_INL(my_bucket);
          my_bucket |= bs_key_value.second;
          _count += NUM_ONES_INL(my_bucket);
        }
      }
      return *this;
    }

    iterable_bitset& operator-=(const iterable_bitset& bs) {
      for(const auto& bs_key_value: bs.storage){
        auto _iter = storage.find(bs_key_value.first);
        if(_iter != storage.end()){
          bucket_type& my_bucket = (*_iter).second;
          _count -= NUM_ONES_INL(my_bucket);
          my_bucket &= ~(bs_key_value.second);
          _count += NUM_ONES_INL(my_bucket);
        }
      }
      return *this;
    }


    template<IterableType Set>
    bool operator==(const Set& s) const {
      if(size() == s.size()) {
        for(const value_type x: *this)
          if(!test(s, x)) return false;
      } else return false;
      return true;
    }

    template<class T>
    bool operator==(const iterable_bitset<T>& bs) const {
      if(_count != bs._count) return false;
      return storage == bs.storage;
    }
     
    template<mstd::IterableType Container> requires (not IterBitsetType<Container>)
    iterable_bitset& operator&=(const Container& c) {
      auto _iter = begin();
      const auto _end = end();
      while(_iter != _end){
        const value_type i = *_iter;
        if(!c.count(i)) clear(i);
        ++_iter;
      }
      return *this;
    }

    template<mstd::IterableType Container> requires (not IterBitsetType<Container>)
    iterable_bitset& operator^=(const Container& c) {
      for(const auto& i: c) flip(i);
      return *this;
    }

    template<mstd::IterableType Container> requires (not IterBitsetType<Container>)
    iterable_bitset& operator|=(const Container& c) {
      for(const auto& i: c) insert(i);
      return *this;
    }
    template<mstd::IterableType Container> requires (not IterBitsetType<Container>)
    iterable_bitset& operator-=(const Container& c) {
      for(const auto& i: c) erase(i);
      return *this;
    }

    template<mstd::IterableType Container>
    iterable_bitset operator&(const Container& c) const {
      iterable_bitset result(*this);
      result &= c;
      return result;
    }
    template<mstd::IterableType Container>
    iterable_bitset operator^(const Container& c) const {
      iterable_bitset result(*this);
      result ^= c;
      return result;
    }
    template<mstd::IterableType Container>
    iterable_bitset operator|(const Container& c) const {
      iterable_bitset result(*this);
      result |= c;
      return result;
    }
    template<mstd::IterableType Container>
    iterable_bitset operator-(const Container& c) const {
      iterable_bitset result(*this);
      result -= c;
      return result;
    }


    // ------- methods: initialization --------
    // ------- methods: query --------
  public:
    using bmap_traits::full_bucket;
    using bmap_traits::num_bytes_in_bucket;
    using bmap_traits::num_bits_in_bucket;
    using bmap_traits::log_bits_in_bucket;
    using bmap_traits::bucket_size_is_pow_of_two;    
    using bmap_traits::bucket_of;
    using bmap_traits::pos_of;
    using bmap_traits::bucket_and_pos_of;

    size_t num_buckets() const { return storage.size(); }
    const bucket_map& data() const { return storage; }
    size_t capacity() const { return _capacity; }
    size_t count() const { return _count; }    
    bool count(const value_type x) const { return test(x); }
    bool contains(const value_type x) const { return test(x); }
    size_t size() const { return count(); }
    bool empty() const { return _count == 0; }
    value_type front() const { return *begin(); }
    bool full() const { return _capacity == _count; }

    bool test(const value_type x) const {
      const auto it = storage.find(bucket_of(x));
      if(it != storage.end())
        return (*it).second & (1ul << pos_of(x));
      else
        return false;
    }

    iterator begin() const { return bitset_iterator<bucket_map>(storage); }
    iterator end() const { return bitset_iterator<bucket_map>(storage, storage.end()); }
    iterator cbegin() const { return begin(); }
    iterator cend() const { return end(); }

    iterator find(const value_type x) const {
      if(test(x)) {
        const auto [bucket_num, bucket_offset] = bucket_and_pos_of(x);
        return bitset_iterator<bucket_map>(storage, storage.find(bucket_num), bucket_offset);
      } else return end();
    }

    // ------- methods: modification --------
    void emplace_back(const bool bit) { if(bit) emplace(capacity()); else ++_capacity; }
    auto emplace(const value_type x) { const bool res = set(x); return std::pair{find(x), res}; }
    auto insert(const value_type x) { return emplace(x); }
    bool erase(const value_type x) { return clear(x); }
    bool unset(const value_type x) { return clear(x); }
    bool set(const value_type x, const bool value) { if(value) return set(x); else return clear(x); }
    void invert() { flip_all(); }

    template<class Iterator_>
    void insert(Iterator_ start, const Iterator_& finish) { while(start != finish) { insert(*start); ++start; } }

    // set a bit & return whether the size changed (that is, if it wasn't set before)
    bool set(const value_type x) {
      bucket_type bit_set = (1ul << pos_of(x));
      const auto [iter, success] = storage.try_emplace(bucket_of(x), bit_set);
      if(!success){
        bucket_type& bucket = (*iter).second;
        if(bucket & bit_set) return false; else bucket |= bit_set;
      }
      ++_count;
      if(_capacity <= x) _capacity = x + 1;
      return true;
    }

    // clear a bit and return whether the size changed (that is, if it was set before)
    bool clear(const value_type x) {
      if(x < _capacity){
        const bucket_iter it = storage.find(bucket_of(x));
        if(it != storage.end()){
          bucket_type& buffer = (*it).second;
          bucket_type bit_set = (1ul << pos_of(x));
          if(buffer & bit_set){
            buffer ^= bit_set;
            --_count;
            if(!buffer) storage.erase(bucket_of(x));
            return true;
          }
        } // if the bucket in which we want to clear doesn't exist, we don't care
      }
      return false;
    }

    // flip a bit & return whether it is now set
    bool flip(const value_type x) {
      if(x < _capacity){
        bucket_type& buffer = storage[bucket_of(x)];
        bucket_type bit_set = (1ul << pos_of(x));
        
        buffer ^= bit_set;
        const bool bit_now_set = (buffer & bit_set != 0);
        _count = _count + 2 * size_t{bit_now_set} - 1;
        if(!buffer) storage.erase(bucket_of(x));
        return bit_now_set;
      } else return set(x);
    }

    // set all _capacity bits of the set: 0..._capacity-1
    void set_all() {
      _count = _capacity;
      size_t bits = _capacity;
      size_t i = 0;
      while(bits > num_bits_in_bucket){
        storage[i++] = full_bucket;
        bits -= num_bits_in_bucket;
      }
      if(bits)
        storage[i] = full_bucket >> (num_bits_in_bucket - bits);
    }

    void flip_all() {
      _count = _capacity - _count;
      size_t bits = _capacity;
      size_t i = 0;
      while(bits > num_bits_in_bucket){
        bucket_type& buffer = storage[i];
        buffer ^= full_bucket;
        if(!buffer) storage.erase(i);
        bits -= num_bits_in_bucket;
        ++i;
      }
      if(bits != 0){
        storage[i] ^= full_bucket >> (num_bits_in_bucket - bits);
        if(!storage[i]) storage.erase(i);
      }
    }
        

    void print(std::ostream& os) const {
      for(size_t i = capacity(); i != 0;) os << (test(--i) ? '1' : '0');
      return os << " ("<<num_buckets()<<" buckets, "<<capacity()<<" bits, "<<count()<<" set)";
    }

    friend std::ostream& operator<<(std::ostream& os, const iterable_bitset& bs) { bs.print(os); return os; }
  };




  // ------------------ unordered_map-based bitset ----------------------------
  struct unordered_bitset:
    public iterable_bitset<std::unordered_map<size_t, uint64_t>>
  {
    using Parent = iterable_bitset<std::unordered_map<size_t, uint64_t>>;
  protected:
    using Parent::storage;
  public:
    using Parent::bucket_of;
    using Parent::pos_of;
    using Parent::Parent;
    using Parent::clear;

    void clear() { clear_all(); }
    void clear_all() { 
      storage.clear();
      _count = 0;
    }

    void set_capacity(const size_t new_capacity) {
      if(new_capacity < _capacity) {
        for(auto it = storage.begin(); it != storage.end();) {
          auto [index, bucket] = *it;
          auto [target_i, target_pos] = bucket_and_pos_of(new_capacity);
          if((target_i < index) || ((target_i == index) and (target_pos == 0))) {
            _count -= NUM_ONES_INL(bucket);
            it = storage.erase(it);
          } else if(target_i == index) {
            assert(target_pos != 0);
            const auto mask = full_bucket << target_pos;
            _count -= NUM_ONES_INL(bucket & mask);
            bucket &= ~mask;
          }
        }
      }
      _capacity = new_capacity;
    }
    void reserve(const size_t new_capacity) { set_capacity(new_capacity); }
  };


  // ------------------ vector-based bitset ----------------------------
  struct ordered_bitset:
    public iterable_bitset<mstd::raw_vector_map<iter_bitset_default_key, iter_bitset_default_bucket>>
  {
    // ------- static stuff --------
    using Parent = iterable_bitset<mstd::raw_vector_map<iter_bitset_default_key, iter_bitset_default_bucket>>;
    using typename Parent::value_type;
    
    friend class unordered_bitset; 

    // ------- members --------
  protected:
    using Parent::storage;
    using Parent::_count;

    // ------- construction & desctruction ---------
    using Parent::Parent;

    explicit ordered_bitset(const size_t _num_bits, const bool _set_all = 0):
      Parent(_num_bits, 0)
    {
      storage.resize(bucket_of(_num_bits - 1) + 1, _set_all * full_bucket);
      if(_set_all){
        _count = _num_bits;
        // clear the unused bits of the highest bucket
        storage[num_buckets() - 1] ^= (full_bucket << pos_of(_num_bits));
      }
      DEBUG5(std::cout << "made ordered_bitset with "<<size()<<" bits set and "<<capacity()<<" bits capacity\n");
    }

    // ------- operators --------
    ordered_bitset& operator++() {
      const size_t lowest_zero = std::min(num_trailing_ones() + 1, _capacity);
      flip_lowest_k(lowest_zero);
      return *this;
    }
    ordered_bitset operator++(int) { ordered_bitset result = *this; ++(*this); return result; }

    ordered_bitset& operator--() {
      const size_t lowest_one = std::min(num_trailing_zeros() + 1, _capacity);
      flip_lowest_k(lowest_one);
      return *this;
    }
    ordered_bitset operator--(int) { ordered_bitset result = *this; ++(*this); return result; }

    // ------- methods: initialization --------
    // ------- methods: query --------
  public:
    using Parent::bucket_of;
    using Parent::pos_of;
    using Parent::num_buckets;

    value_type min() const {
      value_type result = 0;
      for(size_t i = 0; i < num_buckets(); ++i)
        if(storage[i]){
          result += NUM_TRAILING_ZEROSL(storage[i]);
          return result;
        } else result += num_bits_in_bucket;
      throw std::out_of_range("min() on empty bitset");
    }

    value_type max() const {
      if(_capacity == 0) throw std::out_of_range("max() on empty bitset");
      size_t i = num_buckets();
      while(i ? storage.at(--i) == 0 : false);
      assert(storage.at(i) != 0); // need to assert this as NUM_LEADING_ZEROS() is undefined for 0
      return (i * num_bits_in_bucket) + num_bits_in_bucket - NUM_LEADING_ZEROSL(storage.at(i)) - 1;
    }

    value_type front() const { return min(); }

    //! get the index of the k'th zero
    //! NOTE: counting starts with 0, so you get the index of the least significant zero by passing k = 0
    value_type index_of_kth_zero(value_type k) const {
      size_t i = 0;
      size_t z;
      while(true) {
        if(i != num_buckets()) {
          z = NUM_ZEROS_INL(storage.at(i));
          if(k >= z){
            k -= z;
            ++i;
          } else break;
        } else return (i * num_bits_in_bucket) + k;
      }
      if(k != 0) {
        const auto& buffer = storage.at(i);
        uint_fast32_t width = num_bits_in_bucket / 2;
        uint_fast32_t j = width;
        // using binary seach, find the index j such that the k'th unset bit is at position j
        while(width > 1){
          width /= 2;
          j = (NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) ? j - width : j + width;
        }
        // j might still be off by one in the end
        if(NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) --j;
        return num_bits_in_bucket * i + j;
      } else return num_bits_in_bucket * i + NUM_TRAILING_ONESL(storage.at(i));
    }

    value_type index_of_kth_one(value_type k) const {
      size_t i = 0;
      size_t z;
      while(true){
        if(i != num_buckets()) {
          z = NUM_ONES_INL(storage.at(i));
          if(k >= z){
            k -= z;
            ++i;
          } else break;
        } else throw std::out_of_range("not enough set bits");
      }
      if(k != 0) {
        const auto& buffer = storage.at(i);
        uint_fast32_t width = num_bits_in_bucket / 2;
        uint_fast32_t j = width;
        // using binary seach, find the index j such that the k'th unset bit is at position j
        while(width > 1){
          width /= 2;
          j = (NUM_ONES_IN_LOWEST_K_BITL(j, buffer) > k) ? j - width : j + width;
        }
        // j might still be off by one in the end
        if(NUM_ONES_IN_LOWEST_K_BITL(j, buffer) > k) --j;
        return num_bits_in_bucket * i + j;
      } else return num_bits_in_bucket * i + NUM_TRAILING_ZEROSL(storage.at(i));
    }

    size_t num_trailing_ones() const { return index_of_kth_zero(0); }
    size_t num_trailing_zeros() const { return index_of_kth_one(0); }

    //! count the items whose value is at least x
    size_t count_larger_or_equal(const value_type x) const {
      if(x < _capacity){
        const auto [first_bucket, first_offset] = bucket_and_pos_of(x);
        size_t accu = NUM_ONES_INL(storage[first_bucket] >> first_offset);
        for(size_t i = first_bucket + 1; i < num_buckets(); ++i)
          accu += NUM_ONES_INL(storage[i]);
        return accu;
      } else return 0;
    }

    //! count the items whose value is at most x
    size_t count_smaller(const value_type x) const {
      if(x < _capacity){
        const auto [last_bucket, bits_in_last_bucket] = bucket_and_pos_of(x);
        size_t accu = 0;
        for(size_t i = 0; i < last_bucket; ++i)
          accu += NUM_ONES_INL(storage[i]);
        accu += NUM_ONES_INL(storage[last_bucket] << (num_bits_in_bucket - bits_in_last_bucket));
        return accu;
      } else return count();
    }

    void print(auto& ostr) const {
      const std::vector<bucket_type>& vec = storage;
      for(auto it = vec.rbegin(); it != vec.rend(); ++it) {
        auto x = *it;
        ostr << std::bitset<num_bits_in_bucket>(x) << ' ';
      }
      ostr << "(size "<<size()<<" capacity "<<capacity()<<')';
    }


    // ------- methods: modification --------
  public:
    void clear() { clear_all(); }
    void clear_all() {
      clear_memory(storage.data(), num_buckets() * num_bytes_in_bucket);
      _count = 0;
    }

    //! set the k'th unset bit (k = 0 corresponding to the first unset bit), and return its index
    value_type set_kth_unset(value_type k) {
      value_type result = index_of_kth_zero(k);
      set(result);
      return result;
      
    }

    value_type clear_kth_set(value_type k) {
      value_type result = index_of_kth_one(k);
      Parent::clear(result);
      return result;
    }

    //! flip lowest k bits
    void flip_lowest_k(value_type k) {
      if(k > _capacity) _capacity = k;
      size_t i = 0;
      while(k >= num_bits_in_bucket){
        bucket_type& buffer = storage[i];
        _count = _count + num_bits_in_bucket - 2 * NUM_ONES_INL(buffer);
        buffer = ~buffer;
        k -= num_bits_in_bucket;
        ++i;
      }
      if(k > 0) {
        bucket_type& buffer = storage[i];
        const bucket_type xor_op = ~(full_bucket << k); // xor_op = 2^k-1
        _count += k;
        _count -= 2 * NUM_ONES_INL(buffer & xor_op);
        buffer ^= xor_op;
      }
    }

    void set_capacity(const size_t new_capacity) {
      if(new_capacity > 0) {
        if(new_capacity < _capacity) {
          _count -= count_larger_or_equal(new_capacity);
          auto [c_bucket, c_pos] = bucket_and_pos_of(new_capacity);
          storage.resize(c_bucket + (c_pos != 0));
          if(c_pos != 0)
            storage[c_bucket] &= ~(full_bucket << c_pos);
        } else storage.resize(bucket_of(new_capacity - 1) + 1);
      } else {
        storage.clear();
        _count = 0;
      }
      _capacity = new_capacity;
    }
    void reserve(const size_t new_capacity) { set_capacity(new_capacity); }


    //! flip bits starting from x upwards until k'th zero encountered
    //! return number of flipped bits
    size_t flip_upwards_until_kth_zero(const value_type x, size_t k = 1) {
      DEBUG6(std::cout << "flipping from index "<< static_cast<int>(x)<<" ("<<k<<" more zeros)\n");
      if((x < _capacity) and (k > 0)) {
        const auto [first_bucket, first_offset] = bucket_and_pos_of(x);
        auto& bucket = storage.at(first_bucket);
        const auto first_bucket_shifted = (bucket >> first_offset);
        size_t num_trailing_ones = NUM_TRAILING_ONES(first_bucket_shifted);
        if(num_trailing_ones + first_offset == num_bits_in_bucket) {
          DEBUG5(std::cout << "all remainging bits are set, continueing to next bucket (if there is any)\n");
          // step 1: clear upper 'num_trailing_ones' bits in the bucket
          size_t accu = num_trailing_ones;
          bucket ^= first_bucket_shifted << first_offset;
          // step 2: treat the other buckets
          for(size_t i = first_bucket + 1; i < num_buckets(); ++i) {
            auto& new_bucket = storage.at(i);
            num_trailing_ones = NUM_TRAILING_ONES(new_bucket);
            accu += num_trailing_ones;
            DEBUG5(std::cout << "bucket "<<i<<": "<<std::bitset<num_bits_in_bucket>(new_bucket)<<" ("<<num_trailing_ones<<" trailing ones; now "<<accu<<" bits flipped)\n");
            if(num_trailing_ones != num_bits_in_bucket) {
              const bool not_beyond_capacity = (x + accu < _capacity);
              const size_t first_zero = num_trailing_ones + not_beyond_capacity;
              // flip the lowest bits including the first zero
              new_bucket ^= (1ul << first_zero) - 1;
              _count -= accu - not_beyond_capacity;
              // recurse for k-1 zeros
              return accu + not_beyond_capacity + flip_upwards_until_kth_zero(x + accu + not_beyond_capacity, k - 1);
            } else new_bucket = 0ul;
          }
          _count -= accu;
          return accu;
        } else {
          // flip all ones AND the first encountered zero, unless the zero is beyond capacity
          const bool not_beyond_capacity = (x + num_trailing_ones < _capacity);
          const size_t to_flip = num_trailing_ones + not_beyond_capacity;
          bucket ^= ((1ul << to_flip) - 1) << first_offset;
          _count -= num_trailing_ones - not_beyond_capacity;
          return to_flip + flip_upwards_until_kth_zero(x + to_flip, k - 1);
        }
      } else return 0;
    }
  };
  

  template<class C, class = void> struct is_bitset: public std::false_type {};
  template<class C> struct is_bitset<C, std::void_t<typename C::bucket_map>>: public std::true_type {};
  template<class C> constexpr bool is_bitset_v = is_bitset<std::remove_cvref_t<C>>::value;

  // ------- iterable_bitset: factories ---------
  
  
  // ------- iterable_bitset: deduction guides ---------
  
  // ------- iterable_bitset: defaults ---------


}

namespace std {
  template<typename bucket_map>
  struct hash<mstd::iterable_bitset<bucket_map>>{
    size_t operator()(const mstd::iterable_bitset<bucket_map>& bs) const{
      size_t result = 0;
      for(const auto& item: bs.data()) result ^= item;
      return result;
    }
  };
}



