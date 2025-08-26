
// a cyclic iterator

#pragma once

namespace mstd {

  // this makes normal iterators cyclic
  template<class Container, class NormalIterator>
  struct  cyclic_iterator {
    using reference = typename NormalIterator::reference;

  protected:
    Container* c;
    NormalIterator i;
    NormalIterator start;
    NormalIterator begin;
    NormalIterator end;
    // the interator has a counter how many times it should cycle before being considered "at the end"
    // to construct an end-iterator, set this to 0
    size_t remaining_loops;
 
    void increment_index() {
      assert(!is_end());
      ++i;
      wrap_index();
    }

    void wrap_index() {
      if((remaining_loops > 0) && (i == end)){
        --remaining_loops;
        i = begin;
      }
    }

    bool is_end() const { return remaining_loops ? false :  i == start; }

  public:
    cyclic_iterator(Container& _c, const NormalIterator& _i, const NormalIterator& _begin, const NormalIterator& _end, size_t num_loops = 1):
      c(&_c), i(_i), start(_i), begin(_begin), end(_end), remaining_loops(num_loops)
    {
      wrap_index();    
    }

    cyclic_iterator& operator++() {
      increment_index();
      return *this;
    }

    cyclic_iterator& operator++(int) {
      cyclic_iterator result(*this);
      ++(*this);
      return result;
    }
    
    operator NormalIterator() { return i; }

    template<class SecondIterator>
    bool operator==(const cyclic_iterator<Container, SecondIterator>& it) const {
      if(!is_end()){
        if(!it.is_end()){
          return i == it.i;
        } else return false;
      } else return it.is_end();
    }

    reference operator*() const { return *i; }
  
    template<class Container_, class SecondIterator>
    friend int64_t distance(const cyclic_iterator<Container_, SecondIterator>& it1, const cyclic_iterator<Container_, SecondIterator>& it2) {
      // the distance of two cyclic vectors is the difference between their underlying iterator
      //    plus the difference in remaining loops times the size of the container
      assert(it1.c == it2.c);
      return distance(it1.i, it2.i) + it1.c->size() * (it1.remaining_loops - it2.remaining_loops);
    }
  };



}// namespace
