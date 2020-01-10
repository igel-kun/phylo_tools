
// I can't believe STL doesn't support disjoint sets...

#include<unordered_map>

namespace std{
  template<class T>
  class DisjointSetForest;

  template<class T>
  struct DSet
  {
  private:
    const T* representative;    // the representative element of our set
    unsigned rank;
  public:
    const T& get_representative() const {return *representative;}

    DSet(const T* _representative):
      representative(_representative), rank(0)
    {}
    DSet():
      representative(nullptr), rank(0)
    {}

    bool operator==(const DSet<T>& y) const { return *representative == *y.representative; }
    bool operator!=(const DSet<T>& y) const { return *representative != *y.representative; }

    friend class DisjointSetForest<T>;
  };

  template<class T>
  std::ostream& operator<<(std::ostream& os, const DSet<T>& ds)
  {
    return os << "->" << ds.get_representative();
  }


  template<class T>
  class DisjointSetForest: public std::unordered_map<T, DSet<T>>
  {
  private:
    size_t _set_count = 0;

  public:
    using Parent = std::unordered_map<T, DSet<T>>;
    using Parent::emplace;
    using Parent::at;

    // add a new set to the forest
    DSet<T>& add_new_set(const std::unordered_set<T>& x)
    {
      const T* r = nullptr;
      DSet<T>* result = nullptr;
      for(const auto& i: x){
        const auto emp_res = emplace(PWC, std::make_tuple(i), std::make_tuple(r));
        if(emp_res.second){
          if(r == nullptr) {
            r = &(emp_res.first->first);
            result = &(emp_res.first->second);
            result->representative = r;
          }
        } else throw std::logic_error("item already in the set-forest");
      }
      return *result;
    }

    // add a new item to the set x_set (that should exist in the set forest)
    DSet<T>& add_item_to_set(const T& x, const DSet<T>& x_set)
    {
      const T* const r = x_set.representative;
      // assert that the set exists
      assert(contains(*this, *r));
      // insert the item
      auto emp_result = emplace(PWC, {x}, {r});
      if(emp_result.second)
        return emp_result.first->second;
      else throw std::logic_error("item already in the set-forest");
    }

    // add a new item x to the set of another item y
    DSet<T>& add_item_to_set_of(const T& x, const T& y)
    {
      // assert that y exists
      assert(contains(*this, y));
      // insert the item
      auto emp_result = emplace(PWC, {x}, {&y});
      if(emp_result.second)
        return emp_result.first->second;
      else throw std::logic_error("item already in the set-forest");
    }

    // merge two sets into one
    // the one with the higher rank is merged into the one with the lower
    // in case of ties, x is merged into y's set
    DSet<T>& merge_sets_of(const T& x, const T& y)
    {
      DSet<T>& x_set = set_of(x);
      DSet<T>& y_set = set_of(y);
      if(x_set != y_set)
        return link(x_set, y_set);
      else return x_set;
    }
    
    // merge the set of x into the set of y, without considering ranks at all
    DSet<T>& merge_sets_no_rank(const T& x, const T& y)
    {
      assert(contains(*this, y));
      auto& result = set_of(x);
      result.representative = &y;
      --_set_count;
      return result;
    }

    // return the set containing x, use path compression
    DSet<T>& set_of(const T& x)
    {
      DSet<T>& sx = at(x);
      if(*sx.representative != x){
        sx.representative = set_of(*sx.representative).representative;
        return at(*sx.representative);
      } else return sx;
    }

    // return true iff the given items are in the same set
    bool in_same_set(const T& x, const T& y)
    {
      return set_of(x).representative == set_of(y).representative;
    }

    // return true iff the given items are in different sets
    bool in_different_sets(const T& x, const T& y)
    {
      return !in_same_set(x, y);
    }

    // return the number of sets in the forest
    size_t set_count() const
    {
      return _set_count;
    }

  private:
    // merge two sets whose representatives are up-to-date (!!!)
    // DO NOT CALL UNLESS YOU KNOW WHAT YOU ARE DOING, USE merge_sets_of() INSTEAD!
    DSet<T>& link(DSet<T>& x_set, DSet<T>& y_set)
    {
      unsigned& x_rank = x_set.rank;
      unsigned& y_rank = y_set.rank;
      
      --_set_count;
      if(x_rank <= y_rank){
        if(x_rank == y_rank) ++y_rank;
        x_set.representative = y_set.representative;
        return y_set;
      } else {
        y_set.representative = x_set.representative;
        return x_set;
      }
    }
  };

}


