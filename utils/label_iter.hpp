
#pragma once

#include <memory>
#include "utils.hpp"
#include "stl_utils.hpp"
#include "types.hpp"

namespace PT {

  // For all types except integral types:
  template<typename T>
  struct deref{ static Node do_deref(const T& x) { return *x; } };
  // For integral types only:
  template<>
  struct deref<Node>{ static Node do_deref(const Node x) { return x; } };

  // an iterator producing pairs of (Node, Property), where the property is produced by the getter
  // Note: the getter should have a type called 'Property'
  template<class Iter, class PropertyGetter>
  class LabeledNodeIter
  {
    Iter it;
    const PropertyGetter& getter;

    LabeledNodeIter(); // disallow default construction
  public:
    using Property = typename PropertyGetter::Property;
    using value_type = LabeledNode<Property>;
    using reference = value_type&;
    using pointer = value_type*;
    using difference_type = ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    LabeledNodeIter(const Iter& _it, const PropertyGetter& _getter):
      it(_it), getter(_getter)
    {}
    
    // dereference
    value_type operator*() const
    {
      const Node x = deref<Iter>::do_deref(it);
      return {x, getter(x)};
    }

    // increment
    LabeledNodeIter& operator++() { ++it; return *this; }
    LabeledNodeIter operator++(int) { LabeledNodeIter result(it, getter); ++(*this); return result; }

    bool operator==(const LabeledNodeIter& it) const { return it.it == it; }
    bool operator!=(const LabeledNodeIter& it) const { return it.it != it; }
  };

  template<class NodeContainer, class PropertyGetter>
  class LabeledNodeIterFactory
  {
    std::shared_ptr<NodeContainer> c;
    PropertyGetter getter;
  public:
    using iterator = LabeledNodeIter<std::IteratorOf_t<NodeContainer, false>, PropertyGetter>;

    LabeledNodeIterFactory(const std::shared_ptr<NodeContainer>& _c, const PropertyGetter& _getter): c(_c), getter(_getter) {}
    LabeledNodeIterFactory(NodeContainer* _c, const PropertyGetter& _getter): c(_c), getter(_getter) {}

    iterator begin() const { return {c->begin(), getter}; }
    iterator end() const { return {c->end(), getter}; }
    size_t size() const { return c->size(); }
    size_t empty() const { return c->empty(); }
  };

  template<class NodeContainer, class PropertyGetter>
  LabeledNodeIterFactory<NodeContainer, PropertyGetter> labeled_nodes(const std::shared_ptr<NodeContainer>& c, const PropertyGetter& pg) { return {c, pg}; }
  template<class NodeContainer, class PropertyGetter>
  LabeledNodeIterFactory<NodeContainer, PropertyGetter> labeled_nodes(NodeContainer* c, const PropertyGetter& pg) { return {c, pg}; }

}// namespace

