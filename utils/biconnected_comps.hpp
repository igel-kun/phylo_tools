
// this iterates over biconnected components in post-order

#pragma once

#include "union_find.hpp"

#include "extract_data.hpp"
#include "types.hpp"
#include "cuts.hpp"

namespace PT{

  template<PhylogenyType Network>
  using BCCChainDecomposition = ChainDecomposition<Network, CutObject::bcc>;
  template<PhylogenyType Network>
  using BasicBCCIter = BCCCutIter<Network, postorder, const BCCChainDecomposition<Network>*>;

  //------------- STEP 1: for each cut-node, get its child-container ---------------------
  template<StrictPhylogenyType Network>
  using CutNodeChildContainerIterator = mstd::transforming_iterator<BasicBCCIter<Network>, functor_children_of<Network>>;

  // ------------- STEP 2: concatenate the child-containers of all cut-nodes -------------------
  template<StrictPhylogenyType Network>
  using CutNodeChildrenIterator = mstd::concatenating_iterator<CutNodeChildContainerIterator<Network>>;


  // ------------- STEP 3: filter the children that, with their cut-node, form a BCC-starting edge ------------
  template<PhylogenyType Network, bool allow_trivial = true>
  struct BCCStartEdge {
    using Iterator = CutNodeChildrenIterator<Network>;
    // we will get the concatenating iterator listing cut-nodes transformed into their child-container
    bool operator()(const Iterator& iter) const {
      const auto& cut_it = static_cast<const BasicBCCIter<Network>&>(iter);
      const auto& chains = cut_it.get_predicate();
      DEBUG4(std::cout << "checking edge "<<*cut_it<<" -> "<<*iter<<" for BCC - "<< chains->is_first_edge_in_bcc(*cut_it, *iter)  <<"\n");
      if constexpr (allow_trivial)
        return chains->is_first_edge_in_bcc(*cut_it, *iter);
      else 
        return chains->is_first_edge_in_nontrivial_bcc(*cut_it, *iter);
    }
  };
  template<StrictPhylogenyType Network, bool allow_trivial = true>
  using BCCStartingCutNodeChildIterator = mstd::filtered_iterator<CutNodeChildrenIterator<Network>, BCCStartEdge<Network, allow_trivial>, true>;


  // ------------------- STEP 5: transform the filtered child-nodes into biconnected components using a BCCmaker ----------------
  template<StrictPhylogenyType _Network,
           StrictPhylogenyType _Component = _Network,
           StrictEdgeEmplacerType Emplacer = EdgeEmplacerWithHelper<false, _Component, _Network, NodeTranslation*, DataExtracter<_Network>>>
  struct BCCmaker {
    using Network = _Network;
    using Component = _Component;
    using EmplacementHelper = typename Emplacer::Helper;
    using Extracter = typename Emplacer::Extracter;
    
    mutable NodeSet seen;
    mutable Component output; // cache the current component to be output on operator*
    mutable Emplacer output_emplacer;

    BCCmaker() = delete; //: output_emplacer(output) {}

    template<class First, class... Args> 
      requires (not mstd::is_same_v<First, BCCmaker> and not mstd::is_same_v<First, std::piecewise_construct_t>)
    BCCmaker(First&& first, Args&&... args):
      output_emplacer{output, std::forward<First>(first), std::forward<Args>(args)...}
    {}

    // NOTE: when piecewise_construct is passed, we need to smuggle-in our output as a network parameter, so we have to extract and repack the first tuple
    template<mstd::TupleType First, mstd::TupleType Second>
    BCCmaker(std::piecewise_construct_t, First&& first, Second&& second):
      output_emplacer(
        std::make_from_tuple<EmplacementHelper>(std::tuple_cat(std::forward_as_tuple(output), std::forward<First>(first))),
        std::make_from_tuple<Extracter>(std::forward<Second>(second)))
    {}

    BCCmaker(const BCCmaker& other):
      seen{other.seen}, output{}, output_emplacer{other.output_emplacer} {}
    BCCmaker(BCCmaker&& other) = default;
  
    // construct a biconnected component containing the arc uv and store it in 'output'
    //NOTE: remember to set the root of the output component after calling this!
    void make_component_along(const NodeDesc rt, const NodeDesc v) const {
      if(append(seen, v).second){
        DEBUG4(std::cout << "BCC: making component along " << v << " (root "<<rt<<")\n");
        const auto& v_node = node_of<Network>(v); // NOTE: make_data.second may want to change the edge-data of the v_node, so we cannot pass it as const
        for(auto uv: v_node.in_edges()) output_emplacer.emplace_edge(uv);
        for(const NodeDesc u: v_node.parents()) 
          if(u != rt)
            make_component_along(rt, u);
        for(const NodeDesc w: v_node.children())
          make_component_along(rt, w);
      }
    }

    void make_component(const NodeDesc u, const NodeDesc v) const {
      output.clear();
      output_emplacer.clear();
      make_component_along(u, v);
      output_emplacer.mark_root(u);
    }

    Component& operator()(const NodeDesc u, const NodeDesc v) const {
      if(output.empty()) make_component(u, v);
      return output;
    }
    BCCmaker& operator++() { output.clear(); return *this; }

    template<bool allow_trivial>
    Component& operator()(const BCCStartingCutNodeChildIterator<Network, allow_trivial>& iter) const {
      DEBUG4(std::cout << "making new BCC\n");
      const auto& cut_it = static_cast<const BasicBCCIter<Network>&>(iter);
      DEBUG4(std::cout << "anchor 1: "<<*cut_it<<"\n");
      DEBUG4(std::cout << "anchor 2: "<<*iter<<"\n");
      return operator()(*cut_it, *iter);
    }
  };

  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           bool allow_trivial = true,
           EdgeEmplacerType Emplacer = EdgeEmplacerWithHelper<false, Component, Network, NodeTranslation*, DataExtracter<Network>>>
  using BCCIterator = mstd::transforming_iterator<BCCStartingCutNodeChildIterator<Network, allow_trivial>, BCCmaker<Network, Component, Emplacer>, true>;


  // the factory for BCC iterators will store
  // (1) the BCCChainDecomposition that is const-referenced in all BCCIterators
  // (2) the node-translation from network-nodes to BCC-nodes
  // (3) the extracter used to extract data and labels from the network
  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           bool allow_trivial = true,
           NodeTranslationType OldToNew = NodeTranslation,
           DataExtracterType Extracter = DataExtracter<Network>>
  struct BCCBeginEnd {
    using StrictOldToNew = std::remove_cvref_t<OldToNew>;
    using OldToNewRef = StrictOldToNew&;
    using Emplacer = EdgeEmplacerWithHelper<false, Component, Network, OldToNewRef, Extracter>;
    using InIterator = typename BasicBCCIter<Network>::Iterator;
    using OutIterator = BCCIterator<Network, Component, allow_trivial, Emplacer>;
    using MyBCCmaker = BCCmaker<Network, Component, Emplacer>;

    // NOTE: this will break on GCC before version 11.3
    static_assert(mstd::really_pre_incrementable<MyBCCmaker>);

    // note: "const_iterators" returned by "begin() const" may actually modify the global "old_to_new" map that is accumulating all node-translations
    BCCChainDecomposition<Network> chains;
    mutable OldToNew old_to_new;
    Extracter extracter;

    BCCBeginEnd() = default;
    BCCBeginEnd(const BCCBeginEnd&) = default;
    BCCBeginEnd(BCCBeginEnd&&) = default;

    // old_to_new can be initialized in 3 ways: default, copy, and move
    // (1) default:
    template<class First, class... Args>
      requires (not mstd::is_same_v<First, StrictOldToNew> && not mstd::is_same_v<First, BCCBeginEnd>)
    BCCBeginEnd(const Network& N, First&& first, Args&&... args):
      chains{N}, old_to_new{}, extracter{std::forward<First>(first), std::forward<Args>(args)...} {}
    // (2) copy
    template<class... Args>
    BCCBeginEnd(const Network& N, const OldToNew& _old_to_new, Args&&... args):
      chains{N}, old_to_new{_old_to_new}, extracter{std::forward<Args>(args)...} {}
    // (3) move
    template<class... Args>
    BCCBeginEnd(const Network& N, OldToNew&& _old_to_new, Args&&... args):
      chains{N}, old_to_new{std::move(_old_to_new)}, extracter{std::forward<Args>(args)...} {}

    template<class Iter, class... Args> requires (mstd::is_same_v<Iter, InIterator>)
    OutIterator construct_bcc_iter(Iter&& iter, Args&&... args) const {
      DEBUG4(std::cout << "creating new BCC iterator\n");
      auto a = BasicBCCIter<Network>(std::forward<Iter>(iter), &chains);
      auto b = CutNodeChildContainerIterator<Network>(std::piecewise_construct, std::tuple{std::move(a)}, std::tuple{});
      auto c = CutNodeChildrenIterator<Network>(std::move(b));
      auto d = BCCStartingCutNodeChildIterator<Network, allow_trivial>(std::piecewise_construct, std::tuple{std::move(c)}, std::tuple{});  
      auto bcc_maker = BCCmaker<Network, Component, Emplacer>(std::forward<Args>(args)...);
      auto e = OutIterator(std::piecewise_construct, std::forward_as_tuple(std::move(d)), std::forward_as_tuple(std::move(bcc_maker)));
      return e;
    }

    // we're going to use references to our extracter and our old_to_new translation when we're not going out of scope
    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) const & {
      return construct_bcc_iter(std::forward<Iter>(iter), std::piecewise_construct, std::tuple{&old_to_new}, std::tuple{extracter});
    }
    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) & {
      return construct_bcc_iter(std::forward<Iter>(iter), std::piecewise_construct, std::tuple{&old_to_new}, std::tuple{extracter});
    }
    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) && {
      return construct_bcc_iter(std::forward<Iter>(iter),
          std::piecewise_construct, std::forward_as_tuple(std::move(old_to_new)), std::forward_as_tuple(std::move(extracter)));
    }
    auto operator()(const mstd::GenericEndIterator& i) const { return i; }

  };

  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           bool allow_trivial = true,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           DataExtracterType Extracter = DataExtracter<Network>>
  struct BiconnectedComponents: public mstd::IterFactoryWithBeginEnd<
                                  typename BasicBCCIter<Network>::Iterator,
                                  BCCBeginEnd<Network, Component, allow_trivial, OldToNewTranslation, Extracter>>
  {
    using MyBeginEnd = BCCBeginEnd<Network, Component, allow_trivial, OldToNewTranslation, Extracter>;
    using Parent = mstd::IterFactoryWithBeginEnd<typename BasicBCCIter<Network>::Iterator, MyBeginEnd>;

    template<class... Args>
    BiconnectedComponents(const Network& N, Args&&... args):
      Parent(std::piecewise_construct, std::forward_as_tuple(N, std::forward<Args>(args)...), std::forward_as_tuple(N))
    {}
  };

  // deduce parameters from arguments
  // NOTE: if you don't want the biconnected component to have the same type as the Network, then pass a StrictPhylogenyType as first template parameter
  template<OptionalStrictPhylogenyType _Component = void,
           bool allow_trivial = true,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           StrictPhylogenyType Network,
           class... ExtracterArgs>
  auto get_biconnected_components(const Network& N, OldToNewTranslation&& old_to_new = OldToNewTranslation(), ExtracterArgs&&... ex_args) {
    using Component = mstd::FirstNonVoid<_Component, Network>;
    using Extracter = decltype(make_data_extracter<Network>(std::forward<ExtracterArgs>(ex_args)...));
    return BiconnectedComponents<Network, Component, allow_trivial, OldToNewTranslation, Extracter>(
        N,
        std::forward<OldToNewTranslation>(old_to_new),
        std::forward<ExtracterArgs>(ex_args)...);
  }

  template<OptionalStrictPhylogenyType _Component = void,
           bool allow_trivial = true,
           StrictPhylogenyType Network,
           class First,
           class... ExtracterArgs>
             requires (!NodeTranslationType<First>)
  auto get_biconnected_components(const Network& N, First&& first, ExtracterArgs&&... ex_args) {
    using Component = mstd::FirstNonVoid<_Component, Network>;
    using Extracter = decltype(make_data_extracter<Network>(std::forward<First>(first), std::forward<ExtracterArgs>(ex_args)...));
    return BiconnectedComponents<Network, Component, allow_trivial, NodeTranslation, Extracter>(
        N,
        std::forward<First>(first),
        std::forward<ExtracterArgs>(ex_args)...);
  }

  template<OptionalStrictPhylogenyType _Component = void, class... Args>
  auto get_nontrivial_biconnected_components(Args&&... args) {
    return get_biconnected_components<_Component, false>(std::forward<Args>(args)...);
  }

}// namespace
