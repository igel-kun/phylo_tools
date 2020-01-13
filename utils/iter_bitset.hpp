
#pragma once

#include <cstdint>
#include <string.h> // for explicit_bzero
#include <set>
#include <initializer_list>
#include "utils.hpp"
#include "vector_map.hpp"

#define BITSET_FULL_BUCKET (~(bucket_type(0)))
#define BITSET_BUCKET_TYPE bucket_type
#define BITSET_BYTES_IN_BUCKET (sizeof(BITSET_BUCKET_TYPE))
#define BITSET_BITS_IN_BUCKET (CHAR_BIT * BITSET_BYTES_IN_BUCKET)
#define BUCKET_OF(x) (((x) / BITSET_BITS_IN_BUCKET))
#define POS_OF(x) ((x) % BITSET_BITS_IN_BUCKET)
#define TEST_IN_BUCKET(x, y) ((((x) >> POS_OF(y)) & 1) == 1)

namespace std {

  template<class bucket_map = std::vector_map<uint64_t> >
  class bitset_iterator;
  class ordered_bitset;
  class unordered_bitset;

  // ATTENTION: this does not do error checking if NDEBUG is on (except front())
  template<class _bucket_map>
  class iterable_bitset 
  {
  public:
    using bucket_map = _bucket_map;
    using bucket_type = typename bucket_map::mapped_type;

    using bucket_iter = typename bucket_map::iterator;
    using bucket_const_iter = typename bucket_map::const_iterator;

    using value_type = uint64_t;
    // NOTE: bitsets cannot provide meaningful references to their members
    using reference  = uint64_t;
    using const_reference = uint64_t;

    using iterator = bitset_iterator<bucket_map>;
  protected:
    uint64_t num_bits;
    uint64_t _count = 0;
    bucket_map storage;

    inline uint64_t num_buckets() const { return storage.size(); }
    
  public:

    iterable_bitset(const uint64_t _num_bits, const bool _set_all):
      num_bits(_num_bits)
    {
      if(_set_all) set_all();
    }
    iterable_bitset(const uint64_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {}
    
    // construct with some items
    iterable_bitset(std::initializer_list<value_type> init_list, const uint64_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {
      for(const value_type x: init_list) set(x);
    }

    template<class _InitSet>
    iterable_bitset(const typename _InitSet::const_iterator _begin, const typename _InitSet::const_iterator _end, const uint64_t _num_bits = 0):
      iterable_bitset(_num_bits, 0)
    {
      for(typename _InitSet::const_iterator i = _begin; i != _end; ++i) set(*i);
    }

    iterable_bitset(const iterable_bitset<bucket_map>&) = default;
    iterable_bitset(iterable_bitset<bucket_map>&&) = default;
    iterable_bitset& operator=(iterable_bitset<bucket_map>&& bs) = default;

    template<class T>
    iterable_bitset& operator=(const iterable_bitset<T>& bs)
    {
      num_bits = bs.num_bits;
      _count = bs._count;
      storage.clear();
      storage.insert(bs.storage.begin(), bs.storage.end());
    }


    const bucket_map& data() const { return storage; }
    bool insert(const uint64_t x) { return set(x); }
    std::pair<iterator,bool> emplace(const uint64_t x) { const bool res = set(x); return {find(x), res}; }
    bool erase(const uint64_t x) { return clear(x); }
    bool set(const uint64_t x, const bool value) { if(value) return set(x); else return clear(x); }
    void invert() { flip_all(); }
    uint64_t capacity() const { return num_bits; }
    uint64_t size() const { return count(); }
    uint64_t front() const { return *begin(); }

    bool test(const uint64_t x) const
    {
      try{
        return (storage.at(BUCKET_OF(x)) >> POS_OF(x)) & 1;
      } catch(std::out_of_range& e) {
        return false;
      }
    }

    // set a bit & return whether the size changed (that is, if it wasn't set before)
    bool set(const uint64_t x)
    {
      bucket_type bit_set = (1ul << POS_OF(x));
      auto ins_result = storage.insert({BUCKET_OF(x), bit_set});
      // C++, why can't you accept X->Y as alias for (*X).Y ???
      if(!ins_result.second){
        bucket_type& bucket = (*ins_result.first).second;
        if(!(bucket & bit_set)){
          num_bits = std::max(num_bits, x + 1);
          bucket |= bit_set;
          ++_count;
        } else return false;
      } else {
        ++_count;
        num_bits = (BUCKET_OF(x) + 1) * BITSET_BITS_IN_BUCKET;
      }
      return true;
    }

    // clear a bit and return whether the size changed (that is, if it was set before)
    bool clear(const uint64_t x)
    {
      if(x < num_bits){
        try{ // if the at() fails, we know that the bit was already cleared before, so no modification is necessary
          bucket_type& buffer = storage.at(BUCKET_OF(x));
          bucket_type bit_set = (1ul << POS_OF(x));
          if(buffer & bit_set){
            buffer ^= bit_set;
            --_count;
            if(!buffer) storage.erase(BUCKET_OF(x));
            return true;
          }
        } catch(std::out_of_range& e) {} // if the bucket in which we want to clear doesn't exist, we don't care
      }
      return false;
    }

    // flip a bit & return whether it is now set
    bool flip(const uint64_t x)
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

    void set_all()
    {
      _count = num_bits;
      uint64_t bits = num_bits;
      uint64_t i = 0;
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
      uint64_t bits = num_bits;
      uint64_t i = 0;
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
        
    uint64_t count() const { return _count; }
    
    bool empty() const { return _count == 0; }

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
      static_assert(is_stl_set_type<Set>::value);
      if(size() == s.size()) {
        for(const uint32_t x: *this)
          if(!contains(s, x)) return false;
      } else return false;
      return true;
    }

    template<class T = std::vector_map<uint64_t> >
    bool operator==(const iterable_bitset<T>& bs) const
    {
      if(_count != bs._count) return false;
      return storage == bs.storage;
    }
    
    template<typename Set>
    bool operator!=(const Set& s) const
    {
      static_assert(is_stl_set_type<Set>::value);
      return !operator==(s);
    }

    template<class Container>
    iterable_bitset& operator-=(const Container& c)
    {
      static_assert(!std::is_same<Container, iterable_bitset>::value);
      for(const auto& i: c) erase(i);
      return *this;
    }
 
    template<class Container>
    iterable_bitset& operator|=(const Container& c)
    {
      static_assert(!std::is_same<Container, iterable_bitset>::value);
      for(const auto& i: c) insert(i);
      return *this;
    }

    template<class Container>
    iterable_bitset& operator&=(const Container& c)
    {
      static_assert(!std::is_same<Container, iterable_bitset>::value);
      auto _iter = begin();
      const auto _end = end();
      while(_iter != _end){
        const value_type i = *_iter;
        if(!contains(c, i)) clear(i);
        ++_iter;
      }
      return *this;
    }


    friend class bitset_iterator<bucket_map>;
    template<class T>
    friend std::ostream& operator<<(std::ostream& os, const iterable_bitset<T>& bs);

    iterator begin() const;
    iterator end() const;
    iterator find(const uint64_t x) const;
  };

  template<typename bucket_map>
  struct hash<iterable_bitset<bucket_map>>{
    size_t operator()(const iterable_bitset<bucket_map>& bs) const{
      const bucket_map& storage = bs.data();
      size_t result = 0;
      for(const auto& item: storage) has_combine(result, item);
      return result;
    }
  };


  // ------------------ unordered_map-based bitset ----------------------------

  class unordered_bitset: public iterable_bitset<std::unordered_map<uint32_t, uint64_t> >
  {
    using Parent = iterable_bitset<std::unordered_map<uint32_t, uint64_t> >;
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

  class ordered_bitset: public iterable_bitset<std::vector_map<uint64_t> >
  {
    using Parent = iterable_bitset<std::vector_map<uint64_t> >;
    using Parent::storage;
    using Parent::num_buckets;
    using Parent::clear;
    using Parent::_count;
    using Parent::Parent;
  public:

    ordered_bitset(const uint64_t _num_bits, const bool _set_all = 0):
      Parent(_num_bits, 0)
    {
      storage.resize(BUCKET_OF(_num_bits - 1) + 1, _set_all * BITSET_FULL_BUCKET);
      if(_set_all){
        _count = _num_bits;
        // clear the unused bits of the highest bucket
        storage[num_buckets() - 1] ^= (BITSET_FULL_BUCKET << POS_OF(_num_bits));
      }
    }

    ordered_bitset(const ordered_bitset&) = default;
    ordered_bitset(ordered_bitset&&) = default;
    ordered_bitset& operator=(const ordered_bitset& bs) = default;
    ordered_bitset& operator=(ordered_bitset&& bs) = default;

    void clear() { clear_all(); }
    void clear_all() {
      explicit_bzero(storage.data(), num_buckets() * BITSET_BYTES_IN_BUCKET);
      _count = 0;
    }

    uint64_t min() const
    {
      uint64_t result = 0;
      for(uint64_t i = 0; i < num_buckets(); ++i)
        if(storage[i]) 
          return result + NUM_TRAILING_ZEROSL(storage[i]);
        else
          result += BITSET_BITS_IN_BUCKET;
      throw std::out_of_range("min() on empty bitset");
    }

    uint64_t max() const
    {
      if(num_bits == 0) throw std::out_of_range("max() on empty bitset");
      uint64_t i = num_buckets();
      while(i ? storage.at(--i) == 0 : false);
      return (i * BITSET_BITS_IN_BUCKET) + BITSET_BITS_IN_BUCKET - NUM_LEADING_ZEROSL(storage.at(i)) - 1;
    }

    uint64_t front() const { return min(); }

    //! set the k'th unset bit (k = 0 corresponding to the first unset bit), and return its index
    uint64_t set_kth_unset(uint64_t k)
    {
      uint64_t result= index_of_kth_zero(k);
      set(result);
      return result;
      
    }

    uint64_t clear_kth_set(uint64_t k)
    {
      uint64_t result= index_of_kth_one(k);
      clear(result);
      return result;
    }

    //! get the index of the k'th zero
    //! NOTE: counting starts with 0, so you get the index of the "very first" zero by passing k=0
    uint64_t index_of_kth_zero(uint64_t k) const
    {
      uint64_t i = 0;
      uint64_t z;
      while(1){
        if(i == num_buckets()) return (i * BITSET_BITS_IN_BUCKET) + k;
        z = NUM_ZEROS_INL(storage.at(i));
        if(k >= z){
          k -= z;
          ++i;
        } else break;
      }
      const BITSET_BUCKET_TYPE& buffer = storage.at(i);
      uint32_t width = BITSET_BITS_IN_BUCKET / 2;
      uint32_t j = width;
      // using binary seach, find the index j such that the k'th unset bit is at position j
      while(width > 1){
        width /= 2;
        j = (NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) ? j - width : j + width;
      }
      // j might still be off by one in the end
      if(NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) --j;
      return BITSET_BITS_IN_BUCKET * i + j;
    }
    
    uint64_t index_of_kth_one(uint64_t k) const
    {
      uint64_t i = 0;
      uint64_t z;
      while(1){
        if(i == num_buckets()) throw std::out_of_range("not enough set bits");
        z = NUM_ONES_INL(storage.at(i));
        if(k >= z){
          k -= z;
          ++i;
        } else break;
      }
      const BITSET_BUCKET_TYPE& buffer = storage.at(i);
      uint32_t width = BITSET_BITS_IN_BUCKET / 2;
      uint32_t j = width;
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
    void flip_lowest_k(uint64_t k)
    {
      num_bits = std::max(num_bits, k);
      uint64_t i = 0;
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
    uint64_t count_larger(const uint64_t x) const
    {
      if(x <= num_bits){
        const uint64_t first_bucket = BUCKET_OF(x);
        const uint64_t first_offset = x % BITSET_BITS_IN_BUCKET;
        uint64_t accu = NUM_ONES_INL(storage[first_bucket] >> first_offset);
        for(uint64_t i = first_bucket + 1; i < num_buckets(); ++i)
          accu += NUM_ONES_INL(storage[i]);
        return accu;
      } else return 0;
    }
    
    //! count the items whose value is at most x
    uint64_t count_smaller(const uint64_t x) const
    {
      if(x < num_bits){
        const uint64_t last_bucket = BUCKET_OF(x);
        const uint64_t bits_in_last_bucket = x % BITSET_BITS_IN_BUCKET;
        uint64_t accu = 0;
        for(uint64_t i = 0; i < last_bucket; ++i)
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
    for(uint64_t i = bs.capacity(); i != 0;) os << (bs.test(--i) ? '1' : '0');
    return os << " ("<<bs.num_buckets()<<" buckets, "<<bs.capacity()<<" bits, "<<bs.count()<<" set)";
  }

  template<>
  struct is_stl_set_type<ordered_bitset>{
    static const bool value = true;
  };
  template<>
  struct is_stl_set_type<unordered_bitset>{
    static const bool value = true;
  };

  // ========================= iterators =====================================
  


  // NOTE: we do not correspond to the official standard since we do not abide by the following condition:
  // "if a and b compare equal then either they are both non-dereferenceable or *a and *b are references bound to the same object"
  // since our *-operation does not return a reference, but an integer
  // however, iteration a la "for(auto i: my_set)" works very well...
  template<class bucket_map>
  class bitset_iterator: public std::iterator<std::forward_iterator_tag, uint64_t>
  {
  public:
    using bucket_type = typename iterable_bitset<bucket_map>::bucket_type;
    using bucket_iter = typename bucket_map::const_iterator;
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

    bool is_valid() const { return index != storage.cend(); }
    operator bool() const { return is_valid(); }
    uint64_t operator*() const { 
      return (*index).first * BITSET_BITS_IN_BUCKET + NUM_TRAILING_ZEROSL(buffer); 
    }

    bitset_iterator& operator++()
    {
      buffer ^= (1ul << NUM_TRAILING_ZEROSL(buffer));
      if(!buffer) {
        ++index;
        advance_while_empty();
      }
      return *this;
    }

    bitset_iterator operator++(int)
    {
      bitset_iterator result(*this);
      ++(*this);
      return result;
    }

    bool operator==(const bitset_iterator& it) const
    {
      const bool we_at_end = !is_valid();
      const bool they_at_end = !it.is_valid();
      if(we_at_end != they_at_end) return false;
      if(we_at_end && they_at_end) return true;
      return (index == it.index) && (buffer == it.buffer);
    }

    bool operator!=(const bitset_iterator& it) const { return !this->operator==(it); }
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
  bitset_iterator<bucket_map> iterable_bitset<bucket_map>::find(const uint64_t x) const
  {
    if(test(x))
      return bitset_iterator<bucket_map>(storage, storage.find(x / BITSET_BITS_IN_BUCKET), x % BITSET_BITS_IN_BUCKET);
    else
      return end();
  }

}
