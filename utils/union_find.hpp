
// I can't believe STL doesn't support disjoint sets...

#pragma once

#include "utils.hpp"
#include<unordered_map>

namespace std{
  template<class Key>
  class _DSet {
    Key representative;    // the representative element of our set
    size_t _size = 0;
    
    void grow(const int x) { _size += x; }
  public:
    const Key& get_representative() const { return representative; }
    size_t size() const { return _size; }

    template<class Q>
    _DSet(Q&& _representative):
      representative(std::forward<Q>(_representative)), _size(1)
    {}
    _DSet() = default;

    bool operator==(const _DSet& y) const { return representative == y.representative; }

    void merge_onto(_DSet& x) {
      representative = x.representative;
      x._size += _size;
    }

    template<class, class>
    friend class DisjointSetForest;
  };

  template<class Key, class Payload = void>
  struct DSet: public _DSet<Key> {
    using Parent = _DSet<Key>;
    using Parent::Parent;
    Payload payload;

    template<class Q, class... Args>
    DSet(Q&& _representative, Args&&... args):
      Parent(std::forward<Q>(_representative)),
      payload(std::forward<Args>(args)...)
    {}
    DSet() = default;
  };
  template<class Key>
  struct DSet<Key, void>: public _DSet<Key> {
    using _DSet<Key>::_DSet;
  };


  template<class K, class P>
  ostream& operator<<(ostream& os, const DSet<K,P>& ds) { 
    os << "->" << ds.get_representative() << " [size "<<ds.size();
    if constexpr (std::is_void_v<P>) 
      return os<<"]";
    else
      return os << " payload "<<ds.payload<<"]";
  }


  // a union-find datastructure on keys, allowing an additional payload to be stored for each key
  template<class Key, class Payload = void>
  class DisjointSetForest: public unordered_map<Key, DSet<Key, Payload>> {
    using Parent = unordered_map<Key, DSet<Key, Payload>>;
  protected:
    using Set = DSet<Key, Payload>;
    using Parent::try_emplace;
    using Parent::emplace;
    using Parent::erase;
    using Parent::at;
    using Parent::operator[];
    using Parent::find;

    size_t _set_count = 0;


    static const Set& set_of(const Set& x_set) { return x_set; }
    static Set& set_of(Set& x_set) { return x_set; }
    static Key& representative(Set& x_set) { return x_set.representative; }
    void shrink(const Key& x) { set_of(x).grow(-1); }

    // return the set containing x, use path compression
    Set& set_of(const Key& x, Set& x_set, const unsigned decrease_size = 0) {
      const Key& x_set_rep = x_set.get_representative();
      if(x_set_rep != x){
        x_set.grow(-decrease_size);
        Set& x_parent = at(x_set_rep);
        const Key& x_parent_rep = x_parent.get_representative();
        assert(x_parent_rep != x); // this is bizarre, we already know that x's representative is x_parent so why would their representative be x???
        if(x_parent_rep != x){
          Set& root_set = set_of(x_set_rep, x_parent, decrease_size + x_set.size());
          x_set.representative = root_set.representative;
          return root_set;
        } else return at(x_parent_rep);
      } else return x_set;
    }

  public:

    template<class... Args>
    auto emplace_set(const Key& x, Args&&... args) {
      const auto result = try_emplace(x, x, std::forward<Args>(args)...);
      _set_count += result.second;
      return result;
    }

    // add a new set to the forest
    template<class... Args>
    const Set& add_new_set(const Key& x, Args&&... args) {
      const auto [iter, success] = emplace_set(x, std::forward<Args>(args)...);
      if(!success) throw logic_error("trying to add existing item to set-forest");
      return iter->second;
    }

    template<class... Args>
    auto emplace_item_to_set(Set& y_set, const Key& x, Args&&... args) {
      const auto result = try_emplace(x, y_set.representative, std::forward<Args>(args)...);
      y_set.grow(result.second);
      return result;
    }

    // add a new item to the set x_set (that should exist in the set forest)
    template<class... Args>
    Set& add_item_to_set(Set& y_set, const Key& x, Args&&... args) {
      // assert that y exists
      assert(test(*this, y_set.get_representative()));
      // insert the item
      const auto [iter, success] = try_emplace(x, y_set.representative, std::forward<Args>(args)...);
      if(success){
        y_set.grow(1);
        return iter->second;
      } else throw logic_error("trying to add existing item to set-forest");
    }
    // add a new item x to the set of another item y
    Set& add_item_to_set_of(const Key& y, const Key& x) { return add_item_to_set(set_of(y), x); }

    // merge two sets into one
    // the one with the lower size is merged into the one with the higher
    // in case of ties, x is merged into y's set
    // return the set that the other has been merged into
    template<bool respect_sizes = true>
    Set& merge_sets(auto& x, auto& y) {
      Set& x_set = set_of(x);
      Set& y_set = set_of(y);
      if(x_set != y_set){
        --_set_count;
        if constexpr (respect_sizes) {
          if(x_set.size() < y_set.size()){
            x_set.merge_onto(y_set);
            return y_set;
          }
        }
        y_set.merge_onto(x_set);
      }
      return x_set;
    }
    // merge y onto x (y's representative will be lost (set to x's representative))
    Set& merge_sets_keep_order(auto& x, auto& y) {
      return merge_sets<false>(x, y);
    }

    Set& set_of(const Key& x) { return set_of(x, at(x)); }

    pair<Set*,Set*> lookup(const Key& x) {
      const auto iter = this->find(x);
      if(iter != this->end()) {
        return {&(iter->second), &set_of(x, iter->second)};
      } else return {nullptr, nullptr};
    }


    static const Key& representative(const Set& x_set) { return x_set.representative; }
    static const Key& representative(const Key& x) { return x; }


    // erase a Set s from the union-find structure
    // NOTE: this is only possible if s is a leaf-set
    void erase_element(auto& x) {
      Set& x_set = set_of(x);
      const Key& x_rep = representative(x_set);
      
      // if x has a representative that is not x, then we need to shrink the representative's set
      assert((x_set.size() == 1) && "trying to erase a non-leaf set from a disjoint-set forest");
      if(x_rep != x) 
        set_of(x_rep).grow(-1);

      Parent::erase(x);
    }

    // split an element off its representative
    void split_element(const Key& x) {
      const auto iter = Parent::find(x);
      assert(iter != Parent::end());
      auto& x_set = iter->second;
      const Key& x_rep = representative(x_set);
     
      if(x_rep != x) {
        set_of(x_rep).grow(-x_set.size());
        x_set.representative = x;
      }
    }

    // replace the representative of a set of elements by re-hanging the representative from the given element
    void make_representative(const Key& x) {
      Set& x_set = at(x);
      const Key& x_rep = x_set.get_representative();

      if(x_rep != x) {
        Set& x_rep_set = at(x_rep);
        x_rep_set.grow(-x_set.size());
        x_set.grow(x_rep_set.size());
        x_rep_set.representative = x_set.representative = x;
      }
    }

    // if one wants to keep a single item of each set, one can use is_root, which returns true if all elements in its set have x as their root
    bool is_root(const Key& x) const { return *(at(x).representative) == x; }

    // return true iff the given items are in the same set
    bool in_same_set(const Key& x, const Key& y) { return set_of(x) == set_of(y); }

    // return true iff the given items are in different sets
    bool in_different_sets(const Key& x, const Key& y) { return !in_same_set(x, y); }

    // return the number of sets in the forest
    size_t set_count() const { return _set_count; }

    // we'll need a custom copy and move constructor :/

    DisjointSetForest(): Parent() {}
    DisjointSetForest(const DisjointSetForest& _dsf):
      Parent(_dsf)
    {}
    // for moving, the unordered_map move-constructor should be fine
    DisjointSetForest(DisjointSetForest&& _dsf):
      Parent(_dsf), _set_count(move(_dsf._set_count))
    {}

    DisjointSetForest& operator=(const DisjointSetForest& _dsf) {
      Parent::operator=(_dsf);
      return *this;
    }
    // for moving, the unordered_map move-constructor should be fine
    DisjointSetForest& operator=(DisjointSetForest&& _dsf) {
      Parent::operator=(_dsf);
      return *this;
    }
  };

}


