
#pragma once

#include "set_interface.hpp"
#include "extension.hpp"
#include "tree_extension.hpp"
#include "subsets_constraint.hpp"

namespace PT {

  // this DP table entry recomputes the scanwidth each time, but only stores the essentials (the extension)
  template<PhylogenyType Network,
           class NetworkDegrees = DefaultDegrees<Network>>
  struct _DPEntryLowMem {
    Extension ex;

    sw_t get_scanwidth() const { return ex.template scanwidth<Network, NetworkDegrees>(); }
    // update entry with the next node u
    void update(const NodeDesc u) { mstd::append(ex, u); }
  };

  // this DP table entry stores alot of stuff in order to avoid re-computing the scanwidth each time (good if you have plenty of mem, but not much time)
  template<PhylogenyType Network,
           class NetworkDegrees = DefaultDegrees<Network>>
  struct _DPEntry: public _DPEntryLowMem<Network> {
    using Parent = _DPEntryLowMem<Network>;
    using Edge = typename Network::Edge;
    using Parent::ex;
    using DynamicScanwidth = typename Extension::DynamicScanwidth<Network, NodeMap<sw_t>, NetworkDegrees>;

    DynamicScanwidth ds; 
    sw_t scanwidth = 0;

    sw_t get_scanwidth() const { return scanwidth; }
    // update entry with the next node u
    void update(const NodeDesc u) {
      Parent::update(u);
      ds.update_sw(u);
      scanwidth = std::max(scanwidth, ds.out.at(u));
    }
  };


  template<StrictPhylogenyType Network, class EdgeWeightExtracter = void>
  struct WeightedDegrees {

    Degrees operator()(const NodeDesc u) const {
      Degrees result{0,0};
      EdgeWeightExtracter extract;
      for(const auto& v_adj: Network::parents(u)) result.first += extract(v_adj);
      for(const auto& v_adj: Network::children(u)) result.second += extract(v_adj);
      return result;
    }
  };
  template<StrictPhylogenyType Network>
  struct WeightedDegrees<Network, void> {
    Degrees operator()(const NodeDesc u) { return Network::degrees(u); }
  };


  // NOTE: you can pass an EdgeWeightExtracter functor that, given an edge uv of the network, returns the number of edges represented by this edge
  //       this is useful when doing preprocessing which can double certain edges or even have edges represent other edges
  //       if this is void, only the edge itself is considered
  template<bool low_memory_version,
           PhylogenyType Network,
           class EdgeWeightExtracter = void,
           bool ignore_deg2 = false>
  class ScanwidthDP {
  public:
    using DegreeExtracter = WeightedDegrees<Network, EdgeWeightExtracter>;
    using LowMemEntry = _DPEntryLowMem<Network, DegreeExtracter>;
    using NormalEntry = _DPEntry<Network, DegreeExtracter>;
    using DPEntry = typename std::conditional_t<low_memory_version, LowMemEntry, NormalEntry>;
    using DPTable = std::unordered_map<NodeSet, DPEntry, mstd::set_hash<NodeSet>>;
 
  protected:
    Network& N;
    DPTable dp_table;

    // return whether u is a root in N[c], that is, if u has parents in c
    bool is_root_in_set(const NodeDesc u, const NodeSet& c) {
      for(auto v: node_of<Network>(u).parents()){
        // ignore deg-2 nodes
        if constexpr (ignore_deg2) while(Network::is_suppressible(v)) v = Network::parent(v);
        if(mstd::test(c, v)) return false;
      }
      return true;
    }
  
  public:

    ScanwidthDP(Network& _N): N(_N) {}

    // NOTE: you can pass either an extension or a callable to register nodes in order
    //       if you pass any iterable, then we will append each node's NodeData to it in order
    template<bool include_root = false, class RegisterNode>
    void compute_min_sw_extension_no_bridges(RegisterNode&& _register_node) {
      DEBUG4(std::cout << "computing scanwidth of block:\n"<<ExtendedDisplay(N)<<" (low mem: "<< low_memory_version <<")\n");

      // this is the main dynamic programming table - it could grow exponentially large...
      // the table maps a set X of nodes to any extension with smallest sw for the graph where all nodes but X are contracted onto the root
      // start off with the empty set of scanwidth 0

      if(N.num_nodes() > 1){
        // rememeber the best entry for the last node-set (which contains the root since the NetworkConstraintSubsetFactory goes bottom-up
        typename DPTable::iterator last_iter;
       
        DEBUG5(std::cout << "======= checking constraint node subsets ========\n");
        // check all node-subsets constraint by the arcs in N
        STAT(uint64_t num_subsets = 0;)
        for(auto& nodes: NetworkConstraintSubsetFactory<Network, NodeSet, ignore_deg2>(N)){
          DEBUG4(std::cout << "\tcurrent subset: "<<nodes<<"\n");
          sw_t best_sw = std::numeric_limits<sw_t>::max();
          last_iter = mstd::append(dp_table, std::move(nodes)).first; // if the node-container is non-const, move the nodes into the map
          DPEntry& best_entry = last_iter->second;

          STAT(++num_subsets; DEBUG4(std::cout << "processed "<<num_subsets<<" subsets\n"));
          DEBUG5(
              std::cout << "computing best partial extension for node-set "<<last_iter->first<< "\n";
              std::cout << "....::::: best extensions ::::....\n";
              for(const auto& x: dp_table) std::cout << x.first << ":\t"<<x.second.ex<<"\n";
              );

          // for each node u in the set, check the sw of the extension (dp_table[nodes-u].ex + u)
          for(const NodeDesc u: nodes){
            // first, make sure that u is a root in N[nodes]
            if(is_root_in_set(u, nodes)){
              // look-up the best extension for nodes - u
              NodeSet lookup_set(nodes);
              lookup_set.erase(u);
              // copy the dp-table entry at lookup_set
              DPEntry entry = dp_table.at(lookup_set);
              // add u at the end of it and compute the sw
              DEBUG5(std::cout << "looked up table for " <<lookup_set<<" (u = "<<u<<"): "<< entry.ex<<std::endl);
              // append u along with its direct deg-2 ancestors and update the sw-map
              entry.update(u);
              // also append all suppressible ancestors of u
              if constexpr (ignore_deg2) {
                for(NodeDesc v: N.parents(u))
                  while(N.is_suppressible(v)){
                    entry.update(v);
                    v = N.parent(v);
                  }
              }
              // compute the new scanwidth
              const sw_t sw = entry.get_scanwidth();
              if(sw < best_sw){
                DEBUG4(std::cout << "storing best extension "<<entry.ex << ":\t sw = "<<sw<<"\n");
                best_sw = sw;
                best_entry = std::move(entry); // move assignment
              }
            }
          }
        }
        STAT(uint64_t count_unsupp = 0; for(const NodeDesc u: N.nodes()) { if(!N.is_suppressible(u)) ++count_unsupp;})
        STAT(std::cout << "STAT: " <<N.num_nodes() << " nodes, "<<count_unsupp<<" non-suppressible & "<<num_subsets << " subsets\n";)
        // the last extension should be the one we are looking for
        const auto& ex = last_iter->second.ex;
        std::cout << "\n\nfound extension "<<ex<<" for\n"<<ExtendedDisplay(N)<<"\n";
        assert(ex.size() == N.num_nodes());
        size_t num_nodes = ex.size();
        if constexpr (!include_root) --num_nodes;
        for(size_t i = 0; i != num_nodes; ++i)
          mstd::append(_register_node, ex[i]);
      } else {
        if constexpr (include_root)
          mstd::append(_register_node, N.root());
      }
    }
  };


}
