
// this iterates over biconnected components in post-order

#pragma once

namespace PT{

  template<class _Network, class _Component = _Network, bool enumerate_trivial = true>
  class BiconnectedComponentIter
  {
  public:
    using Edge = typename _Network::Edge;
    using EdgeVec = std::vector<Edge>;
    using BridgeIter = typename std::vector<Edge>::const_iterator;
    using reference = _Component;
    using const_reference = _Component;

  protected:
    const _Network& N;              // the network
    const std::vector<Edge>& bridges;// bridge-container
    bool is_end_iter;               // indicates whether we reached the end
    BridgeIter current_bridge;      // iterator into the bridge-container
    NodeSet seen;  // set of seen nodes
    EdgeVec current_component;      // the current component to be output on operator*
    bool bridge_next = false;       // at alternating output, indicate whether to output the next bridge or its biconnected component

    void next_component()
    {
      if(!is_end_iter){
        if(current_bridge != bridges.end()){
          const Edge& uv = *current_bridge;
          DEBUG4(std::cout << "found bridge "<<uv<<std::endl);
          // by presenting the bridges in post-order, we can be sure that no bridge is below the current bridge, so we only split off leaf-components
          if(bridge_next){
            current_component = {uv};
            bridge_next = false;
            ++current_bridge;
          } else {
            // if v is a leaf, just add it to the extension; otherwise recurse to add a cheapest sub-extension
            const Node v = uv.head(); 
            current_component = N.get_edges_below_except(seen, v);
            append(seen, v);
            if(current_component.empty()){
              ++current_bridge;
              if(!enumerate_trivial)
                next_component();
              else
                append(current_component, uv);
            } else {
              if(!enumerate_trivial)
                ++current_bridge;
              else
                bridge_next = true;
            }
          }
        } else {
          // we indicate that we have seen the root component by setting bridge_next = true,
          // this is OK since bridge_next can otherwise only be true if current_bridge != bridges.end()
          if(!bridge_next){
            current_component = N.get_edges_below_except(seen, N.root());
            DEBUG4(std::cout << "root component (below "<<N.root()<<"): "<<current_component<<"\n");
            bridge_next = true;
          } else is_end_iter = true;
        }
      }
    }

  public:
    BiconnectedComponentIter(const _Network& _N, const std::vector<Edge>& _bridges, const bool _construct_end_iterator = false):
      N(_N), bridges(_bridges), is_end_iter(_construct_end_iterator), current_bridge(bridges.begin())
    {
      std::cout << "computing BCCs of \n"<<N<<"\n with bridges "<<bridges<<" ("<<bridges.size()<<")\n";
      next_component();
    }
    // NOTE: calling operator* is expensive, consider calling it at most once for each item
    reference operator*() const { return _Component(current_component, N.labels()); }
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
  };

  // factory for biconnected components
  template<class _Network, class _Component = _Network, bool enumerate_trivial = true>
  class BiconnectedComponents
  {
  public:
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
