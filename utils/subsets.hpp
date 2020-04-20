
// infrastructure for enumerating subsets of a set X such that all subsets of an enumerated set have already been enumerated before

#include "set_interface.hpp"

#pragma once

namespace PT{

  //! note: if you want to modify the original container, set _OutPutContainer = std::reference_wrapper<_Container::value_type> 
  template<class _Container, class _OutPutContainer = _Container>
  class SubsetIterator
  {
  protected:
    _Container& c;
    std::ordered_bitset bits;

  public:
    using value_type = _OutPutContainer;
    using reference  = value_type&;
    using const_reference = const value_type&;

    SubsetIterator(_Container& _c):
      c(_c), bits(_c.size())
    {}
    // construction with an initialization set
/*    template<class _InitSet>
    SubsetIterator(_Container& _c, const _InitSet& init_set):
      c(_c), bits(init_set.begin(), init_set.end(), _c.size())
    {}
    SubsetIterator(_Container& _c, const std::ordered_bitset& init_set):
      c(_c), bits(init_set)
    {}
    SubsetIterator(_Container& _c, const std::unordered_bitset& init_set):
      c(_c), bits(init_set)
    {}
*/
    SubsetIterator(_Container& _c, const uint64_t item):
      c(_c), bits(_c.size())
    {
      bits.set(item);
    }

    //! increment operator
    SubsetIterator& operator++()
    {
      ++bits;
      return *this;
    }

    bool operator==(const SubsetIterator& it) const { return bits == it.bits; }
    bool operator!=(const SubsetIterator& it) const { return !operator==(it); }

    // dereference
    value_type operator*()
    {
      value_type out;
      auto container_iter = c.begin();
      uint64_t last = 0;
      // emplace the items of 
      std::cout << "collecting items with mask "<< bits << " of "<<c<<"\n";
      for(auto b_iter = bits.begin(); b_iter; ++b_iter){
        const uint64_t current = *b_iter;
        std::advance(container_iter, current - last);

        assert(container_iter != c.end());
        append(out, *container_iter);
        last = current;
      }
      return out;
    }

  };


  template<class _Container, class _OutPutContainer = _Container>
  class Subsets
  {
  protected:
    _Container& c;

  public:
    using iterator = SubsetIterator<_Container, _OutPutContainer>;
    using const_iterator = SubsetIterator<const _Container, _OutPutContainer>;

    Subsets(_Container& _c): c(_c) {}

    iterator begin() { return {c}; }
    iterator end() { return {c, c.size()}; }
    const_iterator begin() const { return {c}; }
    const_iterator end() const { return {c, c.size()}; }

  };


}// namespace
