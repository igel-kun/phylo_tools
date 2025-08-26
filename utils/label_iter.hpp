
#pragma once

#include <memory>
#include "utils.hpp"
#include "stl_utils.hpp"
#include "types.hpp"

namespace PT {

  template<class T>
  concept SingleLabelMap = std::derived_from<T, std::singleton_set<typename T::mapped_type>>;

  // an iterator producing pairs of (Node, Property), where the property is produced by the getter
  // Note: the getter should have a type called 'Property'
  template<std::ContainerType Container, class PropertyGetter>
  struct LabeledNodeIter
  {
    using Iter = std::iterator_of_t<Container>;
    using PropertyType = typename PropertyGetter::PropertyType;
    using value_type      = LabeledNode<PropertyType>;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using difference_type = ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

  protected:
    Iter it;
    PropertyGetter* getter;
 
  public:
    LabeledNodeIter() = delete; // disallow default construction

    LabeledNodeIter(const Iter& _it, PropertyGetter& _getter):
      it(_it), getter(&_getter)
    {}
    
    // dereference
    value_type operator*() const {
      return value_type(*it, getter(*it));
    }

    // increment
    LabeledNodeIter& operator++() { ++it; return *this; }
    LabeledNodeIter operator++(int) { LabeledNodeIter result(it, getter); ++(*this); return result; }

    bool operator==(const LabeledNodeIter& _it) const { return _it.it == it; }
  };

  template<class PropertyGetter>
  struct LabeledNodeIterFor { template<class Container> using type = LabeledNodeIter<Container, PropertyGetter>; };

  template<class NodeContainer, class PropertyGetter>
  using LabeledNodeIterFactory = std::IterFactory<NodeContainer,
                                    const PropertyGetter,
                                    std::BeginEndIters<NodeContainer, false,LabeledNodeIterFor<PropertyGetter>::template type>>;

}// namespace

