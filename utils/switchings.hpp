
#pragma once

#include "types.hpp"
#include "iter_factory.hpp"

namespace PT {


  // this is an iterator for switchings of a network
  template<StrictPhylogenyType Net, NodeContainerType<mstd::TR_VoidPtrOK> Leaves, mstd::VectorType OutputVec = NetEdgeVec<Net>>
  class SwitchingIter:
    public mstd::optional_tuple<Leaves>,
    public mstd::iter_traits_from_reference<OutputVec>
  {
    using Traits = mstd::iter_traits_from_reference<OutputVec>;
    using Tuple = mstd::optional_tuple<Leaves>;
    using ParentIter = mstd::iterator_of_t<typename Net::ParentContainer>;
    using ParentAutoIter = mstd::auto_iter<ParentIter>;
    using EdgeVec = NetEdgeVec<Net>;
    using AdjVec = NetAdjVec<Net>;

    auto& get_leaves() requires (not std::is_void_v<Leaves>) { return mstd::access(Tuple::template get<0>()); }
    auto& get_leaves() const requires (not std::is_void_v<Leaves>) { return mstd::access(Tuple::template get<0>()); }

    // we're going to store for each reticulation an iterator to its currently active parent-adjacency
    NodeMap<ParentAutoIter> active_parent;
    const Net* N = nullptr;

    // fill active_edges with the switched-on edges below root and return whether we found a leaf
    // NOTE: the active_edges are guaranteed in post-order of the switching (the tree containing all switched-on edges)
    // NOTE: if we have Leaves stored in the iter, then we'll use those to base the active edges, otherwise, we use what's passed, or N->leaves()
    template<mstd::VectorType EdgeVec, class... Args>
    EdgeVec get_active_edges(Args&&... args) const {
      EdgeVec active_edges;
      NodeSet seen;
      NodeVec to_do;
      
      if constexpr (not std::is_void_v<Leaves>) {
        to_do.reserve(128);
        to_do.insert(to_do.end(), get_leaves().begin(), get_leaves().end());
      } else if constexpr (sizeof...(Args) > 0) {
        mstd::append(to_do, std::forward<Args>(args)...);
      } else to_do = N->leaves.template to_container<NodeVec>;
      
      while(not to_do.empty()) {
        DEBUG5(std::cout << "next node: "<<to_do.back() << " ("<< to_do.size() - 1 << " to go)\n");
        NodeDesc& v = to_do.back();
        if(seen.emplace(v).second && (Net::in_degree(v) > 0)) {
          const auto u_adj = (Net::is_reti(v)) ? *active_parent.at(v) : Net::parent(v);
          append(active_edges, u_adj, v);
          v = u_adj;
        } else to_do.pop_back();
      }
      return active_edges;
    }

  public:
    using typename Traits::value_type;
    using typename Traits::pointer;

    bool is_valid() const { return N == nullptr; }

    const NodeMap<ParentAutoIter>& get_active_parents() const { return active_parent; }

    SwitchingIter() = default;

    template<class... Args> requires (not std::is_void_v<Leaves> && (sizeof...(Args) != 0) && std::is_constructible_v<Tuple, Args&&...>)
    SwitchingIter(const Net& _N, Args&&... args):
      Tuple{std::forward<Args>(args)...},
      N{&_N}
    {
      for(const NodeDesc r: N->retis_above(get_leaves())) {
        DEBUG5(std::cout << "switching-iter adding parents "<<Net::parents(r) <<" of "<<r<<'\n');
        append(active_parent, r, Net::parents(r));
      }
      DEBUG5(std::cout << "done making switching iter\n");
    }

    SwitchingIter(const Net& _N):
      N(&_N)
    {
      if constexpr (not std::is_void_v<Leaves>)
        get_leaves() = N->leaves().template to_container<Leaves>();
    }

    template<class... Args>
    auto active_adjacencies(Args&&... args) const { return get_active_edges<AdjVec>(std::forward<Args>(args)...);}
    template<class... Args>
    auto active_edges(Args&&... args) const { return get_active_edges<EdgeVec>(std::forward<Args>(args)...);}

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
      N = nullptr;
      return *this;
    }

    SwitchingIter operator++(int) { SwitchingIter old{*this}; ++(*this); return old; }

    bool operator==(const SwitchingIter& other) const {
      if(is_valid()) {
        if(other.is_valid()) {
          return (active_parent == other.active_parent);
        } else return false;
      } else return not other.is_valid();
    }
  };

  template<StrictPhylogenyType Net, NodeContainerType<mstd::TR_ConstRefPtrOK> Leaves, mstd::VectorType OutputVec = NetEdgeVec<Net>>
  using SwitchingFactory = mstd::IterFactory<SwitchingIter<Net, Leaves, OutputVec>>;

  //------------- deduction guides ------------------
  template<typename Net>
  SwitchingIter(Net) -> SwitchingIter<Net, NodeVec>;

}
