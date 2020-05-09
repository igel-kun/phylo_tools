
// this is a simple hashing vector 
// on collision, we just move forward until we find an empty spot
// NOTE: insert and query may be expensive, but we hope that, in practice, they are not :)
// NOTE: erase may be VERY expensive (might move items around the whole vector)
//
// theory 1: a slot at index i is considered empty <=> the value in slot i is i+1
//    this means that, whenever we want to store a value with hash i+1 in slot i, we MUST rehash!
//    If our hash function is good, this happens only if the vector is full.
//    By keeping the load factor down, we can decrease search/insert times at the cost of wasted memory
// theory 2: stored values always have increasing hashes (modulo the vector size) -
//    consider the scenario with size = 4 and we insert 2, then 3, then 6 (collision with 2) and remove the 2 afterwards.
//    If we stored the 6 willy-nilly after the 3, we would vacate the slot for 2 and never find the 6 again...
//    Thus, the storage after insertion will be [1,2,6,3] (note: slot 0 is vacant and 3 is NOT stored at vec[hash(3)])
#pragma once

#include "utils.hpp"
#include "stl_utils.hpp"
#include "filter.hpp"
#include <vector>
#ifdef STATISTICS
#include <unordered_map>
#endif
//#include "utils.hpp"

// shift forward y keys at index x by z indices
#define __VECTOR_HASH_SHIFT_FWD(x,y,z) memmove(static_cast<void*>(data() + (x) + (z)), static_cast<void*>(data() + (x)), (y) * sizeof(Key))    
// shift backward y keys to index x by z indices
#define __VECTOR_HASH_SHIFT_BWD(x,y,z) memmove(static_cast<void*>(data() + (x)), static_cast<void*>(data() + (x) + (z)), (y) * sizeof(Key))    
// return whether the index x with value y is vacant
#define __VECTOR_HASH_IS_VACANT(x, y) ((uintptr_t)(y) == (uintptr_t(x)) + 1u)
// advance the given index
#define __VECTOR_HASH_ADVANCE_IDX(x) {x = (x + 1u) & mask; STAT(++count);}
#define __VECTOR_HASH_REVERT_IDX(x) {x = (x + vector_size() - 1u) & mask; STAT(++count);}
// return the mask for the given number of elements
#define __VECTOR_HASH_MASK(x) (~( (uintptr_t)0u ) >> (sizeof(uintptr_t)*8u - 1u - integer_log( (x) - 1u )) )
// hashing
#define __VECTOR_HASH_DO_HASH(x) ((uintptr_t)(x) & mask)
// default load factor, right below 7/8
#define __VECTOR_HASH_DEFAULT_LOAD_FACTOR 0.8749

namespace std{


  template<class Container>
  struct VacantPredicate{
    const Container& c;
    
    VacantPredicate(const Container& _c): c(_c) {}

    template<class Iter>
    bool value(const Iter& it) { return c.is_vacant(it); }
  };


  template<class Container, class ParentContainer, bool reverse = false>
  using linear_vector_hash_iterator = filtered_iterator<
        Container,
        VacantPredicate<Container>,
        reverse,
        std::conditional_t<reverse, std::reverse_iterator_of<typename Container::Parent>, std::iterator_of_t<typename Container::Parent>>,
        std::BeginEndIters<typename Container::Parent, reverse>>;
  //class linear_vector_hash_iterator;

  template<
    class Key,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<Key>>
  class vector_hash: private vector<Key>
  {
  public:
    using Parent = vector<Key>;
  protected:
    using Parent::Parent;

    using Parent::data;
    using Parent::size;
    using Parent::begin;
    using Parent::end;
    using Parent::resize;

  public:
    using allocator_type  = Allocator;
    using typename Parent::value_type;
    using typename Parent::difference_type;
    using typename Parent::size_type;
    using typename Parent::pointer;
    using typename Parent::const_pointer;
    using typename Parent::reference;
    using typename Parent::const_reference;
  
    using vector_iterator = typename Parent::iterator;
    using const_vector_iterator = typename Parent::const_iterator;
    using reverse_vector_iterator = typename Parent::reverse_iterator;
    using const_reverse_vector_iterator = typename Parent::const_reverse_iterator;

    using iterator = linear_vector_hash_iterator<vector_hash, Parent>;
    using const_iterator = linear_vector_hash_iterator<const vector_hash, const Parent>;
    using reverse_iterator = linear_vector_hash_iterator<vector_hash, Parent, true>;
    using const_reverse_iterator = linear_vector_hash_iterator<const vector_hash, const Parent, true>;
    
    using insert_result = pair<vector_iterator, bool>;
    using const_insert_result = pair<const_vector_iterator, bool>;

#ifdef STATISTICS
    using HistMap = unordered_map<uintptr_t, uintptr_t>;
    mutable HistMap hist;
    mutable uintptr_t count;
#endif
  protected:

    // number of values in the set
    uintptr_t active_values;

    // when this load factor is reached, double the size and trigger a rehash
    float max_load_factor;

    // ANDing this to some x gives x's hash value
    uintptr_t mask;

    // make an iterator poiting to the index
    iterator make_iterator(const uintptr_t index) 
    { return {*this, make_vector_iterator(index), false, *this}; }
    const_iterator make_iterator(const uintptr_t index) const
    { return {*this, make_vector_iterator(index), false, *this}; }
    vector_iterator make_vector_iterator(const uintptr_t index) 
    { return next(Parent::begin(), index); }
    const_vector_iterator make_vector_iterator(const uintptr_t index) const
    { return next(Parent::begin(), index); }

    // compute the hash of an integer in the current vector
    inline uintptr_t do_hash(const Key& x) const noexcept {  return __VECTOR_HASH_DO_HASH(x); } 
    // advance the given index by one (circular)
    inline void advance_index(uintptr_t& index) const noexcept { __VECTOR_HASH_ADVANCE_IDX(index); }
    inline Key empty_key(const uintptr_t index) const { return (Key)(index + 1); }
    inline void set_vacant(const uintptr_t index) { *(data() + index) = empty_key(index); }

  public:
    // define what it means to be vacant
    inline bool is_vacant(const uintptr_t index, const Key& key) const noexcept { return __VECTOR_HASH_IS_VACANT(index, key); }
    inline bool is_vacant(const uintptr_t index) const noexcept { return is_vacant(index, *(data() + index)); }
    inline bool is_vacant(const const_vector_iterator& it) const noexcept { return is_vacant(std::distance(vector_begin(), it)); }
    inline bool is_vacant(const const_reverse_vector_iterator& it) const noexcept { return is_vacant(std::distance(vector_begin(), it.base()) - 1); }

  protected:
    // compute the index where key should be located in the vector
    // return 1 if the index points to a position containing key
    // return 0 if key is not in the set and the index points to a vacant position
    // return 2 if key is not in the set and the index points to the next item with larger hash
    pair<uintptr_t,char> find_slot(const Key& key) const
    {
      assert(!Parent::empty());
      uintptr_t index = do_hash(key);
      const uintptr_t key_hash = index;
      const Key* slot = data() + index;
      uintptr_t slot_hash = do_hash(*slot);
      uintptr_t prev_hash;
      DEBUG5(cout << "finding "<<key<<" (hash "<<key_hash<<") starting from index "<<index<<"\n");

      // first, skip all large hashes that overflow onto us; return 0 if we found a vacant slot
      if(slot_hash > key_hash + 1){
        const uintptr_t start_index = index;
        do{
          DEBUG5(cout << "skipping index "<<index<<" (value: "<<*slot<<" hash: "<<slot_hash<<" (vs bound "<<key_hash+1<<")\n");
          advance_index(index);
          slot = data() + index;
          prev_hash = slot_hash;
          slot_hash = do_hash(*slot);
          if(is_vacant(index, *slot)) return {index, 0};
          if(index == start_index) return {index, 2};
        } while(prev_hash <= slot_hash);
        DEBUG5(cout << "skipped to index "<<index<<" where the slot is "<<*slot<<" (hash "<<slot_hash<<")\n");
        if(slot_hash > key_hash) return {index, 2};
      } else if(is_vacant(index, *slot)) return {index, 0};
      // if we skipped onto key or a vacant slot, return success
      if((slot_hash == key_hash) && (*slot == key)) return {index, 1};
      
      // otherwise, keep searching
      DEBUG5(cout << "keep looking from index "<<index<<"\n");
      prev_hash = slot_hash;
      const uintptr_t start_index = index;
      do{
        advance_index(index);
        slot = data() + index;
        slot_hash = do_hash(*slot);
        DEBUG5(cout << "next index: "<<index<<" (value: "<<*slot<<" hash: "<<slot_hash<<")\n");
        // if we find a free slot, then return failure
        if(is_vacant(index, *slot)) return {index, 0};
        // if we find the key, return it
        if((slot_hash == key_hash) && (*slot == key)) return {index, 1};
      } while((slot_hash <= key_hash) && (prev_hash <= slot_hash) && (index != start_index));
      // if the slot hash grew larger than the key_hash, there is no hope of finding the key
      return {index, 2};
    }

    // erase the key at index from the container
    void _erase(const uintptr_t index)
    {
      assert(!empty());
      uintptr_t next_index = index;
      const Key* next_slot;
      do{
        advance_index(next_index);
        next_slot = data() + next_index;
      } while(!is_vacant(next_index, *next_slot) && !(do_hash(*next_slot) == next_index));
      __VECTOR_HASH_REVERT_IDX(next_index);
      DEBUG5(cout << "shifting up to (including) index "<<next_index<<" (key "<<*next_slot<<")\n");
      // next_index points to the last slot to move
      if(next_index < index){
        // if next_index < index, we wrapped around the end of the vector, so we need 2 move operations
        __VECTOR_HASH_SHIFT_BWD(index, vector_size() - index - 1, 1);
        *(data() + vector_size() - 1) = *(data());
        __VECTOR_HASH_SHIFT_BWD(0, next_index, 1);
      } else __VECTOR_HASH_SHIFT_BWD(index, next_index - index, 1);
      // finally, mark the last index as vacant
      set_vacant(next_index);
      --active_values;
    }

    // insert key and return index and whether an insertion took place
    template<typename KeyRef = const Key&>
    insert_result _insert(KeyRef key)
    {
      DEBUG5(cout << "inserting "<<key<<" into vector-hash of vec-size "<<vector_size()<<" with size = "<<size()<<" & load_factor = "<<load_factor()<<" <= "<<max_load_factor<<endl);
      // find the slot where we would place the key
      const pair<uintptr_t, char> result = find_slot(key);

      switch(result.second){
        case 0: 
          DEBUG5(cout << "found vacant index "<<result.first<<" for "<<key<<"\n");
          // 0 is returned if we reached an empty slot, so insert there
          // unless 'key' is already there, which means that key = index + 1, and we went all the way around to find this vacant slot
          // In this case, trigger a rehash
          if(*(data() + result.first) != key){
            *(data() + result.first) = key;
            ++active_values;
            return {make_vector_iterator(result.first), true};
          } else {
            rehash();
            return _insert<KeyRef>(static_cast<KeyRef>(key));
          }
        case 1:
          DEBUG5(cout << key << " is already in the set (index "<<result.first<<")\n");
          // 1 is returned if the key was found, so return failure
          return {make_vector_iterator(result.first), false};
        default:{
          // otherwise, the hash at the index has grown too large
          // in this case, we'll shift everyone forward by one and insert at result.first
          uintptr_t next_free = result.first;
          do{
            advance_index(next_free);
          } while(!is_vacant(next_free));
          assert((next_free != result.first) && "vector is full, did you tamper with the load factor?");
          DEBUG5(cout << "next free index is "<<next_free<<"\n");
          if(next_free < result.first){
            // if next_free < index, we wrapped around the end of the vector, so we need 2 move operations
            __VECTOR_HASH_SHIFT_FWD(0, next_free, 1);
            *(data()) = *(data() + vector_size() - 1);
            __VECTOR_HASH_SHIFT_FWD(result.first, vector_size() - result.first - 1, 1);
          } else __VECTOR_HASH_SHIFT_FWD(result.first, next_free - result.first, 1);
          // the slot at resukt.first should not be free to receive the key
          *(data() + result.first) = key;
          ++active_values;
          return {make_vector_iterator(result.first), true};
      }}
    }

    // to rehash, double the size of the vector and re-insert everyone
    inline void rehash() { rehash(empty() ? 2 : 2 * vector_size()); }
    inline void rehash(uintptr_t target_size)
    {
      target_size = 1 << (integer_log(target_size - 1) + 1);
      DEBUG5(cout << "\n   REHASH to "<<target_size<<" \n");
      DEBUG5(cout << "before:\n"<<static_cast<vector<Key>>(*this)<<" (size "<<size()<<")\n");
      DEBUG5(cout << "set: "; for(auto it = begin(); it != end(); ++it) cout << *it << " "; cout << "\n");
      assert(target_size >= size());
      
      const size_t old_vec_size = vector_size();
      vector<Key> tmp_vec;
      tmp_vec.reserve(size());
      Parent::resize(target_size);
      mask = __VECTOR_HASH_MASK(target_size);

      for(size_t i = 0; i < old_vec_size; ++i){
        Key& key = *(data() + i);
        if((key != empty_key(i)) && (do_hash(key) != i)){
          tmp_vec.emplace_back(key);
          key = empty_key(i); // set key to empty
        }
      }
      active_values -= tmp_vec.size();

      for(size_t i = old_vec_size; i < vector_size(); ++i)
        set_vacant(i);

      insert(tmp_vec.begin(), tmp_vec.end(), false);
      DEBUG5(cout << "after:\n"<<*this<<" (size "<<size()<<")\n");
      DEBUG5(cout << "set: "; for(auto it = begin(); it != end(); ++it) cout << *it << " "; cout << "\n");
    }

    inline void init_vector()
    {
      for(uintptr_t i = 0; i != vector_size(); ++i)
        set_vacant(i);
    }

  public:

    vector_hash():
      Parent(),
      active_values(0),
      max_load_factor(__VECTOR_HASH_DEFAULT_LOAD_FACTOR),
      mask(0)
    {
      assert(max_load_factor <= 1);
    }
    // create an empty vector_hash with _size empty slots
    vector_hash(const size_t _size, const Allocator& alloc = Allocator()):
      Parent(_size, 0, alloc),
      active_values(0),
      max_load_factor(__VECTOR_HASH_DEFAULT_LOAD_FACTOR),
      mask(__VECTOR_HASH_MASK(_size))
    {
      assert(max_load_factor <= 1);
      // prepare the container such that vector[i] = i+1, that is, all slots are unoccupied
      init_vector();
    }
    template<class InputIt>
    vector_hash(const InputIt& _begin,
                const InputIt& _end,
                const float _max_load_factor = __VECTOR_HASH_DEFAULT_LOAD_FACTOR,
                const Allocator& alloc = Allocator()):
      Parent(0, 0, alloc),
      active_values(0),
      max_load_factor(_max_load_factor),
      mask(0)
    {
      // prepare the container such that vector[i] = i+1, that is, all slots are unoccupied
      insert(_begin, _end);
    }

    inline void set_max_load_factor(const float _max_load_factor)
    {
      assert(_max_load_factor > 0 && _max_load_factor < 1);
      max_load_factor = _max_load_factor;
      if(load_factor() > max_load_factor) rehash();
    }

    inline size_t size() const noexcept { return active_values; }
    inline size_t vector_size() const noexcept { return Parent::size(); }
    inline float load_factor() const noexcept { return vector_size() ? (1.0 * active_values) / vector_size() : 2.0; }

    void swap(vector_hash&& other) noexcept
    {
      active_values = other.active_values;
      max_load_factor = other.max_load_factor;
      mask = other.mask;
      Parent::swap(other);
    }

    bool contains(const Key& key) const { 
      if(empty()) return false;
      STAT(count = 0);
      bool result = find_slot(key).second == 1;
      STAT(++hist[count]);
      STAT(cout << count << "hops\n");
      return result;
    }

    bool count(const Key& key) const { return contains(key); }

    iterator find(const Key& key)
    {
      if(empty()) return end();
      STAT(count = 0);
      const pair<uintptr_t,char> result = find_slot(key);
      STAT(++hist[count]);
      STAT(cout << count << "hops\n");
      return (result.second == 1) ? make_iterator(result.first) : end();
    }

    const_iterator find(const Key& key) const
    {
      if(empty()) return end();
      STAT(count = 0);
      const pair<uintptr_t,char> result = find_slot(key);
      STAT(++hist[count]);
      STAT(cout << count << "hops\n");
      return (result.second == 1) ? make_iterator(result.first) : end();
    }

    insert_result insert(const Key& key)
    {
      // check if load factor is exceeded and trigger rehash
      if(load_factor() > max_load_factor) rehash();
      return _insert(key);
    }

    insert_result insert(Key&& key)
    {
      // check if load factor is exceeded and trigger rehash
      if(load_factor() > max_load_factor) rehash();
      return _insert<Key&&>(static_cast<Key&&>(key));
    }

    template<class InputIt>
    void insert(InputIt _from, const InputIt& _to, const bool do_rehash = true)
    {
      if(do_rehash){
        const size_t num_new_items = distance(_from, _to);
        const size_t prospected_size = (size() + num_new_items) * 1.0 / max_load_factor;
        if(vector_size() < prospected_size) rehash(prospected_size);
      }
      while(_from != _to){
        insert(*_from);
        ++_from;
      }
    }

    template<class T>
    void insert(std::initializer_list<T> _init)
    {
      insert(_init.begin(), _init.end());
    }

    template<class ...Args>
	  insert_result emplace(Args&&... args)
    {
      assert(is_move_assignable_v<Key>);
      if(load_factor() > max_load_factor) rehash();
      return _insert<Key&&>(static_cast<Key&&>(Key(forward<Args>(args)...)));
    }

    inline bool erase(const Key& key) {
      const pair<uintptr_t,char> find_result = find_slot(key);
      if(find_result.second == 1){
        _erase(find_result.first);
        return true;
      } else return false;
    }

    bool erase(const const_iterator& it)
    {
      if(it != vector_end()){
        _erase((&(*it) - data()));
        return true;
      } else return false;
    }

    const Parent& underlying_vector() const { return static_cast<const Parent&>(*this); }
    Parent& underlying_vector() { return static_cast<Parent&>(*this); }

    bool empty() const { return active_values == 0; }
    void clear() { swap(vector_hash()); }
   
    template<class Container>
    bool operator==(const Container& c) const
    {
      if(size() == c.size()){
        for(const auto& i: c)
          if(!contains(*this, i)) return false;
        return true;
      } else return false;
    }
    template<class Container>
    inline bool operator!=(const Container& c) const { return !operator==(c); }
 
    iterator       begin()       { return {*this, vector_begin(), VacantPredicate(*this)}; }
    const_iterator begin() const { return {*this, vector_begin(), VacantPredicate(*this)}; }
    vector_iterator       vector_begin()       { return Parent::begin(); }
    const_vector_iterator vector_begin() const { return Parent::begin(); }

    iterator       end()       { return {*this, vector_end(), VacantPredicate(*this), false}; }
    const_iterator end() const { return {*this, vector_end(), VacantPredicate(*this), false}; }
    vector_iterator       vector_end()       { return Parent::end(); }
    const_vector_iterator vector_end() const { return Parent::end(); }
 
    reverse_iterator       rbegin()       { return {*this, vector_rbegin(), VacantPredicate(*this)}; }
    const_reverse_iterator rbegin() const { return {*this, vector_rbegin(), VacantPredicate(*this)}; }
    reverse_vector_iterator       vector_rbegin()       { return Parent::rbegin(); }
    const_reverse_vector_iterator vector_rbegin() const { return Parent::rbegin(); }
 
    reverse_iterator       rend()       { return {*this, vector_rend(), VacantPredicate(*this), false }; }
    const_reverse_iterator rend() const { return {*this, vector_rend(), VacantPredicate(*this), false }; }
    reverse_vector_iterator       vector_rend()       { return Parent::rend(); }
    const_reverse_vector_iterator vector_rend() const { return Parent::rend(); }
  
    template<class Container, class Predicate, bool reverse, class NormalIterator, class BeginEndIters>
    friend class filtered_iterator;
  };

}// namespace
