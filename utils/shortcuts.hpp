
// This computes all shortcuts of a DAG D.
// This can also be used to compute the transitive reduction of D (just remove all shortcuts)
//
// 
// each vertex u carries a set R of reticulations that are descendants of u

#pragma once

#include "types.hpp"

namespace PT {

  // this class computes all shortcuts / the transitive reduction in O(r*m) time
  // NOTE: it can be used as a predicate to return true if something is a shortcut
  // NOTE: the user can pass the type of the ShortCutContainer we shall use and return
  //    ATTENTION: the ShortCutContainer will be queried if the class is used as a predicate,
  //               so if you want to use it as a predicate, be sure to choosee something queryable as ShortCutContainer!
  // NOTE: if you want get_path to output edges/adjacencies with the EdgeData, then set the preserve_data flag
  template<StrictPhylogenyType Network, mstd::ContainerType ShortCutContainer = NodeMap<NodeSet>, bool preserve_data = false>
  struct Shortcuts {
  protected:
    using ShortCutElement = mstd::mapped_or_value_type_of_t<ShortCutContainer>;
    static constexpr bool shortcuts_in_map = mstd::MapType<ShortCutContainer>;
    static constexpr bool shortcuts_are_edges = EdgeType<ShortCutElement>;

    using EdgeData = std::conditional_t<preserve_data, typename Network::EdgeData, void>;
    using Adj = Adjacency<EdgeData>;
    using PathTable = NodeMap<Adj>; // a pair (r,v) in a PathTable means that r is reachable via v

    // each node stores the next node on the path to each reticulation
    NodeMap<PathTable> path_to_Reti;
    // shortcut store
    ShortCutContainer shortcuts;


    // merge paths from a child of u into the path-table of u
    void merge_paths(auto& v, PathTable& u_table, const PathTable& child_table) {
      for(const auto& rw: child_table)
        append(u_table, rw.first, v);
    }

    // initialize the mappings
    void init(NodeVec todo) {
      // we need a vertex-queue to contain the vertices that we still need to treat
      // we also need to keep track of how many children of v have been treated since we only add vertices to 'todo' whose all children have been treated
      NodeMap<Degree> children_done;

      DEBUG4(std::cout << "starting leaves: "<<todo<<"\n");
      while(!todo.empty()){
        const NodeDesc u = mstd::value_pop(todo);
        PathTable uR;
        std::unordered_set<Adj> reti_children;
        
        // step 1: merge the reachability-sets of the children of u
        for(const Adj& v: Network::children(u)) {
          const auto iter = path_to_Reti.find(v);
          if(iter != path_to_Reti.end()) merge_paths(v, uR, iter->second); // this is the bottleneck causing Theta(m * r) running time
          if(Network::is_reti(v)) append(reti_children, v);
        }
        DEBUG4(std::cout << "reti-reachability of "<< u <<": "<< uR <<"\t+\t" << reti_children  <<"\n");
        
        // step 2: check if any of the reticulation children of u are reachable from u (by a non-trivial path)
        // step 3: insert the reticulation children of u into path_to_Reti[u]
        for(auto& r: reti_children) {
          const bool success = append(uR, r, r).second;
          if(!success) {
            append(shortcuts, u, std::move(r));
          }
        }
        
        // step 4: save uR for later (unless it's empty)
        if(!uR.empty()) append(path_to_Reti, u, std::move(uR));

        // step 5: add the parents of u to todo if possible
        for(const NodeDesc p: Network::parents(u)) {
          const auto p_outdeg = Network::out_degree(p);
          if(p_outdeg > 1) {
            const auto [iter, success] = append(children_done, p);
            if(!success) {
              if(--(iter->second) == 0) {
                append(todo, p);
                erase(children_done, iter);
              }
            } else iter->second = p_outdeg - 1;
          } else append(todo, p);
        }
      }
    }


    template<EdgeType SomeEdge>
    bool is_shortcut(const SomeEdge& uv) const {
      if constexpr (shortcuts_are_edges)
        return test(shortcuts, uv);
      else return is_shortcut(uv.tail(), uv.head());
    }
    bool is_shortcut(const NodeDesc u, const auto& v) const {
      if constexpr (!shortcuts_are_edges) {
        static_assert(shortcuts_in_map);
        return test(shortcuts.at(u), v);
      } else return is_shortcut(PT::Edge{u,v});
    }


  public:

    Shortcuts(NodeVec leaves) {
      DEBUG4(std::cout << "finding shortcuts...\n");
      init(std::move(leaves));
      DEBUG4(std::cout << "found shortcuts:\n" << shortcuts << "\n");
    }
    
    Shortcuts(const Network& N):
      Shortcuts(N.leaves().template to_container<NodeVec>())
    {}

    template<EdgeType SomeEdge>
    bool operator()(const SomeEdge& uv) const { return is_shortcut(uv); }
    bool operator()(const NodeDesc u, const NodeDesc v) const { return is_shortcut(u,v); }

    const auto& get_all_shortcuts() const { return shortcuts; }

    // for nodes s and t where t is a reticulation, construct a path from s to t if there is one, otherwise return the empty path
    //NOTE: instead of a container, the user can pass a lambda taking as argument either a NodeDesc (next node) or a (NodeDesc, Adjacency)-pair (next edge)
    template<class Path> requires (!std::is_reference_v<Path>)
    void get_path(NodeDesc s, const NodeDesc t, Path& P) const {
      constexpr bool P_stores_edges = (std::invocable<Path, NodeDesc, Adj> || EdgeType<mstd::value_type_of_t<Path>>);
      while(s != t) {
        const auto s_iter = path_to_Reti.find(s);
        if(s_iter != path_to_Reti.end()) {
          const auto& reachable_from_s = s_iter->second;
          const auto t_iter = reachable_from_s.find(t);
          assert(t_iter != reachable_from_s.end());
          if constexpr (P_stores_edges)
            append(P, s, t_iter->second);
          else
            append(P, s);
          s = t_iter->second;
        } else return;
      }
      if constexpr (!P_stores_edges) append(P, t);
    }
    template<mstd::ContainerType C>
    C get_path(NodeDesc s, const NodeDesc t) const { C result; get_path(s,t,result); return result; }
  };



  template<mstd::ContainerType ShortCutContainer = NodeMap<NodeDesc>, bool preserve_data = false, StrictPhylogenyType Network>
  auto detect_shortcuts(const Network& N) {
    return Shortcuts<Network, ShortCutContainer, preserve_data>(N);
  }
  template<mstd::ContainerType ShortCutContainer = NodeMap<NodeDesc>, bool preserve_data = false, StrictPhylogenyType Network, NodeContainerType Nodes>
  auto detect_shortcuts(Nodes&& leaves) {
    return Shortcuts<Network, ShortCutContainer, preserve_data>(std::forward<Nodes>(leaves));
  }

}
