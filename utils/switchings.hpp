
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
    using ParentContainer = typename Net::ParentContainer;
    using ParentIter = mstd::iterator_of_t<ParentContainer>;
    using EdgeVec = NetEdgeVec<Net>;
    using AdjVec = NetAdjVec<Net>;

    auto& get_leaves() requires (not std::is_void_v<Leaves>) { return mstd::access(Tuple::template get<0>()); }
    auto& get_leaves() const requires (not std::is_void_v<Leaves>) { return mstd::access(Tuple::template get<0>()); }

    const Net* N = nullptr;

    // reticulations are on a stack of what is visible from the leaves in the current switching
    // NOTE: once the last adjacency in the stack is advanced over its last parent, the iter becomes invalid
    std::vector<NodeWith<ParentIter>> active_parent;
    OutputVec buffer;

    // NOTE: the active_edges are guaranteed in post-order of the switching (the tree containing all switched-on edges)
    // NOTE: if we have Leaves stored in the iter, then we'll use those to base the active edges, otherwise, we use what's passed, or N->leaves()
    template<class Out = OutputVec>
    Out get_active_edges() {
      Out out;
      NodeSet seen;
      NodeVec to_do;
      
      if constexpr (not std::is_void_v<Leaves>) {
        to_do.reserve(16);
        to_do.insert(to_do.end(), get_leaves().begin(), get_leaves().end());
      } else to_do = N->leaves().template to_container<NodeVec>;
      
      size_t retis_seen = 0;
      while(not to_do.empty()) {
        DEBUG6(std::cout << "next node: "<<to_do.back() << " ("<< to_do.size() - 1 << " to go)\n");
        NodeDesc& v = to_do.back();
        if(seen.emplace(v).second and (Net::in_degree(v) > 0)) {          
          ParentIter vp;
          if(Net::is_reti(v)) {
            assert(retis_seen <= active_parent.size());
            if(retis_seen == active_parent.size()) {
              // if it's the first time we encounter v on the path, then add a fresh parent-auto-iter to the stack and move along it
              vp = Net::parents(v).begin();
              append(active_parent, v, vp);
              DEBUG6(std::cout << "added "<<v<<" with it's first parent "<<*vp<<" to active_parent["<<retis_seen<<"]\n");
            } else vp = active_parent[retis_seen].second;
            ++retis_seen;
          } else vp = Net::parents(v).begin();
          // move along the current ParentIter, filling out
          if constexpr (mstd::AppendableR<Out, mstd::TR_RefOK, NodeDesc, decltype(*vp)>) {
            append(out, reverse_edge_tag{}, v, *vp);
          } else append(out, *vp);
          v = *vp;
        } else to_do.pop_back();
      }
      return out;
    }

  public:
    using typename Traits::value_type;
    using typename Traits::pointer;

    bool is_valid() const { return N != nullptr; }

    const auto& get_active_parents() const { return active_parent; }

    SwitchingIter() = default;

    template<class... Args> requires (not std::is_void_v<Leaves> && (sizeof...(Args) != 0) && std::is_constructible_v<Tuple, Args&&...>)
    SwitchingIter(const Net& _N, Args&&... args):
      Tuple{std::forward<Args>(args)...},
      N{&_N}
    { buffer = get_active_edges(); }

    SwitchingIter(const Net& _N):
      N(&_N)
    {
      if constexpr (not std::is_void_v<Leaves>)
        get_leaves() = N->leaves().template to_container<Leaves>();
      buffer = get_active_edges();
    }

    template<class... Args>
    auto active_adjacencies() const { return get_active_edges<AdjVec>();}
    template<class... Args>
    auto active_edges(Args&&... args) const { return get_active_edges<EdgeVec>();}

    auto& operator*() const { return buffer; }
    auto operator->() const { return &buffer; }

    SwitchingIter& operator++() {
      while(1) {
        if(active_parent.empty()) {
          DEBUG5(std::cout << "all switchings considered, iter is now invalid...\n");
          N = nullptr;
          assert(not is_valid());
          buffer.clear();
          return *this;
        } else {
          auto& [r, parent] = active_parent.back();
          if(++parent != Net::parents(r).end()) {
            buffer = get_active_edges();
            return *this;
          } else active_parent.pop_back();
        }
      }
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
