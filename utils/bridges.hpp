
#pragma once

// this is my own bridge finder
// NOTE: most bridge finders out there work only on undirected graphs, so I modified Tarjan's ideas slightly...

namespace PT{

  template<class Node>
  struct BridgeInfo{
    Node num_descendants;
    Node disc_time;
    Node lowest_neighbor;
    Node highest_neighbor;

    BridgeInfo(const Node _disc_time):
      num_descendants(1), disc_time(_disc_time), lowest_neighbor(_disc_time), highest_neighbor(_disc_time)
    {}
 
    // the DFS interval of v is disc_time + [0:num_descendants]
    // uv is a bridge <=> noone in the sub-DFS-tree rooted at v has seen a neighbor outside this interval   
    inline const bool is_bridge_head() const
    {
      return (lowest_neighbor >= disc_time) && (highest_neighbor < disc_time + num_descendants);
    }
    inline void update_lowest_neighbor(const Node u)
    {
      lowest_neighbor = std::min(lowest_neighbor, u);
    }
    inline void update_highest_neighbor(const Node u)
    {
      highest_neighbor = std::max(highest_neighbor, u);
    }

  };

  template<class Node>
  std::ostream& operator<<(std::ostream& os, const BridgeInfo<Node>& b)
  {
    return os << "(disc: "<<b.disc_time<<" desc: "<<b.num_descendants<<" low: "<<b.lowest_neighbor<<" high: "<<b.highest_neighbor<<")";
  }

  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
  class BridgeFinder
  {
  public:
    using Node = typename _Network::Node;

  protected:
    const _Network& N;
    _Container& out;
    Node time;
    
    std::unordered_map<Node, BridgeInfo<Node>> node_infos;

    // the initial DFS sets up preorder numbers and numbers of descendants in the DFS-tree
    const BridgeInfo<Node>& initial_DFS(const Node u)
    {
      auto emp_res = node_infos.emplace(u, time);
      if(emp_res.second){
        ++time;
        BridgeInfo<Node>& u_info = emp_res.first->second;
        for(const auto v: N.children(u)){
          const BridgeInfo<Node>& v_info = initial_DFS(v);
          u_info.num_descendants += v_info.num_descendants;
        }
        return u_info;
      } else return emp_res.first->second;
    }

    const BridgeInfo<Node>& bridge_collector_DFS(const Node u)
    {
      return bridge_collector_DFS(u, u, 0);
    }
    // NOTE: this will return the bridges in post-order!
    const BridgeInfo<Node>& bridge_collector_DFS(const Node u, const Node v, const Node u_time)
    {
      BridgeInfo<Node>& v_info = node_infos.at(v);
      if(v_info.disc_time >= u_time){
        for(const auto w: N.children(v)){
          const BridgeInfo<Node>& w_info = bridge_collector_DFS(v, w, v_info.disc_time);
          // register bridge if need be
          if(!w_info.is_bridge_head()){
            // update u_info if uv is not a bridge (if it is, no update is necessary)
            v_info.update_lowest_neighbor(w_info.lowest_neighbor);
            v_info.update_highest_neighbor(w_info.highest_neighbor);
          } else append(out, v, w);
        }
        // update v_info from the parents of v except its parent in the DFS tree
        if(N.in_degree(v) != 1){
          for(const auto w: N.parents(v)) if(w != u){
            const BridgeInfo<Node>& w_info = node_infos.at(w);
            v_info.update_lowest_neighbor(w_info.disc_time);
            v_info.update_highest_neighbor(w_info.disc_time);
          }
        }
      }
      return v_info;
    }

  public:

    BridgeFinder(const _Network& _N, _Container& _out):
      N(_N), out(_out), time(0), node_infos()
    {
      initial_DFS(N.root());
      bridge_collector_DFS(N.root());
      DEBUG3(std::cout << "found bridges: "<<out<<"\n");
    }

    _Container& list_bridges(const Node u) const { return out; }
    _Container& operator()(const Node u) const { return out; }

  };

  // convenience functions
  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
  _Container list_bridges(const _Network& N, const typename _Network::Node u)
  {
    _Container out;
    return BridgeFinder<_Network, _Container>(N, out).list_bridges(u);
  }



/*
  template<class Node>
  struct BridgeInfo{
    uint32_t disc_time; // ... its discovery time
    uint32_t low;
    const Node parent; // ... its parent in the DFS tree

    BridgeInfo(const Node _parent):
      parent(_parent)
    {}
  };

  // helper function for the bridge finder
  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
	void update_uv(const _Network& N,
                 const typename _Network::Node u,
                 BridgeInfo<typename _Network::Node>& u_info,
                 const typename _Network::Node v,
                 std::unordered_map<typename _Network::Node, BridgeInfo<typename _Network::Node>>& infos,
                 uint32_t& time,
                 _Container& out)
  {
    using Node = typename _Network::Node;

    auto emp_res = infos.emplace(v, u);
    BridgeInfo<Node>& v_info = emp_res.first->second;
    if(emp_res.second){
      list_bridges(N, v, infos, time, out);

      u_info.low = std::min(u_info.low, v_info.low); 
      if(v_info.low > u_info.disc_time) {
        append(out, u, v);
      } else {
        if(u_info.parent != v) 
          u_info.low = std::min(u_info.low, v_info.disc_time); 
      }
    }
  }

  // produce a list of bridges of N below u in post-order & store them in out
  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
	_Container& list_bridges(const _Network& N,
                           const typename _Network::Node u,
                           std::unordered_map<typename _Network::Node, BridgeInfo<typename _Network::Node>>& infos,
                           uint32_t& time,
                           _Container& out)
	{
    using Node = typename _Network::Node;
    BridgeInfo<Node>& u_info = infos.emplace(u, u).first->second;
  
    u_info.disc_time = u_info.low = ++time;
    for(const Node v: N.children(u)) update_uv(N, u, u_info, v, infos, time, out);
    for(const Node v: N.parents(u)) update_uv(N, u, u_info, v, infos, time, out);
    return out;
	} 

  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
	_Container& list_bridges(const _Network& N, _Container& out)
  {
    using _Node = typename _Network::Node;
    uint32_t time = 0;
    std::unordered_map<_Node, BridgeInfo<_Node>> infos;
    list_bridges(N, N.root(), infos, time, out);
    return out;
  }
	
  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
	_Container get_bridges(const _Network& N)
  {
    _Container out;
    list_bridges(N, out);
    std::cout << N << "has bridges: "<< out << "\n";
    return out;
  }
*/
}// namespace
