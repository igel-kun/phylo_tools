
// this iterates over biconnected components in post-order

#pragma once

namespace PT{

  // NOTE: the iterator is not publicly constructible, but only by the factory below
  // NOTE: _Component has to be compatible wo _Network (uses the same LabelMap type)
  // NOTE: if you insist on using an RONetwork as _Component, make sure that you translate nodes using the old_to_new NodeTranslation
  template<class _Network, class _Component = CompatibleRWNetwork<_Network>, bool enumerate_trivial = true,
    class = std::enable_if_t<are_compatible_v<_Network, _Component>>>
  class BiconnectedComponentIter
  {
  public:
    using Edge = typename _Network::Edge;
    using EdgeVec = std::vector<Edge>;
    using BridgeIter = typename std::vector<Edge>::const_iterator;
    using reference = _Component;
    using const_reference = _Component;

  protected:
    const _Network& N;          // the network
    const EdgeVec& bridges;     // bridge-container
    bool is_end_iter;           // indicates whether we reached the end
    BridgeIter current_bridge;  // iterator into the bridge-container
    NodeSet seen;               // set of seen nodes
    EdgeVec current_edges;  // the current component to be output on operator*
    bool bridge_next = false;   // at alternating output, indicate whether to output the next bridge or its biconnected component

    void next_component()
    {
      if(!is_end_iter){
        current_edges.clear();
        // by presenting the bridges in post-order, we can be sure that no bridge is below the current bridge, so we only split off leaf-components
        if(current_bridge != bridges.end()){
          const Edge& uv = *current_bridge;
          DEBUG4(std::cout << "found bridge "<<uv<<std::endl);
          //NOTE: any lonely bridge is itself a biconnected component, so after outputting everything below uv, we'll have to also output uv itself
          //      this behavior can be controled via the "enumerate_trivial" flag
          if(bridge_next){
            // if we are to output a bridge next, then let current_edges be that bridge and get on with it
            append(current_edges, uv);
            bridge_next = false;
            ++current_bridge;
          } else {
            // if we are to output the component below uv, then get "all edges below v, avoiding nodes that have been seen" from N
            const Node v = uv.head();

            std::cout << "collecting edges of BCC below "<<v<<", seen = "<<seen<<"\n";
            current_edges.clear();
            auto my_dfs = N.all_edges_dfs_except(seen);
            auto traversal = my_dfs.postorder(v);
            for(auto it = traversal.begin(); it != traversal.end(); ++it) append(current_edges, *it);

            append(seen, v);

            if(current_edges.empty()){
              ++current_bridge;
              if(enumerate_trivial)
                append(current_edges, uv);
              else next_component();
            } else {
              if(enumerate_trivial)
                bridge_next = true;
              else ++current_bridge;
            }
          }
        } else { // current_bridge == bridges.end()
          // we indicate that we have seen the root component by setting bridge_next = true,
          // this is OK since bridge_next can otherwise only be true if current_bridge != bridges.end()
          if(!bridge_next){
            std::cout << "collecting edges of BCC below the root, seen = "<<seen<<"\n";
            current_edges.clear();
            auto my_dfs = N.all_edges_dfs_except(seen);
            auto traversal = my_dfs.postorder();
            for(auto it = traversal.begin(); it != traversal.end(); ++it) append(current_edges, *it);
            //current_edges = EdgeVec(N.edge_dfs_except(seen).postorder());


            DEBUG4(std::cout << "root component (below "<<N.root()<<"): "<<current_edges<<"\n");
            bridge_next = true;
          } else is_end_iter = true;
        }
      }
    }

    // NOTE: bridges MUST be in post-order!
    BiconnectedComponentIter(const _Network& _N, const std::vector<Edge>& _bridges, const bool _construct_end_iterator = false):
      N(_N), bridges(_bridges), is_end_iter(_construct_end_iterator), current_bridge(bridges.begin())
    {
      if(!_construct_end_iterator){
        DEBUG4(std::cout << "iterating BCCs of \n"<<N<<"\n with bridges "<<bridges<<" ("<<bridges.size()<<")\n");
        next_component();
      }
    }

  public:
    // NOTE: calling operator* is expensive, consider calling it at most once for each item
    // NOTE: we can't be sure that the vertices of the component are consecutive, so if the user requested consecutive output networks, we need to translate
    reference operator*() const {
      return _Component(non_consecutive_tag, current_edges, N.labels());
    }
    BiconnectedComponentIter& operator++() { next_component(); return *this; }

    bool operator==(const BiconnectedComponentIter& _it) const
    {
      if(!_it.is_end_iter){
        if(is_end_iter) return false;
        // at this point, both are non-end iterators, so do a comparison
        return (current_bridge == _it.current_bridge);
      } else return is_end_iter;
    }
    bool operator!=(const BiconnectedComponentIter& _it) const { return !operator==(_it); }

    template<class, class, bool, class>
    friend class BiconnectedComponents;
  };

  // factory for biconnected components, see notes for BiconnectedComponentIter
  template<class _Network, class _Component = CompatibleRWNetwork<_Network>, bool enumerate_trivial = true,
    class = std::enable_if_t<are_compatible_v<_Network, _Component>>>
  class BiconnectedComponents
  {
  public:
    using Component = _Component;
    using Edge = typename _Network::Edge;
    using EdgeVec = std::vector<Edge>;
    using BridgeIter = typename std::vector<Edge>::const_iterator;
    using reference = _Component;
    using const_reference = _Component;
    using iterator = BiconnectedComponentIter<_Network, _Component, enumerate_trivial>;
    using const_iterator = BiconnectedComponentIter<_Network, _Component, enumerate_trivial>;

  protected:
    const _Network& N;
    const std::vector<Edge> bridges;

  public:

    BiconnectedComponents(const _Network& _N):
      N(_N), bridges(N.get_bridges_postorder())
    {}
    
    // NOTE: if you use this, make sure the bridges are in postorder!
    BiconnectedComponents(const _Network& _N, const std::vector<Edge>& _bridges_postorder):
      N(_N), bridges(_bridges_postorder)
    {}

    const_iterator begin() const { return const_iterator(N, bridges); }
    const_iterator end() const { return const_iterator(N, bridges, true); }
  };


}// namespace
