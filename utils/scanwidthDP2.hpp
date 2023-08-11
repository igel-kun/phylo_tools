
#pragma once

#include "set_interface.hpp"
#include "union_find.hpp"
#include "extension.hpp"
#include "tree_extension.hpp"
#include "subsets_constraint.hpp"
#include "scanwidthDP.hpp"

// this scanwidth DP is implemented as a top-down branching with memoization
// its features are:
// 1. moves nodes with at least as many incoming as outgoing edges to the right in the resulting extension
// 2. considers weakly-disconnected sub-extensions seperately

namespace PT {
  // to hash DP entries, we just hash their partial extension with a set-hasher (which ignores ordering)
  struct DP_Entry_Hash {
    static constexpr mstd::set_hash<Extension> Hasher{};
    
    template<class Entry> requires (!mstd::IterableType<Entry>)
    size_t operator()(const Entry& entry) const { return entry.hash(); }
    
    template<NodeIterableType Nodes>
    size_t operator()(const Nodes& nodes) const { return Hasher(nodes); }
  };




  // NOTE: you can pass an EdgeWeightExtracter functor that, given an edge uv of the network, returns the number of edges represented by this edge
  //       this is useful when doing preprocessing which can double certain edges or even have edges represent other edges
  //       if this is void, only the edge itself is considered
  template<bool low_memory_version,
           PhylogenyType Network,
           class EdgeWeightExtracter = void>
  class ScanwidthDP2 {
  public:
    using DegreeExtracter = WeightedDegrees<Network, EdgeWeightExtracter>;
    using LowMemEntry = _DPEntryLowMem<Network, DegreeExtracter>;
    using NormalEntry = _DPEntry<Network, DegreeExtracter>;
    using DPEntry = typename std::conditional_t<low_memory_version, LowMemEntry, NormalEntry>;
    using DPTable = std::unordered_set<DPEntry, DP_Entry_Hash>;
    //using WeakComps = mstd::DisjointSetForest<NodeDesc>;

  protected:
    Network& N;
    DPTable dp_table;
    [[ no_unique_address ]] DegreeExtracter degrees;

    // ----------------------- weak component stuff -----------------------

    struct WeakComps {
      mstd::DisjointSetForest<NodeDesc> comps;

      // compute the wealy-connected components in the given set of nodes
      template<NodeIterableType Nodes>
      static auto get_components(const Nodes& nodes) {
        WeakComps comps;
        augment_components(comps, nodes);
        return comps;
      }

      // add a few more nodes to the given DisjointSetForest
      template<NodeIterableType Nodes>
      static void augment_components(WeakComps& comps, Nodes&& nodes) {
        for(const NodeDesc u: nodes) comps.emplace_set(u);
        for(const NodeDesc u: nodes)
          for(const NodeDesc v: Network::children(u)){
            assert(comps.contains(v)); // check if we have a downwards-closed set
            comps.merge_sets(v, u);
          }
      }

      size_t num_components() const { return comps.set_count(); }

      bool contains(const NodeDesc u) const { return test(comps, u); }
    };

    // decide if u is sw-raising among the roots
    // NOTE: a root is sw-raising iff there are weakly-connected components that are merged by this root but not by the other roots together
    // NOTE: the DSF is passed by value, thus making a copy of it
    template<NodeIterableType Nodes>
    static bool is_sw_raising(const NodeDesc u, Nodes&& roots, WeakComps comps) {
  #error "this is wrong, we need to use the DegreeExtracter to determine whether something is sw-raising!"
      // step 1: add all roots except u to the DSF
      augment_components(comps, roots | std::views::filter([u](const NodeDesc v){return u != v;}));
      // step 2: check if all children of u end up in the same component
      return comps.in_different_sets(Network::children(u));
    }


    // ------------------------ query stuff -----------------------------

    template<NodeIterableType Nodes>
    struct Query {
      // nodes are to be sorted into 4 cathegories:
      // |--------------------- nodes -------------------------|
      // |--non_roots------|--------------roots----------------|
      //                   |-----------non_fix---------|--fix--|
      //                   |--non-raising--|--raising--|
      //
      // 1. we always put the fix roots last in the extension
      // 2. if there are sw-raising roots, we branch on putting one of them "last" (that is, before the fix roots)
      // 3. otherwise, we branch on putting a non-raising root "last"
      //
      // NOTE: the roots are an anti-chain, so we can permute them arbitrary without violating topological order
      Nodes nodes;
      size_t non_roots; 
      size_t all_but_fixed_roots; 
      size_t all_but_fixed_or_raising; 
      size_t hash;
      WeakComps comps;


      template<NodeIterableType _Nodes>
      void init(Query<_Nodes>&& other) {
        nodes = std::forward<_Nodes>(other.nodes);
        non_roots = other.non_roots;
        all_but_fixed_roots = other.all_but_fixed_roots;
        all_but_fixed_or_raising = other.all_but_fixed_roots; // since we removed some roots to get here, some raising roots may now be non-raising
        hash = other.hash;

        // step 1: repeatedly replace all roots with outgoing weight <= 1
        fix_non_tree_roots(degrees);

        // step 2: find connected components when we remove the roots
        comps = get_components(nodes | std::views::take(non_roots));
        DEBUG4(std::cout << "computed " << comps.set_count() << " weak components: "<< comps << "\n");

        // step 3: for each sw-raising root, replace it with its children
        //         if any replacement happend, then we're ready for the recursive call
        compute_sw_raising(comps);
      }

      size_t num_components() const { return comps.num_components(); }
      size_t num_sw_raising_roots() const { return all_but_fixed_roots - all_but_fixed_or_raising; }
      size_t num_non_fixed_roots() const { return all_but_fixed_roots - comps.num_components(); }

      // translate a DisjointSetForest into a mapping in which each representative is mapped to its weak component (a NodeVec) and the number of non-roots
      // NOTE: we keep the relative order of the nodes, so the non-root ones preceed the root ones
      auto get_component_map() const {
        using ComponentAndNum = std::pair<NodeVec, size_t>;
        using Output = NodeMap<ComponentAndNum>;
        const auto non_fixed = nodes | std::views::take(all_but_fixed_roots);

        // step 1: compute the non-root parts of the components
        Output out_map;
        size_t i = 0;
        for(;i != non_roots; ++i) {
          const NodeDesc u = non_fixed[i]; // why the heck does C++ ranges library not provide at() but does provide operator[]() ?!
          const NodeDesc rep = comps.representative(u);
          auto& rep_comp = out_map[rep];
          append(rep_comp.first, u);
        }
        // step 2: set the non-root sizes
        for(auto& [u, C]: out_map)
          C.second = C.first.size();
        // step 3: add the roots
        for(;i != all_but_fixed_roots; ++i) {
          const NodeDesc u = non_fixed[i];
          const NodeDesc rep = comps.representative(u);
          auto& rep_comp = out_map.at(rep);
          append(rep_comp.first, u);
        }
        assert(out_map.size() > 1);
        return out_map;
      }

      // return the number of parents of u in X
      size_t in_deg(const NodeDesc u) {
        assert(comps.size() == all_but_fixed_roots);
        size_t result = 0;
        for(const NodeDesc p: Network::parents(u))
          result += test(comps, p);
        return result;
      }

      // sort vertices in the given set to the root area
      template<NodeIterableType _Nodes>
      size_t move_to_roots(_Nodes to_move) {
        size_t old_non_roots = non_roots;
        for(size_t i = non_roots; i != 0;) {
          NodeDesc& u = nodes.at(--i);
          if(erase(to_move, u)) {
            if(in_deg(u) <= 1) std::swap(u, nodes.at(--non_roots));
            if(to_move.empty()) break;
          }
        }
        return old_non_roots - non_roots;
      }


      // while there is a root with out-weight <= 1, move its unique child up into the root-section of ex and modify the non_roots counter
      void fix_non_tree_roots(DegreeExtracter& degrees) {
        while(1) {
          NodeSet to_move;
          DEBUG4(std::cout << "fixing roots among "<< (nodes | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots))<<"\n");
          for(size_t i = all_but_fixed_roots; i != non_roots;) {
            NodeDesc& u = nodes.at(--i);
            const auto [indeg, outdeg] = degrees(u);
            if(indeg >= outdeg) {
              DEBUG4(std::cout << "fixing root "<<u<<" with indeg "<<indeg<<" & outdeg "<<outdeg<<"\n");
              // mark u's children as new roots
              append(to_move, Network::children(u));
              // move u to the fixed roots
              std::swap(u, nodes.at(--all_but_fixed_roots));
              DEBUG4(std::cout << "after swap: "<<nodes<<"\n");
            }
          }
          if(move_to_roots(to_move) == 0) break;
        }
        DEBUG4(std::cout << "fixed roots "<< (nodes | std::views::drop(all_but_fixed_roots)) << "\n");
      }

      // comppute sw-raising roots
      size_t compute_sw_raising() {
        if(comps.set_count() > 1){
          NodeSet new_roots;
          DEBUG4(std::cout << "computing sw-raising roots among "<< (nodes | std::views::take(all_but_fixed_or_raising) | std::views::drop(non_roots)) << "\n");
          for(size_t i = non_roots; i < all_but_fixed_or_raising; ++i) {
            NodeDesc& u = nodes.at(i);
            if(is_sw_raising(u, nodes | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots), comps)) {
              append(new_roots, Network::children(u));
              std::swap(u, nodes.at(--all_but_fixed_or_raising));
            }
          }
          DEBUG4(std::cout << "got new roots: "<<new_roots<<"\n");
          return move_to_roots(new_roots);
        } else return 0;
      }

      auto find_in_table(const DPTable& dp_table) const {
        DPEntry tmp{NodeVec{}, hash};

        // WARNING: THIS IS EVIL! Unfortunately, I haven't found a better way to avoiding copies/moves
        return dp_table.find(tmp);
        // take a moment to appreciate what just happend: we created a fake DPEntry with no nodes, but the correct hash for the nodes
        // thus, if a DPEntry with a permutation of the nodes represented by the hash exists, then find returned it
        // otherwise, we are going to compute the Extension in the new entry without computing a new hash for it (we know the hash is correct)
      }

      DPEntry make_entry() && { return DPEntry(std::move(nodes), hash); }

      friend std::ostream& operator<<(std::ostream& os, const Query& Q) {
        os << "\t"<< Q.nodes <<"\n";
        os << Q.non_roots<<" non-roots\t"<< (Q.nodes | std::views::take(Q.non_roots)) << "\n";
        os << "and roots\t"<< (Q.nodes | std::views::drop(Q.non_roots)) << "\n";
        return os;
      }
    };


    const DPEntry& recurse(DPEntry& entry, const size_t query_size, const size_t query_non_roots, const size_t base_hash){
      // recurse for the remaining set
      const size_t subrange_hash = base_hash ^ DPEntry::hash(entry.ex | std::views::drop(query_size));
      DEBUG4(std::cout << "recursing for "<< (entry.ex | std::views::take(query_size))<<"\n");
      const DPEntry& opt_answer = query(entry.ex | std::views::take(query_size), query_non_roots, subrange_hash);
      entry.replace_prefix(opt_answer);  
      DEBUG4(std::cout << "prefix "<< (entry.ex | std::views::take(query_size))<<" of "<<entry.ex<<" is now optimal\n");
    }

    void recurse_for(DPEntry& entry, NodeDesc& root, const size_t query_size, const size_t non_roots, const size_t base_hash) {
      DEBUG4(std::cout << "recusing for root "<< root <<"\n");
      // step 1: swap the root out of the query range
      std::swap(root, entry.ex.at(query_size));
      // step 2: swap the children of the root to the end of the non-roots
      const auto& r_children = Network::children(root);
      if(r_children.size() > config::linear_search_threshold)
        sort_to_before(entry.ex, mstd::to_set<NodeSet>(r_children), non_roots);
      else sort_to_before(entry.ex, r_children, non_roots);
      recurse(entry, query_size, non_roots - r_children.size(), base_hash);
    }

  public:

    ScanwidthDP2(Network& _N): N(_N) {}

    template<NodeIterableType Nodes>
    const DPEntry& query(Nodes&& nodes, const size_t non_roots) {
      return query(std::forward<Nodes>(nodes), non_roots, DPEntry::hash(nodes));
    }

    const DPEntry& query() {
      return query(N.nodes().template to_container<NodeVec>(), N.num_nodes() - N.num_roots());
    }

    const DPEntry& emplace_in_table(Query<Extension>&& Q) {
      return dp_table.emplace(Q.make_entry()).first;
    }

    // query a downwards-closed list X of nodes
    // NOTE: we assume that the roots of the set are the last items in the set
    template<NodeIterableType Nodes>
    const DPEntry& query(Query<Nodes>&& Q) {
      DEBUG4(std::cout << "querying "<< Q << "\n");

      auto iter = Q.find_in_table(dp_table);
      if(iter == dp_table.end()) {
        DEBUG4(std::cout << "no entry for this query yet...\n");
        // if the entry didn't exist before, we will compute it from previous entries
        Query<Extension> tmp;
        tmp.init(std::forward<Query<Nodes>>(Q));

        // step 4: recurse for each root 
        // if all nodes in tmp are roots, then the order doesn't matter and we can just compute its scanwidth
        if(non_roots > 1) {
          DEBUG5(std::cout << "adding the remaining non-fixed roots " << (ex | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots)) << " to the weak-components\n");
          // next, check if X is weakly connected and, if not, recurse for the weak components
          augment_components(comps, ex | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots));

          if(tmp.num_components() == 1) {
            DEBUG4(std::cout << "everything is weakly connected, so we'll recurse for all "<<all_but_fixed_roots - non_roots<<" non-fixed-roots: "<< (ex | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots)) <<"\n");
            // for each root u in X, recurse for X-u and add u in the end of the resulting extension

            // if we have a single root, then there is no need to copy anything
            recurse_for(tmp, ex[all_but_fixed_roots - 1], all_but_fixed_roots - 1, non_roots, nodes_hash);

            // otherwise, we'll have to start juggling temporary DPEntries
            assert(all_but_fixed_roots > non_roots);
            const size_t non_fixed_roots = all_but_fixed_roots - non_roots;
            if(non_fixed_roots > 1) {
              // for each non-fixed root, make that root last in tmp and recurse
              // keep the best entry among all computed ones
              DPEntry best(tmp);
              for(size_t i = all_but_fixed_roots - 1; i != non_roots;) {
                recurse_for(tmp, ex.at(--i), all_but_fixed_roots - 1, non_roots, nodes_hash);
                if(tmp.get_scanwidth() < best.get_scanwidth()) {
                  if(i == non_roots) {
                    best = std::move(tmp);
                    break;
                  } else best = tmp;
                }
              }
              tmp = std::move(best);
            }
          } else { // X is not weakly connected, so recurse for each connected component separately
            DPEntry result;
            for(auto& [u, C_pair]: get_component_map(ex | std::views::take(all_but_fixed_roots), non_roots, comps)) {
              auto& C = C_pair.first;
              const size_t C_non_root = C_pair.second;
              const size_t C_hash = DPEntry::hash(C);
              DEBUG4(std::cout << "querying connected component "<<C<<" with roots "<<(C | std::views::drop(C_non_root))<<"\n");
              const DPEntry& C_entry = query(std::move(C), C_non_root, C_hash);
              append(result.ex, C_entry.ex);
            }
            append(result.ex, ex | std::views::drop(all_but_fixed_roots));
            result.recompute_sw();
            iter = dp_table.emplace(std::move(result)).first;
            return *iter;
          }
        } else tmp.recompute_sw();
        iter = dp_table.emplace(std::move(tmp)).first;
      }
      return *iter;
    }

    // NOTE: you can pass either an extension or a callable to register nodes in order
    //       if you pass any iterable, then we will append each node's NodeData to it in order
    template<bool include_root = false, class RegisterNode>
    void compute_min_sw_extension_no_bridges(RegisterNode&& _register_node) {
      DEBUG4(std::cout << "computing scanwidth of block:\n"<<ExtendedDisplay(N)<<" (low mem: "<< low_memory_version <<")\n");
      if(N.num_nodes() > 1){
        const DPEntry& opt_sol = query();
        const auto& ex = opt_sol.get_ex();
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
