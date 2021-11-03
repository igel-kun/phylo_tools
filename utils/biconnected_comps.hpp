
// this iterates over biconnected components in post-order

#pragma once

#include "types.hpp"
#include "bridges.hpp"

namespace PT{

  // NOTE: only enumeration of vertical components of a **single-rooted** network is supported for now
  // NOTE: make sure that the cut nodes passed to the constructor are in bottom-up order!
  template<PhylogenyType _Network,
           PhylogenyType Component = _Network,
           NodeIterableType CutNodeContainer = const NodeVec,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class CreateNodeData = typename Component::IgnoreNodeDataFunc,
           class CreateEdgeData = typename Component::IgnoreEdgeDataFunc>
  class BiconnectedComponentIter {
  public:
    using Network = _Network;
    using Node = typename Network::Node;
    using Edge = typename Network::Edge;
    using ChildContainer = typename Network::ChildContainer;
    using CutNodeIter = std::iterator_of_t<CutNodeContainer>;
    using OneEdgeArray = const Edge[1];
    using reference = Component&;
    using const_reference = const Component&;
  protected:

    NodeDesc root;
    NodeSet seen;               // set of seen nodes
    Component output;          // the current component to be output on operator*
    OldToNewTranslation old_to_new;
    std::pair<CreateNodeData, CreateEdgeData> make_data;
    std::auto_iter<CutNodeContainer> current_cut_node;
    std::auto_iter<ChildContainer>   current_children;
    EdgeEmplacer<false, Component, OldToNewTranslation&, CreateNodeData&> output_emplacer;

    // construct a vertical biconnected component containing the arc uv and store it in 'output'
    //NOTE: this will mark all nodes of that component in 'seen'
    //NOTE: remember to set the root of the output component after calling this!
    void make_component_along(const NodeDesc& v) {
      if(!test(seen, v)) {
        std::cout << "making component along "<<v<<'\n';
        const Node& v_node = node_of<Node>(v);
        for(const NodeDesc& w: v_node.children()) make_component_along(w);
        for(const NodeDesc& u: v_node.parents()) output_emplacer.emplace_edge(u, v, make_data.second(u,v));
        append(seen, v);
      }
    }

    void next_component() {
#warning "this is wrong if the cut node is the root of one component but in the middle of another component!"
      if(is_valid()) {
        output.clear();
        output_emplacer.clear();
        old_to_new.clear();

        std::cout << "getting next biconnected component...\n";
        // step1: advance the current-children iterator (also skipping seen children)
        while(current_children.is_valid() && test(seen, *current_children)) ++current_children;
        while(!current_children.is_valid()) {
          std::cout << "we scanned all children of the current cut node " << *current_cut_node << "\n";
          // if we're now at the end of the current cut-node's children, go to the next cut-node
          // NOTE: once all cut-nodes have been processed, we will process the root, so check for this special case
          if(current_cut_node.is_valid()) {
            assert(*current_cut_node != root);
            ++current_cut_node;
            if(current_cut_node.is_valid()) {
              std::cout << "advanced current cut node, now at "<< *current_cut_node <<"\n";
              current_children = children_of<Node>(*current_cut_node);
              // if the root is a cut-node, then advance to the next (invalid) cut-node in order to avoid processing the root twice
              if(*current_cut_node == root) ++current_cut_node;
            } else {
              std::cout << "advanced current cut node, now at root "<< root << "\n";
              current_children = children_of<Node>(root); // if we reached the end of the cut-node container, treat the root next
            }
          } else return; // if the current cut-node iterator is invalid, then we just treated the root and are now done!
          // skip over all seen children
          while(current_children.is_valid() && test(seen, *current_children)) ++current_children;
        }
        // step2: construct the component along the current child
        // NOTE: we either skipped all seen children or we just arrived here; in either case, the current child cannot have been seen
        assert(!test(seen, *current_children));
        make_component_along(*current_children);
        output_emplacer.finalize( current_cut_node.is_valid() ? *current_cut_node : root );
        std::cout << "--- done constructing component---\n";
      }
    }

  public:
    // NOTE: cut-nodes MUST be bottom-up (that is, no cut-node x can preceed any descendant of x)!
    template<class CutNodeInit, NodeTranslationType _OldToNewTranslation, class... Args>
    BiconnectedComponentIter(const NodeDesc& _root, _OldToNewTranslation&& _old_to_new, CutNodeInit&& _cut_nodes, Args&&... args):
      root(_root),
      old_to_new(std::forward<_OldToNewTranslation>(_old_to_new)),
      make_data(std::forward<Args>(args)...),
      current_cut_node(std::forward<CutNodeInit>(_cut_nodes)),
      output_emplacer{{output}, old_to_new, make_data.first}
    {
      if(root != NoNode) {
        DEBUG4(std::cout << "iterating BCCs with root "<<root<<"\n");
        if(current_cut_node.is_valid()) {
          current_children = children_of<Node>(*current_cut_node);
        } else current_children = children_of<Node>(root);
        next_component();
      } // if _root == NoNode, then this is the end-iterator so don't do anything here
    }

    BiconnectedComponentIter& operator++() { next_component(); return *this; }

    // NOTE: calling operator* is expensive, consider calling it at most once for each item
    reference operator*() { return output; }
    const_reference operator*() const { return output; }

    bool operator==(const std::GenericEndIterator&) const { return is_end_iter(); }
    template<class Other>
    bool operator==(const Other& _it) const {
      if(!_it.is_end_iter()){
        if(is_end_iter()) return false;
        // at this point, both are non-end iterators, so do a comparison
        return current_children == _it.current_children;
      } else return is_end_iter();
    }
    template<class Other>
    bool operator!=(const Other& _it) const { return !operator==(_it); }
    
    bool is_end_iter() const { return !is_valid(); }
    bool is_valid() const { return current_children.is_valid(); }

    OldToNewTranslation& get_translation() { return old_to_new; }
    const OldToNewTranslation& get_translation() const { return old_to_new; }

    template<PhylogenyType, PhylogenyType, NodeIterableType, NodeTranslationType, class, class>
    friend class BiconnectedComponents;

    template<PhylogenyType, PhylogenyType, NodeIterableType, NodeTranslationType, class, class>
    friend class BiconnectedComponentIter;
  };


  // factory for biconnected components, see notes for BiconnectedComponentIter
  template<PhylogenyType _Network,
           PhylogenyType Component = _Network,
           NodeIterableType CutNodeContainer = const NodeVec,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class CreateNodeData = typename Component::IgnoreNodeDataFunc,
           class CreateEdgeData = typename Component::IgnoreEdgeDataFunc>
  class BiconnectedComponents {
  public:
    using Network = _Network;
    using Edge = typename _Network::Edge;
    using CutNodeIter = std::iterator_of_t<CutNodeContainer>;
    using reference = Component;
    using const_reference = Component;
    using iterator = BiconnectedComponentIter<_Network, Component, CutNodeContainer, OldToNewTranslation, CreateNodeData, CreateEdgeData>;
    using const_iterator = iterator;

  protected:
    static constexpr bool custom_node_data_maker = !std::is_same_v<CreateNodeData, typename Component::IgnoreNodeDataFunc>;
    static constexpr bool custom_edge_data_maker = !std::is_same_v<CreateEdgeData, typename Component::IgnoreEdgeDataFunc>;

    NodeDesc root;
    CutNodeContainer cut_nodes;
    OldToNewTranslation old_to_new;
    std::pair<CreateNodeData, CreateEdgeData> make_data;

  public:

    template<NodeIterableType CutNodeInit, class TranslateInit, class First, class Second, class... Args>
    BiconnectedComponents(CutNodeInit&& _cut_nodes, const NodeDesc& _root, TranslateInit&& trans_init, First&& first, Second&& second, Args&&... args):
      root(_root),
      cut_nodes(std::forward<CutNodeInit>(_cut_nodes)),
      old_to_new(std::forward<TranslateInit>(trans_init)),
      make_data(std::forward<First>(first), std::forward<Second>(second), std::forward<Args>(args)...)
    {}

    // if only 1 argument is passed to initialize the data makers, it's passed to the one that is not ignorant
    template<NodeIterableType CutNodeInit, class TranslateInit, class First>
      requires (custom_node_data_maker && !custom_edge_data_maker)
    BiconnectedComponents(CutNodeInit&& _cut_nodes, const NodeDesc& _root, TranslateInit&& trans_init, First&& first):
      root(_root),
      cut_nodes(std::forward<CutNodeInit>(_cut_nodes)),
      old_to_new(std::forward<TranslateInit>(trans_init)),
      make_data(std::piecewise_construct, std::forward_as_tuple(std::forward<First>(first)), std::forward_as_tuple())
    {}
    template<NodeIterableType CutNodeInit, class TranslateInit, class First>
      requires (!custom_node_data_maker && custom_edge_data_maker)
    BiconnectedComponents(CutNodeInit&& _cut_nodes, const NodeDesc& _root, TranslateInit&& trans_init, First&& first):
      root(_root),
      cut_nodes(std::forward<CutNodeInit>(_cut_nodes)),
      old_to_new(std::forward<TranslateInit>(trans_init)),
      make_data(std::piecewise_construct, std::forward_as_tuple(), std::forward_as_tuple(std::forward<First>(first)))
    {}


    template<class... Args>
    BiconnectedComponents(const Network& N, const NodeDesc& _root, Args&&... args):
      BiconnectedComponents(get_cut_nodes(N, _root), _root, std::forward<Args>(args)...)
    {}

    template<class CutNodeInitTuple, class NodeDataInitTuple, class EdgeDataInitTuple>
    BiconnectedComponents(const std::piecewise_construct_t,
                          CutNodeInitTuple&& cn_init,
                          const NodeDesc& _root,
                          NodeDataInitTuple&& nd_init,
                          EdgeDataInitTuple&& ed_init):
      root(_root),
      cut_nodes(std::make_from_tuple<CutNodeContainer>(std::forward<CutNodeInitTuple>(cn_init))),
      make_data(std::piecewise_construct, std::forward<NodeDataInitTuple>(nd_init), std::forward<EdgeDataInitTuple>(ed_init))
    {}

    iterator begin() const & { return iterator(root, old_to_new, cut_nodes, make_data); }
    iterator begin() & { return iterator(root, old_to_new, cut_nodes, make_data); }
    iterator begin() && { return iterator(root, std::move(old_to_new), std::move(cut_nodes), std::move(make_data)); }

    static auto end() { return std::GenericEndIterator(); }
  };



  // deduce parameters from arguments
  template<PhylogenyType Component,
           PhylogenyType Network = Component,
           NodeIterableType CutNodes,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class CreateNodeData = typename Component::IgnoreNodeDataFunc,
           class CreateEdgeData = typename Component::IgnoreEdgeDataFunc>
  auto get_biconnected_components(CutNodes&& cut_nodes,
                                  const NodeDesc& root,
                                  OldToNewTranslation&& old_to_new = OldToNewTranslation(),
                                  CreateNodeData&& nd = CreateNodeData(),
                                  CreateEdgeData&& ed = CreateEdgeData())
  {
    return BiconnectedComponents<Network, Component, CutNodes, OldToNewTranslation, CreateNodeData, CreateEdgeData>(
        std::forward<CutNodes>(cut_nodes),
        root,
        std::forward<OldToNewTranslation>(old_to_new),
        std::forward<CreateNodeData>(nd),
        std::forward<CreateEdgeData>(ed));
  }

  template<PhylogenyType Component,
           PhylogenyType Network,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class CreateNodeData = typename Component::IgnoreNodeDataFunc,
           class CreateEdgeData = typename Component::IgnoreEdgeDataFunc>
  auto get_biconnected_components(const NodeDesc& root,
                                  OldToNewTranslation&& old_to_new = OldToNewTranslation(),
                                  CreateNodeData&& nd = CreateNodeData(),
                                  CreateEdgeData&& ed = CreateEdgeData())
  {
      return get_biconnected_components<Component, Network>(
          get_cut_nodes<Network>(root),
          root,
          std::forward<OldToNewTranslation>(old_to_new),
          std::forward<CreateNodeData>(nd),
          std::forward<CreateEdgeData>(ed));
  }

  template<PhylogenyType Component,
           PhylogenyType Network,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class CreateNodeData = typename Component::IgnoreNodeDataFunc,
           class CreateEdgeData = typename Component::IgnoreEdgeDataFunc>
  auto get_biconnected_components(const Network& N,
                                  OldToNewTranslation&& old_to_new = OldToNewTranslation(),
                                  CreateNodeData&& nd = CreateNodeData(),
                                  CreateEdgeData&& ed = CreateEdgeData())
  {
      return get_biconnected_components<Component, Network>(
          get_cut_nodes(N),
          N.root(),
          std::forward<OldToNewTranslation>(old_to_new),
          std::forward<CreateNodeData>(nd),
          std::forward<CreateEdgeData>(ed));
  }

}// namespace
