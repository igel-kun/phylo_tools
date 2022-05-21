
#pragma once

#include <cstdint>
#include <string.h> // for explicit_bzero
#include <set>
#include <initializer_list>
#include "utils.hpp"
#include "stl_utils.hpp"
#include "raw_vector_map.hpp"

#define BITSET_BUCKET_TYPE bucket_type
#define BITSET_FULL_BUCKET (~(BITSET_BUCKET_TYPE(0)))
#define BITSET_BYTES_IN_BUCKET (sizeof(BITSET_BUCKET_TYPE))
#define BITSET_BITS_IN_BUCKET (CHAR_BIT * BITSET_BYTES_IN_BUCKET)
#define BUCKET_OF(x) (((x) / BITSET_BITS_IN_BUCKET))
#define POS_OF(x) ((x) % BITSET_BITS_IN_BUCKET)
#define TEST_IN_BUCKET(x, y) ((((x) >> POS_OF(y)) & 1) == 1)

namespace std {

  template<MapType bucket_map = std::raw_vector_map<size_t, uint64_t> >
  class bitset_iterator;

  class ordered_bitset;
  class unordered_bitset;

  // ATTENTION: this does not do error checking if NDEBUG is on (except front())
  template<MapType _bucket_map = std::raw_vector_map<size_t, uint64_t> >
  class iterable_bitset: public iter_traits_from_reference<uintptr_t> {
    // NOTE: bitsets cannot provide meaningful references to their members
    using traits = iter_traits_from_reference<uintptr_t>;
  public:
    using bucket_map = _bucket_map;
    using bucket_type = typename bucket_map::mapped_type;

    using bucket_iter = typename bucket_map::iterator;
    using bucket_const_iter = typename bucket_map::const_iterator;

    using typename traits::value_type;
    using typename traits::reference;
    using typename traits::const_reference;
    using typename traits::pointer;
    using typename traits::const_pointer;

    using iterator = bitset_iterator<bucket_map>;
    using const_iterator = iterator;
  protected:
    size_t num_bits;
    size_t _count = 0;
    bucket_map storage;

    inline size_t num_buckets() const { return storage.size(); }
    
  public:

    iterable_bitset(const size_t _num_bits, const bool _set_all):
      num_bits(_num_bits), storage()
    {
      if(_set_all) set_all();
    }

    iterable_bitset(const size_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {}
    
    // construct with some items
    template<IterableType C>
    iterable_bitset(C&& init, const size_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {
      for(const auto& x: init) set(x);
    }

    template<IterableType _InitSet>
    iterable_bitset(const typename _InitSet::const_iterator _begin, const typename _InitSet::const_iterator _end, const size_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {
      for(typename _InitSet::const_iterator i = _begin; i != _end; ++i) set(*i);
    }

    iterable_bitset(const iterable_bitset&) = default;
    iterable_bitset(iterable_bitset&&) = default;
    iterable_bitset& operator=(iterable_bitset&& bs) = default;
    iterable_bitset& operator=(const iterable_bitset& bs) = default;
    /*{
      num_bits = bs.num_bits;
      _count = bs._count;
      storage = bs.storage;
    }*/

    const bucket_map& data() const { return storage; }
    std::pair<iterator,bool> emplace(const value_type x) { const bool res = set(x); return {find(x), res}; }
    std::pair<iterator,bool> insert(const value_type x) { return emplace(x); }
    bool erase(const value_type x) { return clear(x); }
    bool unset(const value_type x) { return clear(x); }
    bool set(const value_type x, const bool value) { if(value) return set(x); else return clear(x); }
    void invert() { flip_all(); }
    size_t capacity() const { return num_bits; }
    size_t count() const { return _count; }    
    bool count(const value_type x) const { return test(x); }
    bool contains(const value_type x) const { return test(x); }
    size_t size() const { return count(); }
    bool empty() const { return _count == 0; }
    value_type front() const { return *begin(); }
    bool full() const { return num_bits == _count; }

    bool test(const value_type x) const
    {
      const auto it = storage.find(BUCKET_OF(x));
      if(it != storage.end())
        return TEST_IN_BUCKET(it->second, x);
      else
        return false;
    }

    // set a bit & return whether the size changed (that is, if it wasn't set before)
    bool set(const value_type x)
    {
      bucket_type bit_set = (1ul << POS_OF(x));
      const auto [iter, success] = storage.try_emplace(BUCKET_OF(x), bit_set);
      if(!success){
        bucket_type& bucket = iter->second;
        if(bucket & bit_set) return false; else bucket |= bit_set;
      }
      ++_count;
      num_bits = std::max(num_bits, x + 1);
      return true;
    }

    // clear a bit and return whether the size changed (that is, if it was set before)
    bool clear(const value_type x)
    {
      if(x < num_bits){
        const bucket_iter it = storage.find(BUCKET_OF(x));
        if(it != storage.end()){
          bucket_type& buffer = it->second;
          bucket_type bit_set = (1ul << POS_OF(x));
          if(buffer & bit_set){
            buffer ^= bit_set;
            --_count;
            if(!buffer) storage.erase(BUCKET_OF(x));
            return true;
          }
        } // if the bucket in which we want to clear doesn't exist, we don't care
      }
      return false;
    }

    // flip a bit & return whether it is now set
    bool flip(const value_type x)
    {
      if(x < num_bits){
        bucket_type& buffer = storage[BUCKET_OF(x)];
        bucket_type bit_set = (1ul << POS_OF(x));
        
        buffer ^= bit_set;
        const bool bit_now_set = (buffer & bit_set != 0);
        _count = _count + 2 * bit_now_set - 1;
        if(!buffer) storage.erase(BUCKET_OF(x));
        return bit_now_set;
      } else return set(x);
    }

    // set all num_bits bits of the set: 0...num_bits-1
    void set_all()
    {
      _count = num_bits;
      size_t bits = num_bits;
      size_t i = 0;
      while(bits > BITSET_BITS_IN_BUCKET){
        storage[i++] = BITSET_FULL_BUCKET;
        bits -= BITSET_BITS_IN_BUCKET;
      }
      if(bits)
        storage[i] = BITSET_FULL_BUCKET >> (BITSET_BITS_IN_BUCKET - bits);
    }

    void flip_all()
    {
      _count = num_bits - _count;
      size_t bits = num_bits;
      size_t i = 0;
      while(bits > BITSET_BITS_IN_BUCKET){
        bucket_type& buffer = storage[i];
        buffer ^= BITSET_FULL_BUCKET;
        if(!buffer) storage.erase(i);
        bits -= BITSET_BITS_IN_BUCKET;
        ++i;
      }
      if(bits){
        storage[i] ^= BITSET_FULL_BUCKET >> (BITSET_BITS_IN_BUCKET - bits);
        if(!storage[i]) storage.erase(i);
      }
    }
        
    template<class _Iterator>
    void insert(_Iterator start, const _Iterator& finish)
    {
      while(start != finish) insert(*(start++));
    }

      
    iterable_bitset& operator&=(const iterable_bitset& bs)
    {
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
   
    iterable_bitset& operator^=(const iterable_bitset& bs)
    {
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

    iterable_bitset& operator|=(const iterable_bitset& bs)
    {
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

    iterable_bitset& operator-=(const iterable_bitset& bs)
    {
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


    template<typename Set>
    bool operator==(const Set& s) const
    {
      if(size() == s.size()) {
        for(const value_type x: *this)
          if(!test(s, x)) return false;
      } else return false;
      return true;
    }

    template<class T = std::raw_vector_map<size_t, uint64_t> >
    bool operator==(const iterable_bitset<T>& bs) const
    {
      if(_count != bs._count) return false;
      return storage == bs.storage;
    }
    
    template<class Container>
    iterable_bitset& operator-=(const Container& c)
    {
      for(const auto& i: c) erase(i);
      return *this;
    }
 
    template<class Container>
    iterable_bitset& operator|=(const Container& c)
    {
      for(const auto& i: c) insert(i);
      return *this;
    }

    template<class Container>
    iterable_bitset& operator&=(const Container& c)
    {
      auto _iter = begin();
      const auto _end = end();
      while(_iter != _end){
        const value_type i = *_iter;
        if(!c.count(i)) clear(i);
        ++_iter;
      }
      return *this;
    }


    friend class bitset_iterator<bucket_map>;
    template<class T>
    friend std::ostream& operator<<(std::ostream& os, const iterable_bitset<T>& bs);

    iterator begin() const;
    iterator end() const;
    iterator find(const value_type x) const;
  };

  template<typename bucket_map>
  struct hash<iterable_bitset<bucket_map>>{
    size_t operator()(const iterable_bitset<bucket_map>& bs) const{
      size_t result = 0;
      for(const auto& item: bs.data()) result ^= item;
      return result;
    }
  };


template<class T>
concept mod_assignable_from =
  requires(T& lhs, T rhs) {
    { lhs.operator=(rhs) } -> std::same_as<T&>;
};
template<class T>
concept abc = requires(T x) {
  T::operator=(x);
};


  // ------------------ unordered_map-based bitset ----------------------------

  class unordered_bitset: public iterable_bitset<std::unordered_map<size_t, uint64_t> >
  {
    using Parent = iterable_bitset<std::unordered_map<size_t, uint64_t> >;
    using Parent::storage;
  public:
    using Parent::Parent;
    using Parent::clear;

    void clear() { clear_all(); }
    void clear_all() { 
      storage.clear();
      _count = 0;
    }

    unordered_bitset& operator=(const ordered_bitset& bs);

    friend class ordered_bitset;
  };


  // ------------------ vector-based bitset ----------------------------

  class ordered_bitset: public iterable_bitset<std::raw_vector_map<size_t, uint64_t>>
  {
    using Parent = iterable_bitset<std::raw_vector_map<size_t, uint64_t>>;
    using Parent::storage;
    using Parent::num_buckets;
    using Parent::clear;
    using Parent::_count;
    using Parent::Parent;
  public:
    using typename Parent::value_type;

    ordered_bitset(const size_t _num_bits, const bool _set_all = 0):
      Parent(_num_bits, 0)
    {
      storage.resize(BUCKET_OF(_num_bits - 1) + 1, _set_all * BITSET_FULL_BUCKET);
      if(_set_all){
        _count = _num_bits;
        // clear the unused bits of the highest bucket
        storage[num_buckets() - 1] ^= (BITSET_FULL_BUCKET << POS_OF(_num_bits));
      }
    }

    void clear() { clear_all(); }
    void clear_all() {
      explicit_bzero(storage.data(), num_buckets() * BITSET_BYTES_IN_BUCKET);
      _count = 0;
    }

    value_type min() const
    {
      value_type result = 0;
      for(size_t i = 0; i < num_buckets(); ++i)
        if(storage[i]){
          result += NUM_TRAILING_ZEROSL(storage[i]);
          return result;
        } else result += BITSET_BITS_IN_BUCKET;
      throw std::out_of_range("min() on empty bitset");
    }

    value_type max() const
    {
      if(num_bits == 0) throw std::out_of_range("max() on empty bitset");
      size_t i = num_buckets();
      while(i ? storage.at(--i) == 0 : false);
      assert(storage.at(i) != 0); // need to assert this as NUM_LEADING_ZEROS() is undefined for 0
      return (i * BITSET_BITS_IN_BUCKET) + BITSET_BITS_IN_BUCKET - NUM_LEADING_ZEROSL(storage.at(i)) - 1;
    }

    value_type front() const { return min(); }

    //! set the k'th unset bit (k = 0 corresponding to the first unset bit), and return its index
    value_type set_kth_unset(value_type k)
    {
      value_type result = index_of_kth_zero(k);
      set(result);
      return result;
      
    }

    value_type clear_kth_set(value_type k)
    {
      value_type result = index_of_kth_one(k);
      clear(result);
      return result;
    }

    //! get the index of the k'th zero
    //! NOTE: counting starts with 0, so you get the index of the "very first" zero by passing k=0
    value_type index_of_kth_zero(value_type k) const
    {
      size_t i = 0;
      size_t z;
      while(1){
        if(i == num_buckets()) return (i * BITSET_BITS_IN_BUCKET) + k;
        z = NUM_ZEROS_INL(storage.at(i));
        if(k >= z){
          k -= z;
          ++i;
        } else break;
      }
      const BITSET_BUCKET_TYPE& buffer = storage.at(i);
      uint_fast32_t width = BITSET_BITS_IN_BUCKET / 2;
      uint_fast32_t j = width;
      // using binary seach, find the index j such that the k'th unset bit is at position j
      while(width > 1){
        width /= 2;
        j = (NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) ? j - width : j + width;
      }
      // j might still be off by one in the end
      if(NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) --j;
      return BITSET_BITS_IN_BUCKET * i + j;
    }
    
    value_type index_of_kth_one(value_type k) const
    {
      size_t i = 0;
      size_t z;
      while(1){
        if(i == num_buckets()) throw std::out_of_range("not enough set bits");
        z = NUM_ONES_INL(storage.at(i));
        if(k >= z){
          k -= z;
          ++i;
        } else break;
      }
      const BITSET_BUCKET_TYPE& buffer = storage.at(i);
      uint_fast32_t width = BITSET_BITS_IN_BUCKET / 2;
      uint_fast32_t j = width;
      // using binary seach, find the index j such that the k'th unset bit is at position j
      while(width > 1){
        width /= 2;
        j = (NUM_ONES_IN_LOWEST_K_BITL(j, buffer) > k) ? j - width : j + width;
      }
      // j might still be off by one in the end
      if(NUM_ONES_IN_LOWEST_K_BITL(j, buffer) > k) --j;
      return BITSET_BITS_IN_BUCKET * i + j;
    }

    //! flip lowest k bits
    void flip_lowest_k(value_type k)
    {
      num_bits = std::max(num_bits, k);
      size_t i = 0;
      while(k >= BITSET_BITS_IN_BUCKET){
        bucket_type& buffer = storage[i];
        _count = _count + BITSET_BITS_IN_BUCKET - 2 * NUM_ONES_INL(buffer);
        buffer = ~buffer;
        k -= BITSET_BITS_IN_BUCKET;
        ++i;
      }
      if(k > 0) {
        bucket_type& buffer = storage[i];
        const bucket_type xor_op = ~(BITSET_FULL_BUCKET << k); // xor_op = 2^k-1
        _count = _count + k - 2 * NUM_ONES_INL(buffer & xor_op);
        buffer ^= xor_op;
      }
    }

    //! count the items whose value is at least x
    size_t count_larger(const value_type x) const
    {
      if(x <= num_bits){
        const size_t first_bucket = BUCKET_OF(x);
        const size_t first_offset = x % BITSET_BITS_IN_BUCKET;
        size_t accu = NUM_ONES_INL(storage[first_bucket] >> first_offset);
        for(size_t i = first_bucket + 1; i < num_buckets(); ++i)
          accu += NUM_ONES_INL(storage[i]);
        return accu;
      } else return 0;
    }
    
    //! count the items whose value is at most x
    size_t count_smaller(const value_type x) const
    {
      if(x < num_bits){
        const size_t last_bucket = BUCKET_OF(x);
        const size_t bits_in_last_bucket = x % BITSET_BITS_IN_BUCKET;
        size_t accu = 0;
        for(size_t i = 0; i < last_bucket; ++i)
          accu += NUM_ONES_INL(storage[i]);
        accu += NUM_ONES_INL(storage[last_bucket] << (BITSET_BITS_IN_BUCKET - bits_in_last_bucket));
        return accu;
      } else return count();
    }

    ordered_bitset& operator=(const unordered_bitset& bs)
    {
      clear();
      for(const auto& xy: bs.data())
        storage.emplace(xy.first, xy.second);      
      return *this;
    }

    ordered_bitset& operator++()
    {
      flip_lowest_k(index_of_kth_zero(0) + 1);
      return *this;
    }

    friend class unordered_bitset;
  };

  unordered_bitset& unordered_bitset::operator=(const ordered_bitset& bs)
  {
    clear();
    for(const auto& xy: bs.data())
      storage.emplace(xy.first, xy.second);      
    return *this;
  }


  template<class bucket_map>
  std::ostream& operator<<(std::ostream& os, const iterable_bitset<bucket_map>& bs)
  {
    for(size_t i = bs.capacity(); i != 0;) os << (bs.test(--i) ? '1' : '0');
    return os << " ("<<bs.num_buckets()<<" buckets, "<<bs.capacity()<<" bits, "<<bs.count()<<" set)";
  }


  // ========================= iterators =====================================
  


  // NOTE: we do not correspond to the official standard since we do not abide by the following condition:
  // "if a and b compare equal then either they are both non-dereferenceable or *a and *b are references bound to the same object"
  // since our *-operation does not return a reference, but an integer
  // however, iteration a la "for(auto i: my_set)" works very well...
  template<MapType bucket_map>
  class bitset_iterator: public std::iterator<std::forward_iterator_tag, typename iterable_bitset<bucket_map>::value_type>
  {
    using Parent = std::iterator<std::forward_iterator_tag, typename iterable_bitset<bucket_map>::value_type>;
    using Bitset = iterable_bitset<bucket_map>;
  public:
    using bucket_type = typename Bitset::bucket_type;
    using bucket_iter = typename bucket_map::const_iterator;
    using typename Parent::value_type;
    using pointer     = self_deref<value_type>;
    using reference   = value_type;
    using const_reference = const value_type;
  protected:
    const bucket_map& storage;
    bucket_iter index;
    bucket_type buffer;

    // advance the index while its buffer is empty
    // NOTE: this overwrites the current buffer, so make sure it's zero before
    inline void advance_while_empty()
    {
      while(is_valid()) 
        if((buffer = (*index).second)) break; else ++index;
    }

  public:
    bitset_iterator(const bucket_map& _storage, const bucket_iter& _index):
      storage(_storage), index(_index)
    {
      advance_while_empty();
    }
    
    bitset_iterator(const bucket_map& _storage):
      bitset_iterator(_storage, _storage.begin())
    {}

    // create an iterator at a specific bit inside storage[index]
    bitset_iterator(const bucket_map& _storage, const bucket_iter& _index, const unsigned char sub_index):
      bitset_iterator(_storage, _index)
    {
      // if we didn't advance the buffer, advance the sub-index inside the buffer
      if(index == _index)
        buffer &= (BITSET_FULL_BUCKET << sub_index);
    }

    bool is_valid() const { return index != storage.end(); }
    operator bool() const { return is_valid(); }
    value_type operator*() const { return (*index).first * BITSET_BITS_IN_BUCKET + NUM_TRAILING_ZEROSL(buffer); }

    bitset_iterator& operator++()
    {
      buffer ^= (1ul << NUM_TRAILING_ZEROSL(buffer));
      if(!buffer) {
        ++index;
        advance_while_empty();
      }
      return *this;
    }
    bitset_iterator operator++(int) { bitset_iterator result(*this); ++(*this); return result; }

    bool operator==(const bitset_iterator& it) const
    {
      const bool we_at_end = !is_valid();
      const bool they_at_end = !it.is_valid();
      if(we_at_end != they_at_end) return false;
      if(we_at_end && they_at_end) return true;
      return (index == it.index) && (buffer == it.buffer);
    }
  };




  template<class bucket_map>
  bitset_iterator<bucket_map> iterable_bitset<bucket_map>::begin() const
  {
    return bitset_iterator<bucket_map>(storage);
  }

  template<class bucket_map>
  bitset_iterator<bucket_map> iterable_bitset<bucket_map>::end() const
  {
    return bitset_iterator<bucket_map>(storage, storage.end());
  }

  template<class bucket_map>
  bitset_iterator<bucket_map> iterable_bitset<bucket_map>::find(const value_type x) const
  {
    if(test(x))
      return bitset_iterator<bucket_map>(storage, storage.find(x / BITSET_BITS_IN_BUCKET), x % BITSET_BITS_IN_BUCKET);
    else
      return end();
  }


  template<class C, class = void> struct is_bitset : false_type {};
  template<class C> struct is_bitset<C, void_t<typename C::bucket_map>>: true_type {};
  template<class C> constexpr bool is_bitset_v = is_bitset<std::remove_cvref_t<C>>::value;

}



