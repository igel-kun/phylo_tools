
#pragma once

#include "types.hpp"
#include "iter_factory.hpp"

namespace PT {

  // this is an iterator for switchings of a network
  template<StrictPhylogenyType Net, mstd::VectorType OutputVec = NetEdgeVec<Net>>
  class SwitchingIter: public mstd::iter_traits_from_reference<OutputVec> {
    using Traits = mstd::iter_traits_from_reference<OutputVec>;
    using ParentIter = mstd::iterator_of_t<typename Net::ParentContainer>;
    using ParentAutoIter = mstd::auto_iter<ParentIter>;
    using EdgeVec = NetEdgeVec<Net>;
    using AdjVec = NetAdjVec<Net>;
    using Roots = typename Net::RootContainer;

    bool valid = true;
    Roots roots;
  
    // we're going to store for each reticulation an iterator to its currently active parent-adjacency
    NodeMap<ParentAutoIter> active_parent;

    // fill active_edges with the switched-on edges below root and return whether we found a leaf
    // NOTE: the active_edges are guaranteed in pre-order of the switching (the tree containing all switched-on edges)
    template<mstd::VectorType EdgeVec>
    bool add_active_edges_below(const NodeDesc root, EdgeVec& active_edges) const {
      // for each given leaf, add the path from the root in the switching
      if(!Net::is_leaf(root)) {
        assert(is_valid());
        bool result = false;
        for(const auto& v_adj: Net::children(root)) {
          const NodeDesc v = v_adj;
          if(!Net::is_reti(v) || (*active_parent.at(v) == root)) {
            const size_t old_size = active_edges.size(); // record the current size so we can restore it, if we don't find a leaf
            // NOTE: if the EdgeVec stores adjacencies, this will create an adjacency with r and rv's data, so we're fine!
            append(active_edges, root, v_adj);
            if(add_active_edges_below(v, active_edges)) {
              result = true;
            } else {
              // can't use resize here because Edge might not be default-constructible
              // active_edges.resize(old_size); // if we haven't seen a leaf, then remove the added edges
              mstd::vector_shrink_to_size(active_edges, old_size);
            }
          }
        }
        return result;
      } else return true;
    }

  public:
    using typename Traits::value_type;
    using typename Traits::pointer;

    bool is_valid() const { return valid; }

    const NodeMap<ParentAutoIter>& get_active_parents() const { return active_parent; }

    SwitchingIter(): valid{false} {}

    SwitchingIter(const Net& N): roots{N.roots()} {
      for(const NodeDesc r: N.retis())
        append(active_parent, r, Net::parents(r));
    }

    AdjVec active_adjacencies() const {
      AdjVec result;
      for(const NodeDesc r: roots)
        add_active_edges_below(r, result);
      return result;
    }

    EdgeVec active_edges() const {
      EdgeVec result;
      for(const NodeDesc r: roots)
        add_active_edges_below(r, result);
      return result;
    }

    value_type operator*() const { return active_edges(); }
    pointer operator->() const { return operator*(); }

    SwitchingIter& operator++() {
      for(auto& [r, parent_it]: active_parent) {
        // if incrementing the parent iter is possible, we're fine
        if(++parent_it) return *this;
        // otherwise, continue to the next reticulation and reset this one's parent iter
        parent_it = Net::parents(r);
      }
      // if we arrive here, then we've looped through all switchings, so set it to invalid
      valid = false;
      return *this;
    }

    SwitchingIter operator++(int) { SwitchingIter old{*this}; ++(*this); return old; }

    bool operator==(const SwitchingIter& other) const {
      if(!valid) return !other.valid;
      if(!other.valid) return false;
      return (active_parent == other.active_parent);
    }
  };

  template<StrictPhylogenyType Net, mstd::VectorType OutputVec = NetEdgeVec<Net>>
  using SwitchingFactory = mstd::IterFactory<SwitchingIter<Net, OutputVec>>;

  // deduction guide for the factory (not allowed in C++20 yet)
  //template<typename Net>
  //SwitchingFactory(Net) -> SwitchingFactory<Net>;

}
