
#pragma once

#include "optional.hpp"
#include "set_interface.hpp"
#include "extension.hpp"
#include "dynamic_sw.hpp"
#include "tree_extension.hpp"
#include "subsets_constraint.hpp"

namespace PT {

  struct ProtoDPEntry {
    // NOTE: some parts of the code rely on XOR-hashing here, so don't change that willy-nilly!
    static constexpr mstd::XOR_hash<Extension> Hasher{};

    template<NodeIterableType Nodes>
    static constexpr size_t hash(const Nodes& nodes) { return Hasher(nodes); }

    size_t hash_cache = 0;

    void hash_one(const NodeDesc u) { hash_cache = Hasher.hash_one(hash_cache, u); }
    void recompute_hash(const auto& ex) { hash_cache = hash(ex); }

    void clear() { hash_cache = 0; }

    size_t hash() const { return hash_cache; }

    bool operator==(const ProtoDPEntry other) const { return hash_cache == other.hash_cache; }
    ProtoDPEntry& operator=(const ProtoDPEntry& other) noexcept = default;
  };


  // this DP table entry recomputes the scanwidth each time, but only stores the essentials (the extension)
  template<PhylogenyType Network, class NetworkDegrees = DefaultDegrees<Network>>
  struct DPEntryLowMem_: public ProtoDPEntry {
    using Parent = ProtoDPEntry;

    using Parent::hash;

    static constexpr void recompute_sw() {}
    static constexpr void update_sw(const NodeDesc u) {}
    
  protected:
    mutable Extension ex;

    // copy the other entry's Extension, replacing our own prefix
    // NOTE: it's important that the prefix contains the same nodes!
    // NOTE: only friends can do this since they know what they are doing
    void replace_prefix(const DPEntryLowMem_& other) {
      assert(ex.size() >= other.ex.size());
      assert(std::ranges::is_permutation(other.ex, NodeSpan{ex}.subspan(0, other.ex.size())));
      std::ranges::copy(other.ex, ex.begin());
    }

  public:
    using DynamicSW = DynamicScanwidth<Network, NodeMap<sw_t>, NetworkDegrees>;
    using SWInfo = std::pair<sw_t, DynamicSW>;

    DPEntryLowMem_() = default;
    DPEntryLowMem_(const DPEntryLowMem_&) = default;
    DPEntryLowMem_(DPEntryLowMem_&&) noexcept = default;

    // we allow making a DPEntry with a wrong hash, in order to allow hash-based table-lookup without constructing the extension
    // NOTE: please be careful with this!
    explicit constexpr DPEntryLowMem_(const Parent p): Parent{p} {}
    explicit constexpr DPEntryLowMem_(const size_t _hash): Parent{_hash} {}
   
    template<NodeIterableType Nodes>
    explicit DPEntryLowMem_(Nodes&& nodes) noexcept:
      Parent{hash(ex)},
      ex(std::forward<Nodes>(nodes))
    {}

    DPEntryLowMem_& operator=(const DPEntryLowMem_& other) = default;
    DPEntryLowMem_& operator=(DPEntryLowMem_&& other) noexcept = default;
    DPEntryLowMem_& operator=(ProtoDPEntry other) { Parent::operator=(other); ex.clear(); return *this; }



    bool operator==(const DPEntryLowMem_&) const = default;

    SWInfo get_dynamic_scanwidth() const {
      SWInfo result;
      result.first = result.second.update_all(ex);
      return result;
    }

    
    const Extension& get_ex() const { return ex; }

    void swap_nodes(const size_t i, const size_t j) const { std::swap(ex.at(i), ex.at(j)); }

    sw_t get_scanwidth() const { return ex.template scanwidth<Network, NetworkDegrees>(); }

    // update entry with the next node u
    void update(const NodeDesc u) {
      mstd::append(ex, u);
      Parent::hash_one(u);
    }
    void clear() { ex.clear(); Parent::clear(); }

    // this is for debugging purposes only
    bool hash_correct() const { return hash() == hash(ex); }

    // ------------ friends -------------------
    template<bool, PhylogenyType, class> friend class ScanwidthDP2; // we need this to make a fake-Entry to avoid copying/moving around the Extension
  };

  // this DP table entry stores alot of stuff in order to avoid re-computing the scanwidth each time (good if you have plenty of mem, but not much time)
  template<PhylogenyType Network, class NetworkDegrees = DefaultDegrees<Network>>
  struct DPEntry_: public DPEntryLowMem_<Network> {
    using Parent = DPEntryLowMem_<Network>;
    using Edge = typename Network::Edge;
    using Parent::ex;
    using typename Parent::DynamicSW;
    using typename Parent::SWInfo;

    using Parent::Parent;
    
    bool operator==(const DPEntry_& other) const { return Parent::operator==(other); }
    DPEntry_& operator=(ProtoDPEntry other) { Parent::operator=(other); ds.clear(); scanwidth = 0; return *this; }

   
    explicit constexpr DPEntry_(const ProtoDPEntry p):
      Parent(p)
    {}

  protected:
    DynamicSW ds;
    sw_t scanwidth = 0;
    
    void replace_prefix(const DPEntry_& other) {
      Parent::replace_prefix(other);
      ds = other.ds;
      scanwidth = other.scanwidth;
      for(const NodeDesc u: NodeSpan{ex}.subspan(other.ex.size()))
        update_sw(u);
    }

    void recompute_sw() {
      ds.clear(); scanwidth = 0;
      for(const NodeDesc u: ex) update_sw(u);
    }

    void update_sw(const NodeDesc u) {
      scanwidth = std::max(scanwidth, ds.update_sw(u));
    }

  public:
    SWInfo get_dynamic_scanwidth() const { return SWInfo{scanwidth, ds}; }

    sw_t get_scanwidth() const { return scanwidth; }
    // update entry with the next node u
    void update(const NodeDesc u) {
      Parent::update(u);
      update_sw(u);
    }
    void clear() { Parent::clear(); ds.clear(); scanwidth = 0; }

    template<bool, PhylogenyType, class>
    friend class ScanwidthDP2; // we need this to make a fake-Entry to avoid copying/moving around the Extension
  };

  template<PhylogenyType Network, bool low_mem = false, class NetworkDegrees = DefaultDegrees<Network>>
  using SWDPEntry = std::conditional_t<low_mem, DPEntryLowMem_<Network, NetworkDegrees>, DPEntry_<Network, NetworkDegrees>>;

  template<StrictPhylogenyType Network, class EdgeWeightExtracter = void>
  struct WeightedDegrees {
    Degree operator()(const NodeDesc u, const typename Network::Adjacency& v_adj) const {
      EdgeWeightExtracter extract;
      return extract(v_adj);
    }
    Degree operator()(const typename Network::Adjacency& u_adj, const NodeDesc v) const {
      EdgeWeightExtracter extract;
      return extract(u_adj);
    }

    Degrees operator()(const NodeDesc u) const {
      Degrees result{0,0};
      EdgeWeightExtracter extract;
      for(const auto& v_adj: Network::parents(u)) result.first += extract(v_adj);
      for(const auto& v_adj: Network::children(u)) result.second += extract(v_adj);
      return result;
    }
  };
  template<StrictPhylogenyType Network>
  struct WeightedDegrees<Network, void>: public DefaultDegrees<Network> {};


  // NOTE: you can pass an EdgeWeightExtracter functor that, given an edge uv of the network, returns the number of edges represented by this edge
  //       this is useful when doing preprocessing which can double certain edges or even have edges represent other edges
  //       if this is void, only the edge itself is considered
  template<bool low_memory_version,
           StrictPhylogenyType Network,
           class EdgeWeightExtracter = void,
           bool ignore_deg2 = false>
  class ScanwidthDP {
  public:
    using DegreeExtracter = WeightedDegrees<Network, EdgeWeightExtracter>;
    using DPEntry = SWDPEntry<Network, low_memory_version, DegreeExtracter>;
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

    ScanwidthDP(Network& N_): N(N_) {}

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
          DEBUG2(std::cout << "\tcurrent subset: "<<nodes<<"\n");
          sw_t best_sw = std::numeric_limits<sw_t>::max();
          last_iter = mstd::append(dp_table, std::move(nodes)).first; // if the node-container is non-const, move the nodes into the map
          DPEntry& best_entry = last_iter->second;

          STAT(++num_subsets; DEBUG4(std::cout << "processed "<<num_subsets<<" subsets\n"));
          DEBUG5(
              std::cout << "computing best partial extension for node-set "<<last_iter->first<< "\n";
              std::cout << "....::::: best extensions ::::....\n";
              for(const auto& x: dp_table) std::cout << x.first << ":\t"<<x.second.get_ex()<<" --> sw = "<<x.second.get_scanwidth()<<"\n";
              );
#warning "TODO: compute weakly-disconnected parts individually"
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
              DEBUG5(std::cout << "looked up table for " <<lookup_set<<" (u = "<<u<<"): "<< entry.get_ex()<<std::endl);
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
                DEBUG4(std::cout << "storing best extension "<<entry.get_ex() << ":\t sw = "<<sw<<"\n");
                best_sw = sw;
                best_entry = std::move(entry); // move assignment
              }
            }
          }
        }
        STAT(uint64_t count_unsupp = 0; for(const NodeDesc u: N.nodes()) { if(!N.is_suppressible(u)) ++count_unsupp;})
        STAT(std::cout << "STAT: " <<N.num_nodes() << " nodes, "<<count_unsupp<<" non-suppressible & "<<num_subsets << " subsets\n";)
        // the last extension should be the one we are looking for
        const auto& ex = last_iter->second.get_ex();
        DEBUG2(std::cout << "\n\nfound extension "<<ex<<" for\n"<<ExtendedDisplay(N)<<"\n");
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

namespace mstd {
  // in order to use DPEntries with optional_by_invalid's, we'll use the default-constructed DPEntry with hash = 1 as invalid
  template<class P, class Q> struct default_invalid<PT::DPEntryLowMem_<P, Q>> { static constexpr auto value() { return PT::ProtoDPEntry{1}; }; };
  template<class P, class Q> struct default_invalid<PT::DPEntry_<P, Q>> { static constexpr auto value() { return PT::ProtoDPEntry{1}; }; };
}
namespace std {
  template<class P, class Q> struct hash<PT::DPEntryLowMem_<P,Q>> { auto operator()(const PT::DPEntryLowMem_<P,Q>& x) const { return x.hash(); } };
  template<class P, class Q> struct hash<PT::DPEntry_<P,Q>> { auto operator()(const PT::DPEntry_<P,Q>& x) const { return x.hash(); } };
}




