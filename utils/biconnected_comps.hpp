
// this iterates over biconnected components in post-order

#pragma once

#include "union_find.hpp"

#include "types.hpp"
#include "cuts.hpp"

namespace PT{

  // NOTE: only enumeration of vertical components of a **single-rooted** network is supported for now
  // NOTE: ExtractNodeData::operator() must take a NodeDesc and return something that is passed to Component::NodeData()
  //       ExtractEdgeData::operator() must take an Adjacency& and a const NodeDesc and return something that is passed to Component::EdgeData()
  template<StrictPhylogenyType _Network,
           StrictPhylogenyType Component = _Network,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           NodeFunctionType ExtractNodeData  = ExtractData<_Network, Ex_node_data>,
           class ExtractEdgeData             = ExtractData<_Network, Ex_edge_data>,
           NodeFunctionType ExtractNodeLabel = ExtractData<_Network, Ex_node_label>>
  class BiconnectedComponentIter: public CutIter<_Network, CO_BCC, std::iter_traits_from_reference<NodeDesc>> {
    using Parent = CutIter<_Network, CO_BCC, std::iter_traits_from_reference<NodeDesc>>;
    using Traits = std::iter_traits_from_reference<Component&>;
  public:
    using Network = _Network;
    using Node = typename Network::Node;

    using value_type = typename Traits::value_type;
    using reference = typename Traits::reference;
    using const_reference = typename Traits::const_reference;
    using pointer = typename Traits::pointer;
    using const_pointer = typename Traits::const_pointer;
    using Emplacer = EdgeEmplacer<false, Component, OldToNewTranslation&, ExtractNodeData&, ExtractNodeLabel&>;


    using Parent::is_valid;
  protected:
    using Parent::node_infos;
    // NOTE: for each cut node, we will add each of its child nodes into a disjoint set forest
    //       and then merge children v & w when they are in the same biconnected component
    // NOTE: we will use the CutNodeIter's interface to say when two siblings are in the same component
    std::auto_iter<NodeVec> child_comp_iter;
    NodeSet seen;

    // storing a pointer to the output allows us to not compute the actual component up until the point where operator*() is called
    std::unique_ptr<Component> output = nullptr; // the current component to be output on operator*
    OldToNewTranslation old_to_new;
    Extracter<Component, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel> data_extracter;

    NodeDesc get_current_cut_node() const { assert(is_valid()); return Parent::operator*(); }

    // construct a vertical biconnected component containing the arc uv and store it in 'output'
    //NOTE: remember to set the root of the output component after calling this!
    void make_component_along(const NodeDesc rt, const NodeDesc v, Emplacer& output_emplacer) {
      if(append(seen, v).second){
        DEBUG4(std::cout << "BCC: making component along " << v <<'\n');
        Node& v_node = node_of<Network>(v); // NOTE: make_data.second may want to change the edge-data of the v_node, so we cannot pass it as const
        for(auto uv: v_node.in_edges()) output_emplacer.emplace_edge(uv, data_extracter.get_edge_data(uv));
        for(const NodeDesc u: v_node.parents()) 
          if(u != rt)
            make_component_along(rt, u, output_emplacer);
        if(Parent::is_cut_node(v)) {
          // if v is a cut-node, then not all children w of v are in the same BCC as rt (only those seeing an outside neighbor wrt. v)
          const auto& v_info = node_infos.at(v);
          for(const NodeDesc w: v_node.children())
            if(v_info.child_has_outside_neighbor(node_infos.at(w)))
              make_component_along(rt, w, output_emplacer);
        } else
          for(const NodeDesc w: v_node.children())
            make_component_along(rt, w, output_emplacer);
      }
    }

    void make_component_along(const NodeDesc v) {
      output = std::make_unique<Component>();
      Emplacer output_emplacer{*output, old_to_new, {data_extracter.get_node_data, data_extracter.get_node_label}};
      const NodeDesc u = get_current_cut_node();
      make_component_along(u, v, output_emplacer);
      output_emplacer.mark_root(u);
    }

    // this is called when we're through with the current cut-node, in order to
    // 1. advance to the next cut node
    // 2. set up the queue of children of u that are in BCCs
    // 3. initialize current_child
    void advance_parent() {
      assert(is_valid());
      if(!child_comp_iter) {
        // step 1: advance the cut-node iterator
        Parent::operator++();
        if(is_valid()) 
          compute_new_child_comps();
        else DEBUG3(std::cout << "BCC: that's it, no more BCCs\n");
      }
    }

    void next_component() {
      if(is_valid()) {
        output.reset();
        old_to_new.clear();
        ++child_comp_iter;

        DEBUG4(std::cout << "BCC: getting next biconnected component...\n");
        // step1: advance the current-children iterator (also skipping cut-nodes)
        advance_parent();
      }
    }

    void compute_new_child_comps() {
      const NodeDesc u = get_current_cut_node();
      DEBUG4(std::cout << "BCC: cut node "<< u <<" with child stack "<<node_infos.at(u).cut_children<<"\n");
      child_comp_iter = node_infos.at(u).cut_children;
      assert(child_comp_iter.is_valid());
    }

  public:
    // NOTE: cut-nodes MUST be bottom-up (that is, no cut-node x can preceed any descendant of x)!
    template<class ParentInit, NodeTranslationType _OldToNewTranslation = NodeTranslation, class... Args>
    BiconnectedComponentIter(ParentInit&& _parent, _OldToNewTranslation&& _old_to_new, Args&&... args):
      Parent(std::forward<ParentInit>(_parent)),
      old_to_new(std::forward<_OldToNewTranslation>(_old_to_new)),
      data_extracter(std::forward<Args>(args)...)
    {
      if(Parent::is_valid())
        compute_new_child_comps();
    }


    // NOTE: the copy constructor does NOT copy the output Component
    BiconnectedComponentIter(const BiconnectedComponentIter& other):
      Parent(other),
      child_comp_iter(other.child_comp_iter),
      seen(other.seen),
      data_extracter(other.data_extracter)
    {}
    BiconnectedComponentIter(BiconnectedComponentIter&& other) = default;

    BiconnectedComponentIter& operator=(const BiconnectedComponentIter& other) {
      if(this != &other)
        operator=(BiconnectedComponentIter(other));
      return *this;
    }
    BiconnectedComponentIter& operator=(BiconnectedComponentIter&& other) = default;

    BiconnectedComponentIter& operator++() { next_component(); return *this; }
    BiconnectedComponentIter& operator++(int) = delete;

    bool operator!=(const BiconnectedComponentIter& other) const {
      if(!is_valid()) return other.is_valid();
      if(!other.is_valid()) return true;
      return child_comp_iter != other.child_comp_iter;
    }
    bool operator==(const BiconnectedComponentIter& other) const { return !operator!=(other); }

    // NOTE: calling operator* is expensive, consider calling it at most once for each item
    reference operator*() {
      assert(is_valid());
      if(!output) make_component_along(*child_comp_iter);  // step2: construct the component along the current child
      return *output;
    }
    const_reference operator*() const {
      assert(is_valid());
      if(!output) make_component_along(*child_comp_iter);  // step2: construct the component along the current child
      return *output;
    }
    pointer operator->() { return &(operator*()); }
    const_pointer operator->() const { return &(operator*()); }

    OldToNewTranslation& get_translation() { return old_to_new; }
    const OldToNewTranslation& get_translation() const { return old_to_new; }
  };


  /*
  // factory for biconnected components, see notes for BiconnectedComponentIter
  template<PhylogenyType _Network,
           PhylogenyType Component = _Network,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class ExtractNodeData = typename Component::IgnoreNodeDataFunc,
           class ExtractEdgeData = typename Component::IgnoreEdgeDataFunc>
  class BiconnectedComponents {
  public:
    using Network = _Network;
    using Edge = typename _Network::Edge;
    using VcnContainer = CutNodeIterFactory<_Network>;
    using VcnIter = std::iterator_of_t<VcnContainer>;
    using reference = Component;
    using const_reference = Component;
    using iterator = BiconnectedComponentIter<_Network, Component, OldToNewTranslation, ExtractNodeData, ExtractEdgeData>;
    using const_iterator = iterator;

  protected:
    static constexpr bool custom_node_data_maker = !std::is_same_v<ExtractNodeData, typename Component::IgnoreNodeDataFunc>;
    static constexpr bool custom_edge_data_maker = !std::is_same_v<ExtractEdgeData, typename Component::IgnoreEdgeDataFunc>;

    VcnContainer vertical_cut_nodes;
    OldToNewTranslation old_to_new;
    std::pair<ExtractNodeData, ExtractEdgeData> make_data;

  public:

    // if at least 2 arguments are passed to initialize the data makers, we forward these arguments
    template<std::IterableType VcnInit, class TranslateInit, class First, class Second, class... Args>
    BiconnectedComponents(VcnInit&& _vertical_cut_nodes, TranslateInit&& trans_init, First&& first, Second&& second, Args&&... args):
      vertical_cut_nodes(std::forward<VcnInit>(_vertical_cut_nodes)),
      old_to_new(std::forward<TranslateInit>(trans_init)),
      make_data(std::forward<First>(first), std::forward<Second>(second), std::forward<Args>(args)...)
    {}

    // if only 1 argument is passed to initialize the data makers, it's passed to the one that is not ignorant
    template<std::IterableType VcnInit, class TranslateInit, class First> requires (custom_node_data_maker && !custom_edge_data_maker)
    BiconnectedComponents(VcnInit&& _vertical_cut_nodes, TranslateInit&& trans_init, First&& first):
      vertical_cut_nodes(std::forward<VcnInit>(_vertical_cut_nodes)),
      old_to_new(std::forward<TranslateInit>(trans_init)),
      make_data(std::piecewise_construct, std::forward_as_tuple(std::forward<First>(first)), std::forward_as_tuple())
    {}

    template<std::IterableType VcnInit, class TranslateInit, class First> requires (!custom_node_data_maker && custom_edge_data_maker)
    BiconnectedComponents(VcnInit&& _vertical_cut_nodes, TranslateInit&& trans_init, First&& first):
      vertical_cut_nodes(std::forward<VcnInit>(_vertical_cut_nodes)),
      old_to_new(std::forward<TranslateInit>(trans_init)),
      make_data(std::piecewise_construct, std::forward_as_tuple(), std::forward_as_tuple(std::forward<First>(first)))
    {}

    template<class... Args>
    BiconnectedComponents(const NodeDesc _root, Args&&... args):
      BiconnectedComponents(get_cut_nodes<Network>(_root), std::forward<Args>(args)...)
    {}

    template<class VcnInitTuple, class TranslateInitTuple, class NodeDataInitTuple, class EdgeDataInitTuple>
    BiconnectedComponents(const std::piecewise_construct_t,
                          VcnInitTuple&& cn_init,
                          TranslateInitTuple&& tr_init,
                          NodeDataInitTuple&& nd_init,
                          EdgeDataInitTuple&& ed_init):
      vertical_cut_nodes(std::make_from_tuple<VcnIter>(std::forward<VcnInitTuple>(cn_init))),
      old_to_new(std::make_from_tuple<OldToNewTranslation>(std::forward<TranslateInitTuple>(tr_init))),
      make_data(std::piecewise_construct, std::forward<NodeDataInitTuple>(nd_init), std::forward<EdgeDataInitTuple>(ed_init))
    {}

    iterator begin() const & { return iterator(old_to_new, vertical_cut_nodes, make_data); }
    iterator begin() & { return iterator(old_to_new, vertical_cut_nodes, make_data); }
    iterator begin() && { return iterator(std::move(old_to_new), std::move(vertical_cut_nodes), std::move(make_data)); }

    static auto end() { return std::GenericEndIterator(); }
  };
  */


  template<PhylogenyType _Network,
           PhylogenyType Component = _Network,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           NodeFunctionType ExtractNodeData = ExtractData<Component, Ex_node_data>,
           class ExtractEdgeData = ExtractData<Component, Ex_edge_data>,
           NodeFunctionType ExtractNodeLabel = ExtractData<Component, Ex_node_label>>
  using BiconnectedComponents = std::IterFactory<
          BiconnectedComponentIter<_Network, Component, OldToNewTranslation, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>
        >;

  // deduce parameters from arguments
  // NOTE: if you pass a PhylogenyType as first argument, be sure that it's the same as Network as, otherwise, a temporary copy will be made :(
  template<StrictPhylogenyType Network,
           StrictPhylogenyType Component = Network,
           class CutsInit,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           NodeFunctionType ExtractNodeData = ExtractData<Component, Ex_node_data>,
           class ExtractEdgeData = ExtractData<Component, Ex_edge_data>,
           NodeFunctionType ExtractNodeLabel = ExtractData<Component, Ex_node_label>>
             requires (!PhylogenyType<CutsInit> || std::is_same_v<std::remove_cvref_t<CutsInit>,std::remove_cvref_t<Network>>)
  auto get_biconnected_components(CutsInit&& cuts,
                                  OldToNewTranslation&& old_to_new = OldToNewTranslation(),
                                  ExtractNodeData&& nd = ExtractNodeData(),
                                  ExtractEdgeData&& ed = ExtractEdgeData(),
                                  ExtractNodeLabel&& nl = ExtractNodeLabel())
  {
    return BiconnectedComponents<Network, Component, OldToNewTranslation, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>(
        std::forward<CutsInit>(cuts),
        std::forward<OldToNewTranslation>(old_to_new),
        std::forward<ExtractNodeData>(nd),
        std::forward<ExtractEdgeData>(ed),
        std::forward<ExtractNodeLabel>(nl));
  }

}// namespace
