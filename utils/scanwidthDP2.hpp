
#pragma once

#include <span>
#include "set_interface.hpp"
#include "union_find.hpp"
#include "extension.hpp"
#include "tree_extension.hpp"
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
    size_t operator()(const std::unique_ptr<Entry>& entry) const { return Hasher(*entry); }
    
    template<NodeIterableType Nodes>
    size_t operator()(const Nodes& nodes) const { return Hasher(nodes); }
  };



  // NOTE: you can pass a stateless(!) EdgeWeightExtracter functor that, given an edge uv of the network,
  //       returns the number of edges represented by this edge
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
    // NOTE: we're storing pointers to DPEntries so that we can store the 'best' entry consistently over many hashtable inserts
    using DPTable = mstd::vector_hash<DPEntry, DP_Entry_Hash>;
    //using WeakComps = mstd::DisjointSetForest<NodeDesc>;
    
  protected:
    Network& N;
    DPTable dp_table;
    [[ no_unique_address ]] DegreeExtracter degrees;


    using Degrees = std::remove_reference_t<decltype(degrees(NoNode))>;
    using Degree = typename Degrees::first_type;

    // ----------------------- weak component stuff -----------------------
    // to merge in-degrees in the WeakComps DisjointSetForest:
    //    when an item is merged into a weak component, then the indeg-outdeg of that component increases by the indeg-outdeg of the new node
    using DegreePayloadMerge = decltype([](int32_t& x, int32_t& y){ x += y; y = 0; });

    // the WeakComps DSF stores as payload the indeg - outdeg of the all nodes/components
    struct WeakComps: public mstd::DisjointSetForest<NodeDesc, int32_t, DegreePayloadMerge>{
      using Parent = mstd::DisjointSetForest<NodeDesc, int32_t, DegreePayloadMerge>;

      // compute the wealy-connected components in the given set of nodes
      template<NodeIterableType Nodes>
      void from_nodes(const Nodes& nodes) {
        Parent::clear();
        add_roots(nodes);
      }

      // add a few more nodes to the given DisjointSetForest
      // NOTE: this assumes that nodes + *this is downwards closed
      template<NodeIterableType Nodes>
      void add_roots(Nodes&& nodes, auto&& degrees) {
        for(const NodeDesc u: nodes) {
          const auto [indeg, outdeg] = degrees(u);
          Parent::emplace_set(u, int32_t(indeg) - int32_t(outdeg));
        }
        for(const NodeDesc u: nodes)
          for(const NodeDesc v: Network::children(u)){
            assert(Parent::contains(v)); // check if we have a downwards-closed set
            Parent::merge_sets_keep_order(u, v);
          }
      }

      size_t num_components() const { return Parent::set_count(); }
    };


    // ------------------------ query stuff -----------------------------

    template<NodeIterableType Nodes>
    struct Query {
      // nodes of the downwards-closed set X are to be sorted into 4 categories:
      // |--------------------- nodes -------------------------|
      // |--non_roots------|--------------roots----------------|
      //                   |-----------non_fix---------|--fix--|
      //                   |--non-raising--|--raising--|
      //
      // 1. we always put the reticulations (fix roots) and sw-raising nodes last in the extension
      // 2. then, we branch on the sw-raising roots, if there are any
      // 2. only if there are no sw-raising roots, we branch on which non-raising root is "last"
      //
      // NOTE: the roots are an anti-chain, so we can permute them arbitrary without violating topological order
      Nodes nodes;
      size_t non_roots; 
      size_t all_but_fixed_roots; 
      size_t all_but_fixed_or_raising; 
      size_t hash;
      WeakComps comps;
      [[ no_unique_address ]] DegreeExtracter degrees;

      // --------------------------- querying -----------------------------------
      size_t num_components() const { return comps.num_components(); }

      auto make_entry() { return DPEntry{nodes, hash}; }
      auto make_entry() && { return DPEntry{std::move(nodes), hash}; }

      // --------------------------- construction -----------------------------------

      template<class... Args>
      Query(const Network& N, Args&&... args):
        Query(N.nodes_postorder().template to_container<Nodes>(), N.num_nodes() - 1, 0, std::forward<Args>(args)...)
      {
        if(N.num_roots() != 1) throw std::logic_error("unimplemented: cannot deal with multi-root networks yet");
      }

      template<NodeIterableType _Nodes, class DEInit = DegreeExtracter>
      Query(_Nodes&& _nodes, const size_t _non_roots, const size_t _hash = 0, DEInit&& de_init = DegreeExtracter{}):
        nodes(std::forward<_Nodes>(_nodes)),
        non_roots(_non_roots),
        all_but_fixed_roots(_nodes.size()),
        all_but_fixed_or_raising(_nodes.size()),
        hash(_hash == 0 ? DPEntry::hash(nodes) : _hash),
        degrees(std::forward<DEInit>(de_init))
      {}

      template<NodeIterableType _Nodes> requires (!std::is_same_v<_Nodes, Nodes>)
      Query(const Query<_Nodes>& Q):
        nodes(Q.nodes.begin(), Q.nodes.end()),
        non_roots(Q.non_roots),
        all_but_fixed_roots(Q.all_but_fixed_roots),
        all_but_fixed_or_raising(Q.all_but_fixed_or_raising),
        hash(Q.hash),
        degrees(Q.degrees)
      {}

      // --------------------------- moving nodes around -----------------------------------

      // move new_roots to the end of the given area and modify pointers
      template<NodeIterableType _Nodes>
      void move_to_area(const _Nodes& to_move, size_t& area_start, size_t i = 0) {
        if(!to_move.empty()) {
          while(i < area_start) {
            size_t j = i;
            while(j < area_start) {
              if(test(to_move, nodes.at(j))) {
                ++j;
              } else break;
            }
            // rotate the children into the given area
            std::rotate(nodes.begin() + i, nodes.begin() + j, nodes.begin() + area_start);
            // update the area start
            area_start -= j - i;
            i = j + 1;
          }
        }
      }

      template<NodeIterableType _Nodes>
      void move_to_roots(const _Nodes& new_roots) { move_to_area(new_roots, non_roots); } // the root area starts at index 'non_roots'
      
      // --------------------------- init -----------------------------------

      void init() {
        // step 1: find connected components when we remove the roots
        comps.from_nodes(nodes | std::views::take(non_roots));
        // add the roots as singletons to the components (this enables use of "known_parents()"), we'll add their connections later
        for(const NodeDesc r: nodes | std::views::drop(non_roots)) {
          const auto [r_indeg, r_outdeg] = degrees(r);
          comps.emplace_set(r, int32_t(r_indeg) - int32_t(r_outdeg));
        }
        DEBUG4(std::cout << "computed " << comps.set_count() << " weak components: "<< comps << "\n");

        // step 2: find all roots with outgoing weight <= incoming weight
        fix_non_tree_roots();

        // since we removed some roots to get here, some raising roots may now be non-raising
        all_but_fixed_or_raising = all_but_fixed_roots; 

        // step 3: for each sw-raising root, replace it with its children
        compute_sw_raising();
      }

      // while there is a root with out-weight <= in-weight, move its unique child up into the root-section of ex and modify the non_roots counter
      void fix_non_tree_roots() {
        NodeSet fix_roots, reti_children, new_roots;
        DEBUG4(std::cout << "fixing roots among "<< (nodes | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots))<<"\n");
        size_t i = all_but_fixed_roots;
        while(i > non_roots) {
          fix_roots.clear();
          new_roots.clear();
          while(i > non_roots) {
            NodeDesc& u = nodes.at(--i);
            const auto [indeg, outdeg] = degrees(u);
            if(indeg >= outdeg) {
              DEBUG4(std::cout << "fixing root "<<u<<" with indeg "<<indeg<<" & outdeg "<<outdeg<<"\n");
              // move u to the fixed roots
              append(fix_roots, u);
              check_children_for_new_roots(u, new_roots);
            }
          }
          DEBUG4(std::cout << "marking "<< fix_roots << " fixed\n");
          move_to_area(fix_roots, all_but_fixed_roots, non_roots);
          DEBUG4(std::cout << "marking "<< new_roots << " as new roots\n");
          move_to_roots(new_roots);
          DEBUG4(std::cout << "i = "<<i<<" and the query is now:\n" << *this << "\n");
        }
      }

      
      // --------------------------- sw-raising nodes -----------------------------------

      // decide if a root u of a downwards-closed set X is sw-raising among the roots
      // NOTE: a root u is sw-raising   iff   sw(u) > maximum in-degree of the weak-components of the children v of u
      //      sw(u) = [u's in-degree] - [u's out-degree] + [sum_{weak component C of a child v of u} in-degree(C(v))] 
      //
      template<NodeIterableType _Nodes>
      bool is_sw_raising(const NodeDesc u, const _Nodes& roots) {
        // step 1: compute the weak components of the children of u, with a payload to store the sum of in-degrees
        // NOTE: the components will be identified by their representative in 'comps', so only representatives of 'comps' are going to be stored in here
        WeakComps comps_of_children;

        // step 1: setup comps_of_children for all children of roots except u
        for(const NodeDesc r: roots) if(r != u) {
          assert(!contains(comps, r)); // sanity check
          // add r's children with their components to the coomps_of_children
          for(const NodeDesc v: Network::children(r)) {
            // recall: the payload of 'comps' gives the in-degree of the weak component
            const auto& C = comps.set_of(v);
            // in the comps_of_children map, add the component of v with its representative (in comps)
            auto& C_set = comps_of_children.emplace_set(C.representative, C.payload).first->second;
            // also add v to point to C so other roots can merge components later (note: v is added with payload 0 since its degrees are counted in C already)
            auto& v_set = comps_of_children.emplace_set(v, 0).first->second;
            if(C_set != v_set)
              comps_of_children.merge_sets_keep_order(C_set, v_set);
            // finally merge r into the weak component of v
            comps_of_children.merge_sets_keep_order(C_set, r);
          }
        }

        // step 2: gather the in-degrees of child-components of u after having added all other roots (which possibly merged child-components of u)
        const auto [u_indeg, u_outdeg] = degrees(u);
        size_t max_comp_indeg = 0;
        int32_t u_sw = int32_t(u_indeg) - int32_t(u_outdeg);
        NodeSet seen;
        for(const NodeDesc v: Network::children(u)){
          // recall: the payload of 'comps' gives the in-degree of the weak component
          const auto& C = comps.set_of(v);
          // if we haven't seen the weak component of v yet...
          if(seen.emplace(C.representative).second) {
            // record its in-degree
            const size_t C_indeg = C.payload;
            u_sw += C_indeg;
            max_comp_indeg = std::max(max_comp_indeg, C_indeg);
          }
        }

        // step 3: compare sw(u) with indegrees of components
        return u_sw > max_comp_indeg;
      }

      void compute_sw_raising() {
        NodeSet sw_raising;
        DEBUG4(std::cout << "fixing roots among "<< (nodes | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots))<<"\n");
        for(size_t i = all_but_fixed_roots; i > non_roots;) {
          NodeDesc& u = nodes.at(--i);
          if(is_sw_raising(u, nodes | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots))){
            DEBUG4(std::cout << "marking sw-raising "<<u<<"\n");
            append(sw_raising, u);
          }
        }
        move_to_area(sw_raising, all_but_fixed_or_raising, non_roots);
        DEBUG4(std::cout << "sw-raising roots "<< (nodes | std::views::take(all_but_fixed_roots) | std::views::drop(all_but_fixed_or_raising)) << "\n");
      }

      // --------------------------- checking if children will be roots -----------------------------------

      // return the number of parents of u in X
      size_t known_parents(const NodeDesc u) {
        assert(comps.size() >= all_but_fixed_roots);
        return std::ranges::count_if(Network::parents(u), pred::ContainmentPredicate{comps});
        // [&](const NodeDesc p){ return test(comps, p)}
      }

      // each of old_root's children may become a root when old_root is removed; if so, add the child to 'new_roots'
      void check_children_for_new_roots(const NodeDesc old_root, auto& new_roots) {
        for(const NodeDesc child: Network::children(old_root))
          if((Network::in_degree(child) == 1) || (known_parents(child) == 1))
            append(new_roots, child);
      }

      size_t check_children_for_new_roots(const NodeDesc old_root) {
        NodeVec new_roots;
        check_children_for_new_roots(old_root, new_roots);
        const size_t result = new_roots.size();
        move_to_roots(new_roots);
        return result;
      }

      // --------------------------- main query preparation -----------------------------------
      
      // prepare a query of size query_size; by default the query contains everything but the fixed roots and the last non-fix root
      // return the prepared query
      auto prepare_query(const size_t root_pos) const {
        const size_t query_size = all_but_fixed_roots - 1;
        assert(query_size < nodes.size());
        // step 1: swap the root out of the query range
        NodeDesc& root = nodes.at(root_pos);

        if(root_pos < query_size)
          std::swap(root, nodes.at(query_size));

        // step 2: find out which children of the root become roots themselves (when removing root) and swap them into the root area of the next query
        const size_t new_non_roots = non_roots - check_children_for_new_roots(root);
        // compute the new hash by combining the old hash with the hash of everything NOT in the new query
        const size_t new_hash = DPEntry::hash(std::span{nodes.begin() + query_size, nodes.end()}, hash);
        // construct and return the new query
        return Query<std::span<NodeDesc>>(std::span{nodes.begin(), query_size}, new_non_roots, new_hash);
      }

      /*
      // get a span of nodes between which we are going to branch
      auto get_branching_nodes() {
        return std::span{nodes.begin() + non_roots, nodes.begin() + all_but_fixed_or_raising};
      }
      */

      // --------------------------- component-map stuff -----------------------------------

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

      auto get_comp_map_except_fixed_roots() const { return get_component_map(nodes | std::views::take(all_but_fixed_roots), non_roots, comps); }

      void add_non_fixed_roots_to_comps() {
        DEBUG5(std::cout << "adding the remaining non-fixed roots " << (ex | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots)) << " to the weak-components\n");
        // next, check if X is weakly connected and, if not, recurse for the weak components
        comps.add_roots(nodes | std::views::take(all_but_fixed_roots) | std::views::drop(non_roots));
      }

      friend std::ostream& operator<<(std::ostream& os, const Query& Q) {
        os << "\t"<< Q.nodes <<"\n";
        os << Q.non_roots<<" non-roots\t"<< (Q.nodes | std::views::take(Q.non_roots)) << "\n";
        os << "and roots\t"<< (Q.nodes | std::views::drop(Q.non_roots)) << "\n";
        return os;
      }
    };

    /*
    struct EntryInfo {
      size_t implied_sw; // this is not the sw of the entry, but the sw of the Query (that is, entry with the selected root appended at the end)
      size_t hash;
    };
    */

    const auto recurse_for(Query<Extension>& Q, const size_t root_pos) {
      DEBUG4(std::cout << "recursing for root at pos "<< root_pos <<"\n");
      DEBUG4(std::cout << "recursing for "<< Q.nodes.at(root_pos) <<"\n");

      const NodeDesc rt = Q.nodes.at(root_pos);
      
      // prepare the query: shift node at root_pos to the last position and test if its children are now roots
      const auto tmp = Q.prepare_query(root_pos);

      DEBUG4(std::cout << "recursing for "<< tmp <<"\n");
      const DPEntry& opt_answer = query(tmp);
      
      DEBUG4(std::cout << "prefix "<< opt_answer <<" is optimal for "<<Q.nodes<<"\n");

      // copy the dynamic scanwidth of the entry
      auto [sw, ds] = opt_answer.get_dynamic_scanwidth();
      // add the root at the end
      sw = std::max(sw, ds.update_sw(rt));

      return std::pair{&opt_answer, sw};
    }

  public:

    ScanwidthDP2(Network& _N): N{_N} {}

    const DPEntry& emplace_in_table(Query<Extension>&& Q) {
      return dp_table.emplace(Q.make_entry()).first;
    }

    auto dp_entry_by_hash(const size_t hash) const {
      return dp_table.find(DPEntry{NodeVec{}, hash});
    }

    const DPEntry& query() {
      Query Q{N};
      Q.init();
      return query(Q);
    }

    // query a downwards-closed list X of nodes
    // NOTE: we assume that the roots of the set are the last items in the set
    template<NodeIterableType Nodes>
    const DPEntry& query(const Query<Nodes>& Q) {
      DEBUG4(std::cout << "querying "<< Q << "\n");

      auto iter = dp_entry_by_hash(Q.hash);
      if(iter == dp_table.end()) {
        // if the entry didn't exist before, we will compute it from previous entries
        DEBUG4(std::cout << "no entry for this query yet...\n");
        // make a copy of the query (we'll have to make a copy of the nodes anyways for the DPEntry into which we'll move them later)
        // NOTE: this will NOT copy the connected components since we'll need to recompute them anyways; we'll do so in 2 stages
        Query tmp{Q};
        tmp.init();
        // if all (but one) nodes in tmp are roots, then the order doesn't matter and we can just compute its scanwidth
        if(tmp.non_roots > 1) {
          // compute connected components stage 2: add non-fix roots
          tmp.add_non_fixed_roots_to_comps();
          if(tmp.num_components() == 1) {
            DEBUG4(std::cout << "everything is weakly connected, so we'll recurse for all "<<tmp.all_but_fixed_roots - tmp.non_roots<<" non-fixed-roots: ");
            DEBUG4(std::cout << (tmp.nodes | std::views::take(tmp.all_but_fixed_roots) | std::views::drop(tmp.non_roots)) <<"\n");

            // for each root u of the downwards-closed set X in the network, recurse for X-u and add u in the end of the resulting extension
            const size_t last_root_idx = tmp.all_but_fixed_roots - 1;
            const size_t non_roots = tmp.non_roots;
            assert(non_roots <= last_root_idx);
            const size_t non_fixed_roots = last_root_idx - non_roots;
                      
            // for the first recursive call, there is no need to copy anything
            auto best = recurse_for(tmp, last_root_idx);
            // keep track of the best entry together with its scanwidth
#error "this reads very weirdly, why can we just replace tmp's prefix with the saved DPEntry's?"
            if(non_fixed_roots > 1){
              // for each non-fixed root, make that root last in tmp and recurse
              // keep the best entry among all computed ones
              for(size_t i = last_root_idx; i != non_roots;) {
                const auto entry = recurse_for(tmp, --i);
                // each entry implies a scanwidth for tmp
                const size_t tmp_sw = tmp.get_scanwidth();
                if(entry.get_scanwidth() < best.first)
                  best = std::pair(entry.get_scanwidth(), entry.hash());
              }
              const auto best_entry_iter = dp_entry_by_hash(best.second);
              assert(best_entry_iter != dp_table.end());
              tmp.replace_prefix(*best_entry_iter);
            } else tmp.replace_prefix(first_result);
          } else { // X is not weakly connected, so recurse for each connected component separately
            auto resultp = std::make_unique<DPEntry>();
            DPEntry& result = *resultp;
            for(auto& [u, C_pair]: tmp.get_comp_map_except_fixed()) {
              auto& C = C_pair.first;
              const size_t C_non_root = C_pair.second;
              DEBUG4(std::cout << "querying connected component "<<C<<" with roots "<<(C | std::views::drop(C_non_root))<<"\n");
              const DPEntry& C_entry = query(std::move(C), C_non_root);
              append(result.nodes, C_entry.nodes);
            }
            append(result.nodes, tmp.nodes | std::views::drop(tmp.all_but_fixed_roots));
            result.recompute_sw();
            iter = dp_table.emplace(resultp).first;
            return iter->get();
          }
        } else tmp.recompute_sw();
        // compact tmp down to save storage
        tmp.nodes.shrink_to_fit();
        // move the nodes into the DP-entry
        iter = dp_table.emplace(std::make_unique<DPEntry>(std::move(tmp.nodes), tmp.hash).first;
      }
      return iter->get();
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
