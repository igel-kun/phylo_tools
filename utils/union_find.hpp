
// I can't believe STL doesn't support disjoint sets...

#pragma once

#include "utils.hpp"
#include<unordered_map>

namespace std{
  template<class T>
  class DisjointSetForest;

  template<class T>
  class DSet {
  private:
    T representative;    // the representative element of our set
    size_t _size = 0;
    
    void grow(const int x) { _size += x; }
  public:
    const T& get_representative() const { return representative; }
    size_t size() const { return _size; }

    template<class Q>
    DSet(Q&& _representative):
      representative(std::forward<Q>(_representative)), _size(1)
    {}
    DSet() = default;

    bool operator==(const DSet<T>& y) const { return representative == y.representative; }
    bool operator!=(const DSet<T>& y) const { return representative != y.representative; }

    void merge_onto(DSet<T>& x) {
      representative = x.representative;
      x._size += _size;
    }

    friend class DisjointSetForest<T>;
  };


  template<class T>
  ostream& operator<<(ostream& os, const DSet<T>& ds) { 
    return os << "->" << ds.get_representative() << " ["<<ds.size()<<"]";
  }


  template<class T>
  class DisjointSetForest: public unordered_map<T, DSet<T>> {
    using Parent = unordered_map<T, DSet<T>>;
  protected:
    using Parent::try_emplace;
    using Parent::emplace;
    using Parent::erase;

    size_t _set_count = 0;

  public:
    using Parent::at;

    // add a new set to the forest
    DSet<T>& add_new_set(const T& x) {
      const auto [iter, success] = try_emplace(x, x);
      if(success) {
        ++_set_count;
      } else throw logic_error("trying to add existing item to set-forest");
      return iter->second;
    }
    template<IterableType _Container>
    DSet<T>& add_new_set(const _Container& x) { for(const auto& i: x) add_new_set(i); }


    // add a new item to the set x_set (that should exist in the set forest)
    DSet<T>& add_item_to_set(const T& x, DSet<T>& y_set) {
      // assert that y exists
      assert(test(*this, y_set.get_representative()));
      // insert the item
      const auto [iter, success] = try_emplace(x, y_set.representative);
      if(success){
        y_set.grow(1);
        return iter->second;
      } else throw logic_error("trying to add existing item to set-forest");
    }
    // add a new item x to the set of another item y
    DSet<T>& add_item_to_set_of(const T& x, const T& y) { return add_item_to_set(x, set_of(y)); }

    // merge two sets into one
    // the one with the lower size is merged into the one with the higher
    // in case of ties, x is merged into y's set
    // return the set that the other has been merged into
    template<bool respect_sizes = true>
    DSet<T>& merge_sets_of(const T& x, const T& y) {
      assert(test(*this, x) && test(*this, y));

      DSet<T>& x_set = set_of(x);
      DSet<T>& y_set = set_of(y);
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
    DSet<T>& merge_sets_keep_order(const T& x, const T& y) {
      return merge_sets_of<false>(x, y);
    }

    // return the size of the set containing x
    size_t size_of_set_of(const T& x) {  return set_of(x).size(); }
    
    // return the set containing x, use path compression
    DSet<T>& set_of(const T& x, DSet<T>* x_set = nullptr, const unsigned decrease_size = 0) {
      if(!x_set) x_set = &at(x);
      const T& x_set_rep = x_set->get_representative();
      if(x_set_rep != x){
        x_set->grow(-decrease_size);
        DSet<T>& x_parent = at(x_set_rep);
        const T& x_parent_rep = x_parent.get_representative();
        if(x_parent_rep != x){
          DSet<T>& root_set = set_of(x_set_rep, &x_parent, decrease_size + x_set->size());
          x_set->representative = root_set.representative;
          return root_set;
        } else return at(x_parent_rep);
      } else return *x_set;
    }

    // erase a DSet<T> s from the union-find structure
    // NOTE: this is only possible if s is a leaf-set
    void erase_element(const T& x) {
      DSet<T>& x_set = set_of(x);
      const T& x_rep = x_set.get_representative();
      
      // if x has a representative that is not x, then we need to shrink the representative's set
      assert((x_set.size() == 1) && "trying to erase a non-leaf set from a disjoint-set forest");
      if(x_rep != x) 
        set_of(x_rep).grow(-1);

      Parent::erase(x);
    }

    // replace the representative of a set of elements by re-hanging the representative from the given element
    void make_representative(const T& x) {
      DSet<T>& x_set = at(x);
      const T& x_rep = x_set.get_representative();

      if(x_rep != x) {
        DSet<T>& x_rep_set = at(x_rep);
        x_rep_set.grow(-x_set.size());
        x_set.grow(x_rep_set.size());
        x_rep_set.representative = x_set.representative = x;
      }
    }

    // if one wants to keep a single item of each set, one can use is_root, which returns true if all elements in its set have x as their root
    bool is_root(const T& x) const { return *(at(x).representative) == x; }

    // return true iff the given items are in the same set
    bool in_same_set(const T& x, const T& y) {
      return set_of(x).representative == set_of(y).representative;
    }

    // return true iff the given items are in different sets
    bool in_different_sets(const T& x, const T& y) { return !in_same_set(x, y); }

    // return the number of sets in the forest
    size_t set_count() const { return _set_count; }

    void remove_item(const T& x) { set_of(x).grow(-1); }

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


