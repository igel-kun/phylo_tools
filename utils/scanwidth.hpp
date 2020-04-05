
#pragma once

#include "set_interface.hpp"
#include "extension.hpp"
#include "tree_extension.hpp"
#include "bridges.hpp"
#include "subsets_constraint.hpp"
#include "biconnected_comps.hpp"

namespace PT{


  // this DP table entry recomputes the scanwidth each time, but only stores the essentials (the extension)
  template<class _Network>
  struct _DPEntryLowMem {
    Extension ex;

    uint32_t get_scanwidth(const _Network& N) const { return ex.scanwidth(N); }
    // update entry with the next node u
    void update(const _Network& N, const Node u) { append(ex, u); }
  };

  // this DP table entry stores alot of stuff in order to avoid re-computing the scanwidth each time (good if you have plenty of mem, but not much time)
  template<class _Network>
  struct _DPEntry: public _DPEntryLowMem<_Network> {
    using Parent = _DPEntryLowMem<_Network>;
    using Edge = typename _Network::Edge;
    using Parent::ex;

    std::DisjointSetForest<Edge> weak_components;
    std::unordered_map<Node, uint32_t> sw_map;
    uint32_t scanwidth = 0;

    uint32_t get_scanwidth(const _Network& N) const { return scanwidth; }
    // update entry with the next node u
    void update(const _Network& N, const Node u)
    {
      Parent::update(N, u);
      scanwidth = std::max(scanwidth, ex.update_sw(N, u, weak_components, sw_map));
    }
  };




  template<bool low_memory_version, class _Network, class _Extension>
  class ScanwidthDP
  {
  public:
    using DPEntry = typename std::conditional_t<low_memory_version, _DPEntryLowMem<_Network>, _DPEntry<_Network>>;
 
  protected:
    const _Network& N;
    const bool ignore_deg2;
    std::unordered_map<NodeSet, DPEntry, std::set_hash<NodeSet>> dp_table;

    // return whether u is a root in N[c], that is, if u has parents in c
    inline bool is_root_in_set(const Node u, const NodeSet& c)
    {
      for(auto v: N.parents(u)){
        // ignore deg-2 nodes
        if(ignore_deg2) while(N.is_suppressible(v)) v = std::front(N.parents(v));
        if(contains(c, v)) return false;
      }
      return true;
    }

  public:

    ScanwidthDP(const _Network& _N, const bool _ignore_deg2 = true): N(_N), ignore_deg2(_ignore_deg2)
    {}

    void compute_min_sw_extension_no_bridges(_Extension& ex)
    {
      DEBUG4(std::cout << "computing scanwidth of block:\n"<<N<<"\n";);

      // this is thee main dynamic programming table - it could grow exponentially large...
      // the table maps a set X of nodes to any extension with smallest sw for the graph where all nodes but X are contracted onto the root
      // start off with the empty set of scanwidth 0

      DPEntry* best_entry = new DPEntry();
      if(N.num_nodes() > 1){
        DEBUG5(std::cout << "======= checking constraint node subsets ========\n");
        // check all node-subsets constraint by the arcs in N
        STAT(uint64_t num_subsets = 0;)
        for(const auto& nodes: NetworkConstraintSubsets<_Network, NodeSet>(N)){
          STAT(++num_subsets);

          DEBUG5(
              std::cout << "computing best partial extension for node-set "<<nodes << "\n";
              std::cout << "....::::: best extensions ::::....\n";
              for(const auto& x: dp_table) std::cout << x.first << ":\t"<<x.second.ex<<"\n";
              );

          // for each node u in the set, check the sw of the extension (dp_table[nodes-u].ex + u)
          uint32_t best_sw = N.num_nodes() + 1;
          for(const Node u: nodes){
            // first, make sure that u is a root in N[nodes]
            if(is_root_in_set(u, nodes)){
              // look-up the best extension for nodes - u
              NodeSet lookup_set(nodes);
              lookup_set.erase(u);
              // copy the dp-table entry at lookup_set
              DPEntry* entry = new DPEntry(dp_table.at(lookup_set));
              // add u at the end of it and compute the sw
              DEBUG5(std::cout << "looked up table for " <<lookup_set<<" (u = "<<u<<"): "<< entry->ex<<std::endl);
         
              // append u along with its direct deg-2 ancestors and update the sw-map
              entry->update(N, u);
              // also append all suppressible ancestors of u
              for(auto v: N.parents(u))
                while(N.is_suppressible(v)){
                  entry->update(N, v);
                  v = std::front(N.parents(v));
                }
              // compute the new scanwidth
              const uint32_t sw = entry->get_scanwidth(N);
              if(sw < best_sw){
                best_sw = sw;
                if(best_entry) delete best_entry;
                best_entry = entry;
              }
            }
          }
          // lastly, add the best extension for nodes to the lookup table
          dp_table.emplace(nodes, *best_entry);
        }
        STAT(uint64_t count_unsupp = 0; for(const auto& u: N.get_nodes()) { if(!N.is_suppressible(u)) ++count_unsupp;})
        STAT(std::cout << "STAT: " <<N.num_nodes() << " nodes, "<<count_unsupp<<" non-suppressible & "<<num_subsets << " subsets\n";)
        // the last extension should be the one we are looking for
        append(ex, best_entry->ex);
        delete best_entry;
      } else append(static_cast<NodeVec&>(ex), N.root());
    }
  };

  template<bool low_memory_version, class _Network, class _Extension>
  void compute_min_sw_extension(const _Network& N, _Extension& ex)
  {
    using Component = RONetwork<>;

    for(const Component bcc: BiconnectedComponents<_Network, Component>(N)){
      std::cout << "found biconnected component:\n"<< bcc <<"\n";
      if(bcc.num_edges() == 1){
        const auto uv = std::front(bcc.get_edges());
        append(static_cast<NodeVec&>(ex), uv.head());
      } else {
        ScanwidthDP<low_memory_version, Component, _Extension> dp(bcc);
        dp.compute_min_sw_extension_no_bridges(ex);
        // always remove the root of a component, so the bridge can re-insert it
        ex.pop_back();
      }
    }
    append(static_cast<NodeVec&>(ex), N.root());
  }


}// namespace


