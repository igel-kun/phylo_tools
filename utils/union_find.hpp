
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
    const T* representative;    // the representative element of our set
    size_t _size;
  public:
    inline const T& get_representative() const {return *representative;}
    inline size_t size() const { return _size; }
    inline void grow(const int x) { _size += x; }

    DSet(const T* _representative):
      representative(_representative), _size(1)
    {}
    DSet():
      representative(nullptr), _size(0)
    {}

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

    size_t _set_count = 0;

    // redo the representatives with pointer into our own structure after copy(-construct)
    void fix_pointers() {
      for(auto& key_val: *this){
        DSet<T>& dset = key_val.second;
        dset.representative = &(this->find(*(dset.representative))->first);
      }
    }

  public:
    using Parent::at;

    // add a new set to the forest
    template<IterableType _Container = initializer_list<T>>
    DSet<T>& add_new_set(const _Container& x) {
      const T* r = nullptr;
      DSet<T>* result = nullptr;
      for(const auto& i: x){
        const auto [iter, success] = try_emplace(i, r);
        if(success){
          if(r == nullptr) {
            r = &(iter->first);
            result = &(iter->second);
            result->representative = r;
            ++_set_count;
          }
        } else throw logic_error("item already in the set-forest");
      }
      result->_size = x.size();
      return *result;
    }
    DSet<T>& add_new_set(const T& x) {
      const auto [iter, success] = try_emplace(x, nullptr);
      if(success) {
        iter->second.representative = &(iter->first);
        ++_set_count;
      }
      return iter->second;
    }

    // add a new item to the set x_set (that should exist in the set forest)
    DSet<T>& add_item_to_set(const T& x, DSet<T>& y_set) {
      // assert that y exists
      assert(test(*this, *y_set.representative));
      // insert the item
      const auto [iter, success] = try_emplace(x, y_set.representative);
      if(success){
        DSet<T>& x_set = iter->second;
        y_set.grow(1);
        return x_set;
      } else throw logic_error("item already in the set-forest");
    }
    // add a new item x to the set of another item y
    DSet<T>& add_item_to_set_of(const T& x, const T& y) { return add_item_to_set(x, set_of(y)); }

    // merge two sets into one
    // the one with the lower size is merged into the one with the higher
    // in case of ties, x is merged into y's set
    DSet<T>& merge_sets_of(const T& x, const T& y, const bool respect_sizes = true) {
      assert(test(*this, x) && test(*this, y));

      DSet<T>& x_set = set_of(x);
      DSet<T>& y_set = set_of(y);
      if(x_set != y_set){
        --_set_count;
        if(respect_sizes && (x_set.size() < y_set.size())){
          x_set.merge_onto(y_set);
          return y_set;
        } else y_set.merge_onto(x_set);
      }
      return x_set;
    }

    // return the size of the set containing x
    size_t size_of_set_of(const T& x) {  return set_of(x).size(); }
    
    // return the set containing x, use path compression
    DSet<T>& set_of(const T& x, DSet<T>* x_set = nullptr, const unsigned decrease_size = 0) {
      if(!x_set) x_set = &at(x);
      const T& x_set_rep = *(x_set->representative);
      if(x_set_rep != x){
        x_set->grow(-decrease_size);
        DSet<T>* x_parent = &at(x_set_rep);
        const T& x_parent_rep = *(x_parent->representative);
        if(x_parent_rep != x){
          DSet<T>& root_set = set_of(x_set_rep, x_parent, decrease_size + x_set->size());
          x_set->representative = root_set.representative;
          return root_set;
        } else return at(x_parent_rep);
      } else return *x_set;
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
    {
      fix_pointers();
    }
    // for moving, the unordered_map move-constructor should be fine
    DisjointSetForest(DisjointSetForest&& _dsf):
      Parent(_dsf), _set_count(move(_dsf._set_count))
    {}

    DisjointSetForest& operator=(const DisjointSetForest& _dsf) {
      Parent::operator=(_dsf);
      fix_pointers();
      return *this;
    }
    // for moving, the unordered_map move-constructor should be fine
    DisjointSetForest& operator=(DisjointSetForest&& _dsf) {
      Parent::operator=(_dsf);
      fix_pointers();
      return *this;
    }
  };

}


