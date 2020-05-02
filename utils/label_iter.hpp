
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
  template<class Container, class PropertyGetter>
  class LabeledNodeIter
  {
    using Iter = std::iterator_of_t<Container>;
    Iter it;
    const PropertyGetter& getter;

    LabeledNodeIter() = delete; // disallow default construction
  public:
    using PropertyType = typename PropertyGetter::PropertyType;
    using value_type      = LabeledNode<PropertyType>;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using difference_type = ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    LabeledNodeIter(const Iter& _it, const PropertyGetter& _getter):
      it(_it), getter(_getter)
    {}
    
    // dereference
    value_type operator*() const
    {
      const Node x = *it; // deref<Iter>::do_deref(it);
      return value_type(x, getter(x));
    }

    // increment
    LabeledNodeIter& operator++() { ++it; return *this; }
    LabeledNodeIter operator++(int) { LabeledNodeIter result(it, getter); ++(*this); return result; }

    bool operator==(const LabeledNodeIter& _it) const { return _it.it == it; }
    bool operator!=(const LabeledNodeIter& _it) const { return _it.it != it; }
  };

  template<class PropertyGetter>
  struct LabeledNodeIterFor { template<class Container> using type = LabeledNodeIter<Container, PropertyGetter>; };

  template<class NodeContainer, class PropertyGetter>
  using LabeledNodeIterFactory = std::IterFactory<NodeContainer,
                                                  const PropertyGetter,
                                                  std::BeginEndIters<NodeContainer, false,LabeledNodeIterFor<PropertyGetter>::template type>>;

  /*
  template<class NodeContainer, class PropertyGetter>
  class LabeledNodeIterFactory
  {
    std::shared_ptr<NodeContainer> c;
    PropertyGetter getter;
  public:
    using iterator = LabeledNodeIter<std::iterator_of_t<NodeContainer>, PropertyGetter>;

    // if constructed via a reference, do not destruct the object, if constructed via a pointer (à la "new _AdjContainer()"), do destruct after use
    template<class... Args>
    LabeledNodeIterFactory(NodeContainer& _c, Args&&... args): c(&_c, std::NoDeleter()), getter(std::forward<Args>(args)...) {}
    template<class... Args>
    LabeledNodeIterFactory(std::shared_ptr<NodeContainer> _c, Args&&... args): c(_c), getter(std::forward<Args>(args)...) {}
    template<class... Args>
    LabeledNodeIterFactory(NodeContainer&& _c, Args&&... args):
      c(std::make_shared<NodeContainer>(std::forward<NodeContainer>(_c))), getter(std::forward<Args>(args)...) {}

    iterator begin() const { return {c->begin(), getter}; }
    iterator end() const { return {c->end(), getter}; }
    size_t size() const { return c->size(); }
    size_t empty() const { return c->empty(); }
  };

  template<class NodeContainer, class PropertyGetter>
  LabeledNodeIterFactory<NodeContainer, PropertyGetter> labeled_nodes(NodeContainer* const c, const PropertyGetter& pg, const bool del_on_exit = false)
    { return {c, pg, del_on_exit}; }
  */  
}// namespace

