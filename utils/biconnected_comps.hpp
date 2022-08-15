
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
  using BasicBCCIter = BCCCutIter<Network, postorder, const BCCChainDecomposition<Network>&>;


  template<class Network>
  struct functor_children_of { decltype(auto) operator()(const NodeDesc u){ return Network::children(u); } };

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
      std::cout << "checking edge "<<*cut_it<<" -> "<<*iter<<" for BCC - "<< chains.is_first_edge_in_bcc(*cut_it, *iter)  <<"\n";
      if constexpr (allow_trivial)
        return chains.is_first_edge_in_bcc(*cut_it, *iter);
      else 
        return chains.is_first_edge_in_nontrivial_bcc(*cut_it, *iter);
    }
  };
  template<StrictPhylogenyType Network, bool allow_trivial = true>
  using BCCStartingCutNodeChildIterator = mstd::filtered_iterator<CutNodeChildrenIterator<Network>, BCCStartEdge<Network, allow_trivial>, true>;


  // ------------------- STEP 5: transform the filtered child-nodes into biconnected components using a BCCmaker ----------------
  template<StrictPhylogenyType _Network,
           StrictPhylogenyType _Component = _Network,
           StrictEdgeEmplacerType Emplacer = EdgeEmplacerWithHelper<false, _Component, _Network, NodeTranslation, DataExtracter<_Network>>>
  struct BCCmaker {
    using Network = _Network;
    using Component = _Component;
    
    mutable NodeSet seen;
    mutable Component output; // cache the current component to be output on operator*
    mutable Emplacer output_emplacer;

    BCCmaker() = delete; //: output_emplacer(output) {}

    template<class First, class... Args> requires (!std::is_same_v<BCCmaker, std::remove_cvref_t<First>>)
    BCCmaker(First&& first, Args&&... args):
      output_emplacer{output, std::forward<First>(first), std::forward<Args>(args)...} {}

    BCCmaker(const BCCmaker& other):
      seen{other.seen}, output{}, output_emplacer{other.output_emplacer, output} {}
    BCCmaker(BCCmaker&& other):
      seen{std::move(other.seen)}, output{std::move(other.output)}, output_emplacer{std::move(other.output_emplacer), output} {}
  
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
      std::cout << "making new BCC\n";
      const auto& cut_it = static_cast<const BasicBCCIter<Network>&>(iter);
      std::cout << "anchor 1: "<<*cut_it<<"\n";
      std::cout << "anchor 2: "<<*iter<<"\n";
      return operator()(*cut_it, *iter);
    }
  };

  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           bool allow_trivial = true,
           EdgeEmplacerType Emplacer = EdgeEmplacerWithHelper<false, Component, Network, NodeTranslation, DataExtracter<Network>>>
  using BCCIterator = mstd::transforming_iterator<BCCStartingCutNodeChildIterator<Network, allow_trivial>, BCCmaker<Network, Component, Emplacer>, true>;





  // the factory for BCC iterators will store
  // (1) the BCCChainDecomposition that is const-referenced in all BCCIterators
  // (2) the node-translation from network-nodes to BCC-nodes
  // (3) the extracter used to extract data and labels from the network
  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           bool allow_trivial = true,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           DataExtracterType Extracter = DataExtracter<Network>>
  struct BCCBeginEnd {
    using Emplacer = EdgeEmplacerWithHelper<false, Component, Network, OldToNewTranslation, Extracter>;
    using InIterator = typename BasicBCCIter<Network>::Iterator;
    using OutIterator = BCCIterator<Network, Component, allow_trivial, Emplacer>;
    using StrictOldToNew = std::remove_cvref_t<OldToNewTranslation>;
    using MyBCCmaker = BCCmaker<Network, Component, Emplacer>;

    // NOTE: this will break on GCC before version 11.3
    static_assert(mstd::really_pre_incrementable<MyBCCmaker>);
    
    mutable BCCChainDecomposition<Network> chains;
    OldToNewTranslation old_to_new;
    Extracter extracter;

    BCCBeginEnd() = default;
    BCCBeginEnd(const BCCBeginEnd&) = default;
    BCCBeginEnd(BCCBeginEnd&&) = default;

    // old_to_new can be initialized in 3 ways: default, copy, and move
    // (1) default:
    template<class First, class... Args>
      requires (!std::is_same_v<std::remove_cvref_t<First>, StrictOldToNew> && !std::is_same_v<std::remove_cvref_t<First>, BCCBeginEnd>)
    BCCBeginEnd(const Network& N, First&& first, Args&&... args):
      chains{N}, old_to_new{}, extracter{std::forward<First>(first), std::forward<Args>(args)...} {}
    // (2) copy
    template<class... Args>
    BCCBeginEnd(const Network& N, const StrictOldToNew& _old_to_new, Args&&... args):
      chains{N}, old_to_new{_old_to_new}, extracter{std::forward<Args>(args)...} {}
    // (3) move
    template<class... Args>
    BCCBeginEnd(const Network& N, StrictOldToNew&& _old_to_new, Args&&... args):
      chains{N}, old_to_new{std::move(_old_to_new)}, extracter{std::forward<Args>(args)...} {}

    template<class Iter, class... Args> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator construct_bcc_iter(Iter&& iter, Args&&... args) const & {
      std::cout << "creating new BCC iterator\n";
      auto a = BasicBCCIter<Network>(std::forward<Iter>(iter), chains);
      auto b = CutNodeChildContainerIterator<Network>(std::piecewise_construct, std::tuple{std::move(a)}, std::tuple{});
      auto c = CutNodeChildrenIterator<Network>(std::move(b));
      auto d = BCCStartingCutNodeChildIterator<Network, allow_trivial>(std::piecewise_construct, std::tuple{std::move(c)}, std::tuple{});  
      auto bcc_maker = BCCmaker<Network, Component, Emplacer>(std::forward<Args>(args)...);
      auto e = OutIterator(std::piecewise_construct, std::tuple{std::move(d)}, std::forward_as_tuple(std::move(bcc_maker)));
      return e;
    }

    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) const & {
      return construct_bcc_iter(std::forward<Iter>(iter), old_to_new, extracter);
    }
    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) & {
      return construct_bcc_iter(std::forward<Iter>(iter), old_to_new, extracter);
    }
    template<class Iter> requires (std::is_same_v<std::remove_cvref_t<Iter>, InIterator>)
    OutIterator operator()(Iter&& iter) && {
      return construct_bcc_iter(std::forward<Iter>(iter), std::move(old_to_new), std::move(extracter));
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
  template<StrictOptionalPhylogenyType _Component = void,
           bool allow_trivial = true,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           StrictPhylogenyType Network,
           class... ExtracterArgs>
  auto get_biconnected_components(const Network& N, OldToNewTranslation&& old_to_new = OldToNewTranslation(), ExtracterArgs&&... ex_args) {
    using Component = mstd::VoidOr<_Component, Network>;
    using Extracter = decltype(make_data_extracter<Network, Component>(std::forward<ExtracterArgs>(ex_args)...));
    return BiconnectedComponents<Network, Component, allow_trivial, OldToNewTranslation, Extracter>(
        N,
        std::forward<OldToNewTranslation>(old_to_new),
        std::forward<ExtracterArgs>(ex_args)...);
  }

  template<StrictOptionalPhylogenyType _Component = void,
           bool allow_trivial = true,
           StrictPhylogenyType Network,
           class First,
           class... ExtracterArgs>
             requires (!NodeTranslationType<First>)
  auto get_biconnected_components(const Network& N, First&& first, ExtracterArgs&&... ex_args) {
    using Component = mstd::VoidOr<_Component, Network>;
    using Extracter = decltype(make_data_extracter<Network, Component>(std::forward<First>(first), std::forward<ExtracterArgs>(ex_args)...));
    return BiconnectedComponents<Network, Component, allow_trivial, NodeTranslation, Extracter>(
        N,
        std::forward<First>(first),
        std::forward<ExtracterArgs>(ex_args)...);
  }

  template<StrictOptionalPhylogenyType _Component = void, class... Args>
  auto get_nontrivial_biconnected_components(Args&&... args) {
    return get_biconnected_components<_Component, false>(std::forward<Args>(args)...);
  }





/*
  template<StrictPhylogenyType _Network,
           StrictPhylogenyType Component = _Network,
           EdgeEmplacerType Emplacer = EdgeEmplacerWithHelper<false, Component, _Network, NodeTranslation, DataExtracter<_Network>>>
  using BiconnectedComponentIter =
          mstd::transforming_iterator<
            mstd::filtered_iterator<
              mstd::concatenating_iterator<


  // NOTE: only enumeration of bccs of a **single-rooted** network is supported for now
  // NOTE: ExtractNodeData::operator() must take a NodeDesc and return something that is passed to Component::NodeData()
  //       ExtractEdgeData::operator() must take an Adjacency& and a const NodeDesc and return something that is passed to Component::EdgeData()
  template<StrictPhylogenyType _Network,
           StrictPhylogenyType Component = _Network,
           EdgeEmplacerType Emplacer = EdgeEmplacerWithHelper<false, Component, _Network, NodeTranslation, DataExtracter<_Network>>>
  class BiconnectedComponentIter: public CutIter<_Network, CutObject::bcc> {
    using Parent = CutIter<_Network, CutObject::bcc>;
  public:
    using Network = _Network;
    using Node = typename Network::Node;

    using Traits = typename Parent::Traits;
    using typename Traits::value_type;
    using typename Traits::reference;
    using typename Traits::const_reference;
    using typename Traits::pointer;
    using typename Traits::const_pointer;

    using Parent::is_valid;
  protected:
    using Parent::chains;

    // a predicate that returns true for an edge that starts a biconnected component
    friend struct BCCStart {
      BiconnectedComponentIter* bcc_iter = nullptr;
      bool operator()(const NodeDesc v) const { return bcc_iter->chains.is_first_edge_in_bcc(bcc_iter->get_current_cut_node(), v); }
    };

    using ChildIterator = mstd::iterator_of_t<mstd::copy_cvref_t<Network, typename Network::ChildContainer>>;
    using FilteredChildIter = mstd::filtered_iterator<ChildIterator, BCCStart>;

    // NOTE: for each cut-node u, we will iterate over its children v in the network and build a new bcc if v's bcc-root is u
    // NOTE: we will use the CutIter's chain decomposition to tell whether two siblings are in the same component
    FilteredChildIterator child_it;
    BCCmaker make_component;

    NodeDesc get_current_cut_node() const { assert(is_valid()); return Parent::operator*(); }
    decltype(auto) get_current_children() const { assert(is_valid()); return Network::children(get_current_cut_node()); }

    // this is called when we're through with the current cut-node, in order to
    // 1. advance to the next cut node
    // 2. set up the queue of children of u that are in BCCs
    // 3. initialize current_child
    void advance_parent() {
      assert(is_valid());
      // step 1: advance the cut-node iterator
      Parent::operator++();
      initialize_child_iter();
    }

    void initialize_child_iter() {
      if(is_valid()) {
        assert(!get_current_children().empty());
        DEBUG4(std::cout << "BCC: cut node "<< get_current_cut_node() <<" with children "<<get_current_children()<<"\n");
        child_it = get_current_children().begin();
      } else DEBUG3(std::cout << "BCC: that's it, no more BCCs\n");
    }

    void next_component() {
      if(is_valid()) {
        DEBUG4(std::cout << "BCC: getting next biconnected component...\n");
        output.reset();
        old_to_new.clear();
        ++child_it;
        if(!child_it.is_valid())
          advance_parent();
      }
    }

  public:
    // NOTE: cut-nodes MUST be bottom-up (that is, no cut-node x can preceed any descendant of x)!
    template<class ParentInit, NodeTranslationType _OldToNewTranslation = NodeTranslation, class... Args>
    BiconnectedComponentIter(ParentInit&& _parent, _OldToNewTranslation&& _old_to_new, Args&&... args):
      Parent(std::forward<ParentInit>(_parent)),
      child_it{mstd::filter_only_t{}, this},
      make_component{std::forward<_OldToNewTranslation>(_old_to_new), std::forward<Args>(args)...}
    { initialize_child_iter(); }


    // NOTE: the copy constructor does NOT copy the output Component
    BiconnectedComponentIter(const BiconnectedComponentIter& other):
      Parent(other),
      child_it(other.child_it, this),
      make_component(other.make_component)
    {}
    BiconnectedComponentIter(BiconnectedComponentIter&& other):
      Parent(std::move(other)),
      child_it(std::move(other.child_it), this),
      make_component(std::move(other.make_component))
    {}


    BiconnectedComponentIter& operator=(const BiconnectedComponentIter& other) {
      if(this != &other)
        operator=(BiconnectedComponentIter(other));
      return *this;
    }
    BiconnectedComponentIter& operator=(BiconnectedComponentIter&& other) = default;

    BiconnectedComponentIter& operator++() { next_component(); return *this; }
    BiconnectedComponentIter& operator++(int) = delete;

    bool operator==(const BiconnectedComponentIter& other) const {
      if(is_valid()) {
        if(other.is_valid()) {
          return child_comp_iter == other.child_comp_iter;
        } else return false;
      } else return !other.is_valid();
    }

    // NOTE: calling operator* is expensive, consider calling it at most once for each item
    reference operator*() {
      assert(is_valid());
      if(!output) make_component(*child_it);  // step2: construct the component along the current child
      return *output;
    }
    const_reference operator*() const {
      assert(is_valid());
      if(!output) make_component(*child_it);  // step2: construct the component along the current child
      return *output;
    }
    pointer operator->() { return &(operator*()); }
    const_pointer operator->() const { return &(operator*()); }
  };

  template<PhylogenyType _Network,
           PhylogenyType Component = _Network,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           DataExtracterType   _Extracter = DataExtracter<_Network>>
  using BiconnectedComponents = mstd::IterFactory<BiconnectedComponentIter<_Network, Component, OldToNewTranslation, _Extracter>>;

  // deduce parameters from arguments
  // NOTE: if you pass a PhylogenyType as first argument, be sure that it's the same as Network since, otherwise, a temporary copy will be made :(
  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           class CutsInit,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class... ExtracterArgs>
             requires (!PhylogenyType<CutsInit> || std::is_same_v<std::remove_cvref_t<CutsInit>,std::remove_cvref_t<Network>>)
  auto get_biconnected_components(CutsInit&& cuts, OldToNewTranslation&& old_to_new = OldToNewTranslation(), ExtracterArgs&&... ex_args) {
    using Extracter = decltype(make_data_extracter<Network, Component>(std::forward<ExtracterArgs>(ex_args)...));
    return BiconnectedComponents<Network, Component, OldToNewTranslation, Extracter>(
        std::forward<CutsInit>(cuts),
        std::forward<OldToNewTranslation>(old_to_new),
        std::forward<ExtracterArgs>(ex_args)...);
  }

  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           class CutsInit,
           class First,
           class... ExtracterArgs>
             requires ((!PhylogenyType<CutsInit> || std::is_same_v<std::remove_cvref_t<CutsInit>,std::remove_cvref_t<Network>>) &&
                 !NodeTranslationType<First>)
  auto get_biconnected_components(CutsInit&& cuts, First&& first, ExtracterArgs&&... ex_args) {
    using Extracter = decltype(make_data_extracter<Network, Component>(std::forward<First>(first), std::forward<ExtracterArgs>(ex_args)...));
    return BiconnectedComponents<Network, Component, NodeTranslation, Extracter>(
        std::forward<CutsInit>(cuts),
        NodeTranslation(),
        std::forward<First>(first),
        std::forward<ExtracterArgs>(ex_args)...);
  }
*/

}// namespace
