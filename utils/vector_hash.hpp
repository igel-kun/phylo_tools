
// this is a simple flat-hashset (based on vector) with a variant of open addressing
// https://en.wikipedia.org/wiki/Open_addressing
// on collision, we just move forward until we find an empty spot
// NOTE: insert and query may be expensive, but we hope that, in practice, they are not :)
// NOTE: erase may be VERY expensive (might move items around the whole vector)
//
// theory: stored values always have increasing hashes (modulo the vector size) -
//    consider the scenario with size = 4 and we insert 2, then 3, then 6 (collision with 2) and remove the 2 afterwards.
//    If we stored the 6 willy-nilly after the 3, we would vacate the slot for 2 and never find the 6 again...
//    Thus, the storage after insertion will be [tombstone,2,6,3] (note: slot 0 is vacant and 3 is NOT stored at vec[hash(3)]),
//    but skipping all entries with smaller hash than hash(3) gives us the position where 3 should be stored
#pragma once

#include <cstring> // for memmove
#include <vector>
#include <bit>
#include "utils.hpp"
#include "stl_utils.hpp"
#include "filter.hpp"
#include "optional.hpp"
#ifdef STATISTICS
#include <unordered_map>
#endif

namespace mstd{

  template<class Iterator, class Element>
  using vector_hash_iterator = converting_iterator<filtered_iterator<Iterator, HasValuePredicate>, Element>;

  template<
    class _Key,
    class Hash = std::conditional_t<mstd::is_really_arithmetic_v<ValFor<_Key>>, mstd::IdentityFunction<void>, std::hash<ValFor<_Key>>>,
    class KeyEqual = std::equal_to<ValFor<_Key>>,
    class Allocator = std::allocator<OptFor<_Key>>>
  class vector_hash: public std::vector<OptFor<_Key>> {
  public:
    using Key = ValFor<_Key>;
    using KeyOpt = OptFor<_Key>;

    using Parent = std::vector<KeyOpt, Allocator>;
  protected:
    using Parent::Parent;

    using Parent::data;
    using Parent::size;
    using Parent::begin;
    using Parent::end;
    using Parent::resize;

  public:
    // forbid implicit conversion
    explicit operator Parent() { return static_cast<Parent&>(*this); }
    explicit operator const Parent() const { return static_cast<const Parent&>(*this); }

    using allocator_type  = Allocator;
    using typename Parent::value_type;
    using typename Parent::difference_type;
    using typename Parent::size_type;
    using typename Parent::pointer;
    using typename Parent::const_pointer;
    using typename Parent::reference;
    using typename Parent::const_reference;
  
    using vector_iterator         = typename Parent::iterator;
    using const_vector_iterator   = typename Parent::const_iterator;
    using reverse_vector_iterator = typename Parent::reverse_iterator;
    using const_reverse_vector_iterator = typename Parent::const_reverse_iterator;

    using iterator          = vector_hash_iterator<vector_iterator, Key&>;
    using const_iterator    = vector_hash_iterator<const_vector_iterator, const Key&>;
    using reverse_iterator  = vector_hash_iterator<reverse_vector_iterator, Key&>;
    using const_reverse_iterator = vector_hash_iterator<const_reverse_vector_iterator, const Key&>;

    using insert_result       = std::pair<iterator, bool>;
    using const_insert_result = std::pair<const_iterator, bool>;
  
    // default load factor, right below 7/8
    static constexpr float default_load_factor = 0.8749f;

#ifdef STATISTICS
    using HistMap = std::unordered_map<uintptr_t, uintptr_t>;
    mutable HistMap hist;
    mutable uintptr_t _count;
#endif
  protected:
    // finding a key may have one of the following results:
    // the returned index points to a position containing key
    // the key is not in the set and the returned index points to a vacant position
    // the key is not in the set and the returned index points to the item that has to be shifted to make room
    enum class FindStatus:char {FS_found_key, FS_vacant, FS_shiftable};

    // number of values in the set
    uintptr_t active_values = 0;

    // when this load factor is reached, double the size and trigger a rehash
    float max_load_factor = default_load_factor;

    // ANDing this to some x gives x's hash value
    uintptr_t mask = 0;
  
    // the provided hasher
    [[ no_unique_address ]] Hash hasher;
    // key comparator
    [[ no_unique_address ]] KeyEqual key_eq;

    // make an iterator poiting to the index
    iterator make_iterator(const uintptr_t index) 
    { return {do_not_fix_index_tag(), std::piecewise_construct, std::forward_as_tuple(make_vector_iterator(index), vector_end())}; }
    const_iterator make_iterator(const uintptr_t index) const
    { return {do_not_fix_index_tag(), std::piecewise_construct, std::forward_as_tuple(make_vector_iterator(index), vector_end())}; }
    vector_iterator make_vector_iterator(const uintptr_t index) 
    { return next(Parent::begin(), index); }
    const_vector_iterator make_vector_iterator(const uintptr_t index) const
    { return next(Parent::begin(), index); }

    // compute the hash of an integer in the current vector
    inline uintptr_t do_hash(const Key& x) const noexcept {  return simple_hash(hasher(x)); }
    inline uintptr_t simple_hash(const size_t& x) const noexcept {  return static_cast<uintptr_t>(x) & mask; } 
    inline uintptr_t fibo_hash(const size_t& x) const noexcept { return ((11400714819323198485llu * x) >> 32) & mask; }

    // advance the given index by one (circular)
    inline void advance_index(uintptr_t& index) const noexcept { index = (index + 1u) & mask; STAT(++_count);}
    inline void revert_index(uintptr_t& index) const noexcept { index = (index + vector_size() - 1u) & mask; STAT(++_count);}
    
    inline void set_vacant(const uintptr_t index) { (data() + index)->reset(); }

    // shift forward 'num_keys' keys at index 'source_index' by 'offset' indices
    inline void shift_forward(uintptr_t source_index, uint64_t num_keys, int64_t offset) {
      const auto source = data() + source_index;
      const auto dest = source + offset;
      //std::memmove(static_cast<void* const>(dest), static_cast<void* const>(source), count);
      //std::cout << "moving "<<num_keys<<" keys ("<<num_keys*sizeof(KeyOpt)<<" bytes) from idx "<<source - data()<<" to idx "<<dest-data()<<" backwards\n";
      std::move_backward(source, source + num_keys, dest + num_keys);
    }
    // shift backward 'num_keys' keys to index 'target_index' by 'offset' indices
    // NOTE: the internal logic of vector_hash only permits shifting backwards blocks of keys with the same hash
    inline void shift_backward(uintptr_t target_index, uint64_t num_keys, int64_t offset) {
      const auto dest = data() + target_index;
      const auto source = dest + offset;
      //std::memmove(static_cast<void* const>(dest), static_cast<void* const>(source), count);
      //std::cout << "moving "<<num_keys<<" keys ("<<num_keys*sizeof(KeyOpt)<<" bytes) from idx "<<source - data()<<" to idx "<<dest-data()<<" forwards\n";
      std::move(source, source + num_keys, dest);
    }
  public:
    // define what it means to be vacant
    inline bool is_vacant(const KeyOpt& k) const noexcept { return !(k.has_value()); }
    inline bool is_vacant(const const_vector_iterator& it) const noexcept { return *it; }
    inline bool is_vacant(const const_reverse_vector_iterator& it) const noexcept { return is_vacant(std::distance(vector_begin(), it.base()) - 1); }

  protected:
    KeyOpt* slot_at(const uintptr_t index) { return data() + index; }
    const KeyOpt* slot_at(const uintptr_t index) const { return data() + index; }
    KeyOpt& key_at(const uintptr_t index) { return *slot_at(index); }
    const KeyOpt& key_at(const uintptr_t index) const { return *slot_at(index); }

    // compute the index where key should be located in the vector
    // return 1 if the index points to a position containing key
    // return 0 if key is not in the set and the index points to a vacant position
    // return 2 if key is not in the set and the index points to the item that has to be shifted to make room
    std::pair<uintptr_t, FindStatus> find_slot(const Key& key) const {
      return find_slot(do_hash(key), [&](const KeyOpt& other){ return key_eq(other, key); });
    }
    // this version takes a slot (that is, a hash) and a key-comparison function that says 'yes' if it is given the requested key
    std::pair<uintptr_t, FindStatus> find_slot(uintptr_t index, auto&& key_cmp) const {
      const uintptr_t key_hash = index;
      const KeyOpt* slot = slot_at(index);
      if(is_vacant(*slot)) return {index, FindStatus::FS_vacant};
      if(key_cmp(*slot)) return {index, FindStatus::FS_found_key};
      
      uintptr_t slot_hash = do_hash(*slot);
      DEBUG5(std::cout << "finding entry with hash "<<key_hash<<" starting from index "<<index<<"\n");

      // first, skip all large hashes that overflow onto us; return 0 if we found a vacant slot
      if(slot_hash > key_hash){
        const uintptr_t start_index = index;
        uintptr_t prev_hash;
        do{
          DEBUG5(std::cout << "skipping index "<<index<<" (value: "<<*slot<<" hash: "<<slot_hash<<" (vs bound "<<key_hash+1<<")\n");
          advance_index(index);
          if(index == start_index) return {index, FindStatus::FS_shiftable};
          slot = slot_at(index);
          if(is_vacant(*slot)) return {index, FindStatus::FS_vacant};
          prev_hash = slot_hash;
          slot_hash = do_hash(*slot);
        } while(prev_hash <= slot_hash);
        DEBUG5(std::cout << "skipped to index "<<index<<" where the slot is "<<*slot<<" (hash "<<slot_hash<<")\n");
        if(slot_hash > key_hash) return {index, FindStatus::FS_shiftable};
        // if we skipped onto key, return success
        if(key_cmp(*slot)) return {index, FindStatus::FS_found_key};
      }
      // at this point, slot_hash <= key_hash is guaranteed
      assert(slot_hash <= key_hash);
      
      // if we haven't found key or a vacant spot, we'll keep searching
      DEBUG5(std::cout << "keep looking from index "<<index<<"\n");
      const uintptr_t prev_hash = slot_hash;
      const uintptr_t start_index = index;
      do{
        advance_index(index);
        slot = slot_at(index);
        if(is_vacant(*slot)) return {index, FindStatus::FS_vacant};
        slot_hash = do_hash(*slot);
        DEBUG5(std::cout << "next index: "<<index<<" (value: "<<*slot<<" hash: "<<slot_hash<<")\n");
        if(key_cmp(*slot)) return {index, FindStatus::FS_found_key};
      } while((slot_hash <= key_hash) && (prev_hash <= slot_hash) && (index != start_index));
      // if the slot hash grew larger than the key_hash, there is no hope of finding the key
      return {index, FindStatus::FS_shiftable};
    }

    // erase the key at index from the container
    void _erase(const uintptr_t start_index) {
      assert(!is_vacant(start_index));
      // step 1:
      uintptr_t end_index = start_index;
      const KeyOpt* next_slot;
      do{
        advance_index(end_index);
        next_slot = slot_at(end_index);
      } while((do_hash(*next_slot) != end_index) && !is_vacant(*next_slot));
      revert_index(end_index);
      DEBUG5(std::cout << "shifting up to (including) index "<<end_index<<" (key "<<*next_slot<<")\n");
      // end_index points to the last slot to move
      if(end_index < start_index){
        // if end_index < start_index, then we wrapped around the end of the vector, so we need 2 shifts (and a single-element move)
        shift_backward(start_index, vector_size() - start_index - 1, 1);
        key_at(vector_size() - 1) = std::move(key_at(0));
        shift_backward(0, end_index, 1);
      } else shift_backward(start_index, end_index - start_index, 1);
      // finally, mark the last index as vacant
      set_vacant(end_index);
      --active_values;
    }

    // insert key and return index and whether an insertion took place
    template<typename KeyRef>
    insert_result _insert(KeyRef&& key) {
      DEBUG5(std::cout << "===> inserting "<<key<<" into vector-hash of vec-size "<<vector_size()<<" with size = "<<size()<<" & load_factor = "<<load_factor()<<" <= "<<max_load_factor<<'\n');
      DEBUG5(std::cout << "===> current layout: "<< static_cast<const Parent&>(*this)<<'\n');
      // find the slot where we would place the key
      const auto [index, status] = find_slot(key);
      DEBUG5(std::cout << "===> got index "<<index<<" from find_slot\n");


      switch(status){
        case FindStatus::FS_vacant: 
          DEBUG5(std::cout << "found vacant index "<<index<<" for "<<key<<"\n");
          // 0 is returned if we reached an empty slot, so insert there
          // unless 'key' is already there, which means that we went all the way around to find this vacant slot
          // In this case, trigger a rehash
          if(key_at(index) != key){
            key_at(index) = std::forward<KeyRef>(key);
            ++active_values;
            return {make_vector_iterator(index), true};
          } else {
            rehash();
            return _insert(std::forward<KeyRef>(key));
          }
        case FindStatus::FS_found_key:
          DEBUG5(std::cout << key << " is already in the set (index "<<index<<")\n");
          // 1 is returned if the key was found, so return failure
          return {make_vector_iterator(index), false};
        case FindStatus::FS_shiftable: {
          // otherwise, the hash at the index has grown too large
          // in this case, we'll shift everyone forward by one and insert at index
          uintptr_t next_free = index;
          do{
            advance_index(next_free);
          } while(!is_vacant(key_at(next_free)));
          DEBUG5(std::cout << "next free index is "<<next_free<<"\n");
          if(next_free < index){
            // if next_free < index, we wrapped around the end of the vector, so we need 2 move operations
            shift_forward(0, next_free, 1);
            key_at(0) = key_at(vector_size() - 1);
            shift_forward(index, vector_size() - index - 1, 1);
          } else shift_forward(index, next_free - index, 1);
          DEBUG5(std::cout << "===> layout after shift: "<<static_cast<const Parent&>(*this)<<'\n');
          // the slot at resukt.first should not be free to receive the key
          key_at(index) = std::forward<KeyRef>(key);
          ++active_values;
          DEBUG5(std::cout << "===> resulting layout: "<<static_cast<const Parent&>(*this)<<'\n');
          return {make_vector_iterator(index), true};
        }
        default: throw std::logic_error("unexpected find-status out of eval");
      }
    }

    // to rehash, double the size of the vector and re-insert everyone
    void rehash() { rehash(empty() ? 2 : 2 * vector_size()); }
    void rehash(size_t target_size) {
      target_size = std::bit_ceil(target_size);
      DEBUG5(std::cout << "\n   REHASH to "<<target_size<<" \n");
      DEBUG5(std::cout << "before:\n"<<static_cast<std::vector<KeyOpt>>(*this)<<" (size "<<size()<<")\n");
      DEBUG5(std::cout << "set: "; for(auto it = begin(); it != end(); ++it) std::cout << *it << " "; std::cout << "\n");
      assert(target_size >= size());
     
      if(empty()){
        Parent::resize(target_size);
        mask = target_size-1;
      } else {
        vector_hash tmp_vec(target_size, hasher, Parent::get_allocator());
        
        DEBUG5(std::cout << "new vector of size "<<size()<<'\n'; );

        for(size_t i = 0; i < vector_size(); ++i) {
          KeyOpt& key = key_at(i);
          if(key.has_value())
            tmp_vec._insert(std::move(key));
        }
        *this = std::move(tmp_vec);
        DEBUG5(std::cout << "after:\n"<<*this<<" (size "<<size()<<")\n");
        DEBUG5(std::cout << "set: " << *this << '\n');
        DEBUG5(std::cout << "vec: " << static_cast<const Parent&>(*this) << '\n');
      }
    }

  public:

    vector_hash() = default;

    // create an empty vector_hash with _size empty slots
    vector_hash(const size_t _size,
                const Hash& _hasher = Hash(),
                const Allocator& alloc = Allocator()):
      Parent(std::bit_ceil(_size), KeyOpt(), alloc),
      mask{std::bit_ceil(_size)-1},
      hasher{_hasher}
    {
      assert(max_load_factor < 1);
    }

    template<HasIterTraits InputIt1, HasIterTraits InputIt2>
    vector_hash(const InputIt1& _begin,
                const InputIt2& _end,
                const size_t _num_new_elements = 0,
                const float _max_load_factor = default_load_factor,
                const Hash& _hasher = Hash(),
                const Allocator& alloc = Allocator()):
      Parent(0, KeyOpt{}, alloc),
      max_load_factor{_max_load_factor},
      hasher{_hasher}
    {
      // prepare the container such that vector[i] = i+1, that is, all slots are unoccupied
      insert(_begin, _end, _num_new_elements);
    }

    inline void set_max_load_factor(const float _max_load_factor)
    {
      assert(_max_load_factor > 0 && _max_load_factor < 1);
      max_load_factor = _max_load_factor;
      if(load_factor() > max_load_factor) rehash();
    }

    inline size_t size() const noexcept { return active_values; }
    inline size_t vector_size() const noexcept { return Parent::size(); }
    inline float load_factor(const size_t additional_values = 0) const noexcept {
      return vector_size() ? (static_cast<double>(active_values + additional_values) / vector_size()) : 2.0;
    }

    void swap(vector_hash&& other) noexcept
    {
      active_values = other.active_values;
      max_load_factor = other.max_load_factor;
      mask = other.mask;
      Parent::swap(other);
    }

    bool contains(const Key& key) const { 
      if(empty()) return false;
      STAT(_count = 0);
      const bool result = find_slot(key).second == FindStatus::FS_found_key;
      STAT(++hist[_count]);
      return result;
    }

    bool count(const Key& key) const { return contains(key); }

    iterator find(const Key& key) {
      if(empty()) return end();
      STAT(_count = 0);
      const auto [iter, status] = find_slot(key);
      STAT(++hist[_count]);
      return (status == FindStatus::FS_found_key) ? make_iterator(iter) : end();
    }

    const_iterator find(const Key& key) const {
      if(empty()) return end();
      STAT(_count = 0);
      const auto [iter, status] = find_slot(key);
      STAT(++hist[_count]);
      return (status == FindStatus::FS_found_key) ? make_iterator(iter) : end();
    }

    template<class T> requires (std::is_same_v<std::remove_cvref_t<T>, Key>)
    insert_result insert(T&& key) {
      // check if load factor is exceeded and trigger rehash
      if(load_factor() > max_load_factor) rehash();
      return _insert(std::forward<T>(key));
    }

    template<HasIterTraits InputIt1, HasIterTraits InputIt2, size_t num_new_items = 0>
    void insert(InputIt1 _from, const InputIt2& _to) {
      using Cat1 = typename iterator_traits<InputIt1>::iterator_category;
      using Cat2 = typename iterator_traits<InputIt2>::iterator_category;
      if constexpr ((std::is_same_v<Cat1, std::random_access_iterator_tag>) && (std::is_same_v<Cat2, std::random_access_iterator_tag>)) {
        num_new_items = distance(_from, _to);
      }
      if(num_new_items != 0) {
        const size_t projected_size = static_cast<double>(size() + num_new_items) / static_cast<double>(max_load_factor);
        if(vector_size() < projected_size)
          rehash(projected_size);
      }
      while(_from != _to) {
        insert(*_from);
        ++_from;
      }
    }

    template<class T>
    void insert(std::initializer_list<T> _init) {
      insert(_init.begin(), _init.end());
    }

    template<class ...Args>
	  insert_result emplace(Args&&... args) {
      assert(std::is_move_constructible_v<Key>);
      if(load_factor() > max_load_factor) rehash();
      return _insert(Key(std::forward<Args>(args)...));
    }

    bool erase(const Key& key) {
      const auto [index, status] = find_slot(key);
      if(status == FindStatus::FS_found_key){
        _erase(index);
        return true;
      } else return false;
    }

    bool erase(const const_iterator& it) {
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
 
    iterator       begin()       { return typename iterator::Iterator{std::piecewise_construct, std::forward_as_tuple(vector_begin(), vector_end())}; }
    const_iterator begin() const { return typename const_iterator::Iterator{std::piecewise_construct, std::forward_as_tuple(vector_begin(), vector_end())}; }
    vector_iterator       vector_begin()       { return Parent::begin(); }
    const_vector_iterator vector_begin() const { return Parent::begin(); }

    iterator       end()       { return typename iterator::Iterator{do_not_fix_index_tag(), std::piecewise_construct, std::forward_as_tuple()}; }
    const_iterator end() const { return typename const_iterator::Iterator{do_not_fix_index_tag(), std::piecewise_construct, std::forward_as_tuple()}; }
    vector_iterator       vector_end()       { return Parent::end(); }
    const_vector_iterator vector_end() const { return Parent::end(); }
 
    reverse_iterator       rbegin()       { return reverse_iterator{std::piecewise_construct, std::forward_as_tuple(vector_rbegin(), vector_rend())}; }
    const_reverse_iterator rbegin() const { return const_reverse_iterator{std::piecewise_construct, std::forward_as_tuple(vector_rbegin(), vector_rend())}; }
    reverse_vector_iterator       vector_rbegin()       { return Parent::rbegin(); }
    const_reverse_vector_iterator vector_rbegin() const { return Parent::rbegin(); }
 
    reverse_iterator       rend()       { return reverse_iterator{do_not_fix_index_tag(), std::piecewise_construct, std::forward_as_tuple()}; }
    const_reverse_iterator rend() const { return const_reverse_iterator{do_not_fix_index_tag(), std::piecewise_construct, std::forward_as_tuple()}; }
    reverse_vector_iterator       vector_rend()       { return Parent::rend(); }
    const_reverse_vector_iterator vector_rend() const { return Parent::rend(); }
  
//    template<class Container, class Predicate, bool reverse, class NormalIterator, class BeginEndIters>
//    friend class filtered_iterator;
  };

  template<class _Key, class Hash, class KeyEqual, class Allocator>
  constexpr bool is_vector_v<vector_hash<_Key, Hash, KeyEqual, Allocator>> = false;

  static_assert(SetType<vector_hash<int>>);
}// namespace
