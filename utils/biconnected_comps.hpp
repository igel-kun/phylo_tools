
// this iterates over biconnected components in post-order

#pragma once

#include "union_find.hpp"
#include "concat_iter.hpp"

#include "extract_data.hpp"
#include "types.hpp"
#include "cuts.hpp"

namespace PT{

  template<StrictPhylogenyType Network>
  using BCCChainDecomposition = ChainDecomposition<Network, CutObject::bcc>;

  // NOTE: the BasicBCCIter OWNS the chain decomposition
  template<StrictPhylogenyType Network>
  using BasicBCCIter = BCCCutIter<Network, postorder, BCCChainDecomposition<Network>>;

  //------------- STEP 1: for each cut-node, get its child-container ---------------------
  template<StrictPhylogenyType Network>
  using CutNodeChildContainerIterator = mstd::transforming_iterator<BasicBCCIter<Network>, functor_children_of<Network>>;

  // ------------- STEP 2: concatenate the child-containers of all cut-nodes -------------------
  template<StrictPhylogenyType Network>
  using CutNodeChildrenIterator = mstd::concatenating_iterator<CutNodeChildContainerIterator<Network>>;


  // ------------- STEP 3: filter the children that, with their cut-node, form a BCC-starting edge ------------
  template<StrictPhylogenyType Network, bool allow_trivial = true>
  struct IsBCCStartEdge {
    using Iterator = CutNodeChildrenIterator<Network>;
    using BasicIter = BasicBCCIter<Network>;

    // we will get the concatenating iterator listing cut-nodes transformed into their child-container
    bool operator()(const Iterator& iter) const {
      const auto& cut_it = static_cast<const BasicIter&>(iter);
      const auto& chains = cut_it.get_predicate();
      DEBUG4(std::cout << "checking edge "<<*cut_it<<" -> "<<*iter<<" for BCC - "<< mstd::access(chains).is_first_edge_in_bcc(*cut_it, *iter)  <<"\n");
      if constexpr (allow_trivial)
        return mstd::access(chains).is_first_edge_in_bcc(*cut_it, *iter);
      else 
        return mstd::access(chains).is_first_edge_in_nontrivial_bcc(*cut_it, *iter);
    }
  };
  template<StrictPhylogenyType Network, bool allow_trivial = true>
  using BCCStartingCutNodeChildIterator = mstd::filtered_iterator<CutNodeChildrenIterator<Network>, IsBCCStartEdge<Network, allow_trivial>, true>;


  // ------------------- STEP 5: transform the filtered child-nodes into biconnected components using a BCCmaker ----------------
  // NOTE: the filter owns (1) a seen-set, (2) the cached output components, and (3) the emplacer for the output component
  template<StrictPhylogenyType Network_,
           StrictPhylogenyType Component_ = Network_,
           StrictEdgeEmplacerType Emplacer = EdgeEmplacer<EdgeEmplacementHelper<Component_, false, NodeTranslation>, DataExtracter<Network_>>>
  struct BCCmaker {
    using Network = Network_;
    using Component = Component_;
    using EmplacementHelper = typename Emplacer::Helper;
    using Extracter = typename Emplacer::Extracter;
    
    mutable NodeSet seen;
    mutable Component output; // cache the current component to be output on operator*
    mutable Emplacer output_emplacer;

    BCCmaker() = delete; //: output_emplacer(output) {}

    template<class First, class... Args> 
      requires (not mstd::is_any_of<First, BCCmaker, std::piecewise_construct_t>)
    BCCmaker(First&& first, Args&&... args):
      output_emplacer{output, std::forward<First>(first), std::forward<Args>(args)...}
    { // the output should now be connected to the emplacer
      assert(output_emplacer.helper.N == &output);
    }

    // NOTE: when piecewise_construct is passed, we need to smuggle-in our output as a network parameter, so we have to extract and repack the first tuple
    template<mstd::TupleType First, mstd::TupleType Second>
    BCCmaker(std::piecewise_construct_t, First&& first, Second&& second):
      output_emplacer(
        std::make_from_tuple<EmplacementHelper>(std::tuple_cat(std::forward_as_tuple(output), std::forward<First>(first))),
        std::make_from_tuple<Extracter>(std::forward<Second>(second)))
    {
      assert(output_emplacer.helper.N == &output);
    }

    // BCCMakers should not be copied, only moved and if they are moved, we have to remember to reset the pointer to output in the Emplacement Helper
    BCCmaker(const BCCmaker& other) = delete;
    BCCmaker(BCCmaker&& other):
      seen{std::move(other).seen}, output{std::move(other).output}, output_emplacer{std::move(other).output_emplacer}
    {
      output_emplacer.helper.N = &output;
    }
  
    // construct a biconnected component rooted at rt and containing v; construct it using the output_emplacer
    // NOTE: initially, (rt,v) is one of the top-edges of the component
    // NOTE: rt might be the root of many different biconnected components, so passing v is essential!
    // NOTE: other outgoing arcs of rt might also be in the same BCC, so we have to explore both upwards and downwards from v
    // NOTE: remember to set the root of the output component after calling this!
    void make_component_along(const NodeDesc rt, const NodeDesc v) const {
      if(append(seen, v).second){
        DEBUG4(std::cout << "BCC: making component along " << v << " (root "<<rt<<")\n");
        // step 1: emplace all in-edges of v
        for(auto uv: Network::in_edges(v))
          output_emplacer.emplace_edge_translated(uv);
        // step 2: recurse for all non-root parents of v
        for(const NodeDesc u: Network::parents(v)) 
          if(u != rt)
            make_component_along(rt, u);
        // step 3: recurse for all children of v
        for(const NodeDesc w: Network::children(v))
          make_component_along(rt, w);
      }
    }

    void make_component(const NodeDesc u, const NodeDesc v) const {
      output.clear();
      output_emplacer.clear();
      make_component_along(u, v);
      output_emplacer.mark_root(u);
      assert(output_emplacer.helper.N == &output);
      assert(not output.roots().empty());
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
           EdgeEmplacerType Emplacer = EdgeEmplacer<EdgeEmplacementHelper<Component, false, NodeTranslation>, DataExtracter<Network>>>
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
    using Emplacer = EdgeEmplacer<EdgeEmplacementHelper<Component, false, OldToNewRef>, Extracter>;
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
    //BCCBeginEnd(const BCCBeginEnd&) = default;
    //BCCBeginEnd(BCCBeginEnd&&) = default;

    // old_to_new can be initialized in 3 ways: default, copy, and move
    // (1) default:
    template<class First, class... Args>
      requires (not mstd::is_same_v<First, StrictOldToNew>)
    BCCBeginEnd(const NodeDesc rt, const size_t num_nodes, First&& first, Args&&... args):
      chains{rt, num_nodes}, old_to_new{}, extracter{std::forward<First>(first), std::forward<Args>(args)...} {}
    // (2) copy or move
    template<class First, class... Args>
      requires mstd::is_same_v<First, StrictOldToNew>
    BCCBeginEnd(const NodeDesc rt, const size_t num_nodes, First&& _old_to_new, Args&&... args):
      chains{rt, num_nodes}, old_to_new{std::forward<First>(_old_to_new)}, extracter{std::forward<Args>(args)...} {}

    BCCBeginEnd(const NodeDesc rt, const size_t num_nodes):
      chains{rt, num_nodes}, old_to_new{}, extracter{} {}

    // instead of a root and the number of nodes, we can also get that from a network
    template<class... Args>
    BCCBeginEnd(const Network& N, Args&&... args):
      BCCBeginEnd(N.root(), N.num_nodes(), std::forward<Args>(args)...)
    {}


    template<class Iter, class Decomp, class... Args> requires (mstd::is_same_v<Iter, InIterator>)
    static OutIterator construct_bcc_iter(Iter&& iter, Decomp&& chain_decomp, Args&&... args) {
      DEBUG4(std::cout << "creating new BCC iterator\n");
      auto a = BasicBCCIter<Network>(std::forward<Iter>(iter), std::forward<Decomp>(chain_decomp));
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
      return construct_bcc_iter(std::forward<Iter>(iter), chains, std::piecewise_construct, std::tuple{&old_to_new}, std::forward_as_tuple(extracter));
    }
    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) & {
      return construct_bcc_iter(std::forward<Iter>(iter), chains, std::piecewise_construct, std::tuple{&old_to_new}, std::forward_as_tuple(extracter));
    }
    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) && {
      return construct_bcc_iter(
          std::forward<Iter>(iter),
          std::move(chains),
          std::piecewise_construct,
          std::forward_as_tuple(std::move(old_to_new)), std::forward_as_tuple(std::move(extracter)));
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
    template<class... Args>
    BiconnectedComponents(const NodeDesc rt, const size_t num_nodes, Args&&... args):
      Parent(std::piecewise_construct,
          std::forward_as_tuple(rt, num_nodes, std::forward<Args>(args)...), // make the BCCBeginEnd transformation
          std::forward_as_tuple(rt)) // make the NodeTraversal part of the BasicBCCIter
    {}
  };

  // deduce parameters from arguments
  // NOTE: if you don't want the biconnected component to have the same type as the Network, then pass a StrictPhylogenyType as first template parameter
  template<OptionalStrictPhylogenyType Component_ = void,
           bool allow_trivial = true,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           StrictPhylogenyType Network,
           class... ExtracterArgs>
  auto get_biconnected_components(const Network& N, OldToNewTranslation&& old_to_new = OldToNewTranslation(), ExtracterArgs&&... ex_args) {
    using Component = mstd::FirstNonVoid<Component_, Network>;
    using Extracter = std::remove_reference_t<decltype(make_data_extracter<Network>(std::forward<ExtracterArgs>(ex_args)...))>;
    return BiconnectedComponents<Network, Component, allow_trivial, OldToNewTranslation, Extracter>(
        N,
        std::forward<OldToNewTranslation>(old_to_new),
        std::forward<ExtracterArgs>(ex_args)...);
  }

  template<OptionalStrictPhylogenyType Component_ = void,
           bool allow_trivial = true,
           StrictPhylogenyType Network,
           class First,
           class... ExtracterArgs>
             requires (not NodeTranslationType<First>)
  auto get_biconnected_components(const Network& N, First&& first, ExtracterArgs&&... ex_args) {
    using Component = mstd::FirstNonVoid<Component_, Network>;
    using Extracter = std::remove_reference_t<decltype(make_data_extracter<Network>(std::forward<First>(first), std::forward<ExtracterArgs>(ex_args)...))>;
    return BiconnectedComponents<Network, Component, allow_trivial, NodeTranslation, Extracter>(
        N,
        std::forward<First>(first),
        std::forward<ExtracterArgs>(ex_args)...);
  }

  template<OptionalStrictPhylogenyType Component_ = void, class... Args>
  auto get_nontrivial_biconnected_components(Args&&... args) {
    return get_biconnected_components<Component_, false>(std::forward<Args>(args)...);
  }

}// namespace
