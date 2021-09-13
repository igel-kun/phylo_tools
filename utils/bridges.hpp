
#pragma once

#include "set_interface.hpp"

// this is my own bridge finder
// NOTE: most bridge finders out there work only on undirected graphs, so I modified Tarjan's ideas slightly...

namespace PT{

#warning TODO: make a bridge iterator

  struct BridgeInfo {
    size_t num_descendants;
    size_t disc_time;
    size_t lowest_neighbor; // this is the po-numberof the lowest neighbor of any node in the subtree
    size_t highest_neighbor;

    BridgeInfo(const size_t _disc_time):
      num_descendants(1), disc_time(_disc_time), lowest_neighbor(_disc_time), highest_neighbor(_disc_time)
    {}
 
    // the DFS interval of v is disc_time + [0:num_descendants]
    // uv is a bridge <=> noone in the sub-DFS-tree rooted at v has seen a neighbor outside this interval   
    bool is_bridge_head() const {
      return (disc_time != 0) && (lowest_neighbor >= disc_time) && (highest_neighbor < disc_time + num_descendants);
    }
    void update_lowest_neighbor(const size_t u) { lowest_neighbor = std::min(lowest_neighbor, u); }
    void update_highest_neighbor(const size_t u) { highest_neighbor = std::max(highest_neighbor, u); }

  };

  std::ostream& operator<<(std::ostream& os, const BridgeInfo& b)
  {
    return os << "(disc: "<<b.disc_time<<" desc: "<<b.num_descendants<<" low: "<<b.lowest_neighbor<<" high: "<<b.highest_neighbor<<")";
  }

  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
  class BridgeFinder {
  public:
    using NodeInfoMap = std::unordered_map<NodeDesc, BridgeInfo>;

  protected:
    const _Network& N;
    _Container& out;
    size_t time;
    
    NodeInfoMap node_infos;

    // the initial DFS sets up preorder numbers and numbers of descendants in the DFS-tree
    std::emplace_result<NodeInfoMap> initial_DFS(const NodeDesc u) {
      auto emp_res = node_infos.emplace(u, time);
      if(emp_res.second){
        ++time;
        BridgeInfo& u_info = emp_res.first->second;
        for(const auto v: N.children(u)){
          const auto v_emp_res = initial_DFS(v);
          if(v_emp_res.second){
            const BridgeInfo& v_info = v_emp_res.first->second;
            u_info.num_descendants += v_info.num_descendants;
          }
        }
      }
      return emp_res;
    }

    // NOTE: this will return the bridges in post-order!
    void bridge_collector_DFS(const NodeDesc u) { bridge_collector_DFS(u, u); }

    const std::pair<const BridgeInfo&, const bool> bridge_collector_DFS(const NodeDesc u, const NodeDesc v) {
      BridgeInfo& v_info = node_infos.at(v);
      // we use disc_time = 0 as indication of having already visited v
      if(v_info.disc_time != 0){
        for(const auto w: N.children(v)){
          const auto w_info = bridge_collector_DFS(v, w);
          // register bridge if need be
          if(!w_info.second){
            // update u_info if uv is not a bridge (if it is, no update is necessary)
            v_info.update_lowest_neighbor(w_info.first.lowest_neighbor);
            v_info.update_highest_neighbor(w_info.first.highest_neighbor);
          } else append(out, v, w);
        }
        // update v_info from the parents of v except its parent in the DFS tree
        if(N.in_degree(v) != 1){
          for(const auto w: N.parents(v)) if(w != u){
            const BridgeInfo& w_info = node_infos.at(w);
            v_info.update_lowest_neighbor(w_info.disc_time);
            v_info.update_highest_neighbor(w_info.disc_time);
          }
        }
        const bool bridge_head = v_info.is_bridge_head();
        v_info.disc_time = 0;
        return {v_info, bridge_head};
      }
      return {v_info, false};
    }

  public:

    BridgeFinder(const _Network& _N, _Container& _out):
      N(_N), out(_out), time(1), node_infos()
    {
      initial_DFS(N.root());
      bridge_collector_DFS(N.root());
      DEBUG3(std::cout << "This is a BridgeFinder of\n"<<N<<"\nfound bridges: "<<out<<"\n");
    }

    _Container& list_bridges() const { return out; }
    _Container& operator()() const { return out; }
  };

  // convenience functions
  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
  _Container list_bridges(const _Network& N, const NodeDesc u, _Container&& out = _Container()) {
    return BridgeFinder(N, out).list_bridges();
  }
  template<class _Network, class _Container = std::vector<typename _Network::Edge>>
  _Container list_bridges(const _Network& N, _Container&& out = _Container())
  {
    return BridgeFinder(N, out).list_bridges();
  }

}// namespace
