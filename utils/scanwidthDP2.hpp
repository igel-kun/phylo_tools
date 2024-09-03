
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
  /*
  struct DP_Entry_Hash {
    static constexpr mstd::XOR_hash<Extension> Hasher{};
    
    template<class Entry> requires (!mstd::IterableType<Entry>)
    size_t operator()(const std::unique_ptr<Entry>& entry) const { return Hasher(*entry); }
    
    template<NodeIterableType Nodes>
    size_t operator()(const Nodes& nodes) const { return Hasher(nodes); }
  };
  */



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
    using DPTable = mstd::vector_hash<DPEntry>;
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

      // add a few more nodes to the given DisjointSetForest
      // NOTE: this assumes that nodes + *this is downwards closed
      void add_nodes(const NodeSpan nodes, auto&& degrees, bool with_children = true) {
        for(const NodeDesc u: nodes) {
          const auto [indeg, outdeg] = degrees(u);
          Parent::emplace_set(u, int32_t(indeg) - int32_t(outdeg));
        }
        if(with_children)
          add_out_edges(nodes);
      }

      void add_out_edges(const NodeSpan nodes) {
        for(const NodeDesc u: nodes)
          for(const NodeDesc v: Network::children(u)){
            assert(Parent::contains(v)); // check if we have a downwards-closed set
            Parent::merge_sets_keep_order(u, v);
          }
      }

      size_t num_components() const { return Parent::set_count(); }
    };


    // ------------------------ query stuff -----------------------------
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
    struct Query {
      NodeSpan nodes;
      size_t non_roots; 
      size_t all_but_fixed_roots; 
      size_t all_but_fixed_or_raising; 
      size_t hash;
      WeakComps comps;
      [[ no_unique_address ]] DegreeExtracter degrees;


      // --------------------------- construction -----------------------------------
      Query(const Query&) = delete; // don't copy queries since the weak-components are too large :/
      Query(Query&&) = default;

      template<class DEInit = DegreeExtracter>
      Query(NodeSpan _nodes, const size_t _non_roots, const size_t _hash = 0, DEInit&& de_init = DegreeExtracter{}):
        nodes(_nodes),
        non_roots(_non_roots),
        all_but_fixed_roots(_nodes.size()),
        all_but_fixed_or_raising(_nodes.size()),
        hash(_hash == 0 ? DPEntry::hash(nodes) : _hash),
        degrees(std::forward<DEInit>(de_init))
      { init(); }

      // --------------------------- sub-spans ----------------------------
      NodeSpan get_roots() const { return nodes.subspan(non_roots); }
      NodeSpan get_non_roots() const { return nodes.subspan(0, non_roots); }
      NodeSpan get_fixed_roots() const { return nodes.subspan(all_but_fixed_roots); }
      NodeSpan get_non_fixed_roots() const { return nodes.subspan(non_roots, all_but_fixed_roots - non_roots); }
      NodeSpan get_sw_raising_roots() const { return nodes.subspan(all_but_fixed_or_raising, all_but_fixed_roots - all_but_fixed_or_raising); }

      // --------------------------- moving nodes around -----------------------------------

      // move new_roots to the end of the given area and modify pointers
      template<NodeIterableType _Nodes>
      void move_to_area(const _Nodes& to_move, size_t& new_area, size_t old_area = 0) {
        if(not to_move.empty()) {
          while(old_area < new_area) {
            size_t j = old_area;
            while(j < new_area) {
              if(test(to_move, nodes[j])) {
                ++j;
              } else break;
            }
            // rotate the children into the given area
            std::rotate(nodes.begin() + old_area, nodes.begin() + j, nodes.begin() + new_area);
            // update the area starts
            new_area -= j - old_area;
            old_area = j + 1;
          }
        }
      }

      template<NodeIterableType _Nodes>
      void move_to_roots(const _Nodes& new_roots) { move_to_area(new_roots, non_roots); } // the root area starts at index 'non_roots'
      
      // --------------------------- init -----------------------------------
      void comps_init() {
        assert(comps.empty());
        comps.add_nodes(get_non_roots());
        // add the roots as singletons to the components (this enables use of "known_parents()"), we'll add their connections later
        comps.add_nodes(get_roots(), false);
        DEBUG4(std::cout << "computed " << comps.num_components() << " weak components: "<< comps << "\n");
      }

      void init() {
        // step 1: find connected components when we remove the roots
        comps_init();
#warning "TODO: detect if the network below us is a tree (+ some transitive arcs) and, if so, output a reverse topological order"
        // step 2: find all roots with outgoing weight <= incoming weight
        fix_non_tree_roots();

        // since we removed some roots to get here, some raising roots may now be non-raising
        all_but_fixed_or_raising = all_but_fixed_roots; 

        // step 3: for each sw-raising root, replace it with its children
        compute_sw_raising();
      }

      // while there is a root with out-weight <= in-weight, move its unique child up into the root-section of ex and modify the non_roots counter
      void fix_non_tree_roots() {
        NodeSet fix_roots, new_roots;
        DEBUG4(std::cout << "fixing roots among "<< get_non_fixed_roots()<<"\n");
        size_t i = all_but_fixed_roots;
        while(i > non_roots) {
          fix_roots.clear();
          new_roots.clear();
          while(i > non_roots) {
            const NodeDesc u = nodes[--i];
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

      // --------------------------- replacing prefix by DPEntry -----------------------------------
      
      void replace_prefix(const Extension& ex) {
        assert(ex.size() <= nodes.size());
        // ex should be a permutation of the prefix that we're replacing
        assert(std::ranges::is_permutation(ex, nodes.subspan(0, ex.size())));
        std::ranges::copy(ex, nodes.begin());
      }

      // --------------------------- sw-raising nodes -----------------------------------

      // decide if a root u of a downwards-closed set X is sw-raising among the roots
      // NOTE: a root u is sw-raising   iff   sw(u) > maximum in-degree of the weak-components of the children v of u
      //      sw(u) = [u's in-degree] - [u's out-degree] + [sum_{weak component C of a child v of u} in-degree(C(v))] 
      // NOTE: this needs 'comps' to be set up with all non-roots! This is why we add the nodes into comps in 2 stages
      bool is_sw_raising(const NodeDesc u, const NodeSpan roots) {
        // step 1: compute the weak components of the children of u, with a payload to store the sum of in-degrees
        // NOTE: the components will be identified by their representative in 'comps', so only representatives of 'comps' are going to be stored in here
        WeakComps child_comps;

        // step 1: setup child_comps for all children of roots except u
        for(const NodeDesc r: roots) if(r != u) {
          assert(!contains(comps, r)); // sanity check
          // add r's children with their components to the coomps_of_children
          for(const NodeDesc v: Network::children(r)) {
            // recall: the payload of 'comps' gives the in-degree of the weak component
            const auto& C = comps.set_of(v);
            // in the child_comps map, add the component of v with its representative (in comps)
            auto& C_set = child_comps.emplace_set(C.representative, C.payload).first->second;
            // also add v to point to C so other roots can merge components later (note: v is added with payload 0 since its degrees are counted in C already)
            auto& v_set = child_comps.emplace_set(v, 0).first->second;
            if(C_set != v_set)
              child_comps.merge_sets_keep_order(C_set, v_set);
            // finally merge r into the weak component of v
            child_comps.merge_sets_keep_order(C_set, r);
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
        NodeVec sw_raising;
        DEBUG4(std::cout << "fixing roots among "<< get_non_fixed_roots() <<"\n");
        for(size_t i = all_but_fixed_roots; i > non_roots;) {
          NodeDesc& u = nodes[--i];
          if(is_sw_raising(u, get_non_fixed_roots())) {
            DEBUG4(std::cout << "marking sw-raising "<<u<<"\n");
            append(sw_raising, u);
          }
        }
        move_to_area(sw_raising, all_but_fixed_or_raising, non_roots);
        DEBUG4(std::cout << "sw-raising roots "<< get_sw_raising_roots() << "\n");
      }

      // --------------------------- checking if children will be roots -----------------------------------

      // return the number of parents of u in X
      size_t num_known_parents(const NodeDesc u) {
        assert(comps.size() >= all_but_fixed_roots);
        return std::ranges::count_if(Network::parents(u), pred::ContainmentPredicate{comps});
      }

      // each of old_root's children may become a root when old_root is removed; if so, add the child to 'new_roots'
      void check_children_for_new_roots(const NodeDesc old_root, auto& new_roots) {
        for(const NodeDesc child: Network::children(old_root))
          if((Network::in_degree(child) == 1) || (num_known_parents(child) == 1))
            append(new_roots, child);
      }

      size_t mark_new_roots_below(const NodeDesc old_root) {
        NodeVec new_roots;
        check_children_for_new_roots(old_root, new_roots);
        const size_t result = new_roots.size();
        move_to_roots(new_roots);
        return result;
      }

      // --------------------------- main query preparation -----------------------------------
      
      // prepare a query of size query_size; by default the query contains everything but the fixed roots and the last non-fix root
      // return the prepared query
      // NOTE: the nodes of the new queries are a REFERENCE into the old query, so make sure the old query outlives the new!
      auto prepare_new_query(const size_t root_pos) {
        const size_t query_size = all_but_fixed_roots - 1;
        assert(query_size < nodes.size());
        // step 1: swap the root out of the query range
        NodeDesc& root = nodes[root_pos];

        if(root_pos < query_size)
          std::swap(root, nodes[query_size]);

        // step 2: find out which children of the root become roots themselves (when removing root) and swap them into the root area of the next query
        const size_t new_non_roots = non_roots - mark_new_roots_below(root);
        // compute the new hash by combining the old hash with the hash of everything NOT in the new query
        // NOTE: this assumes that the hash is XOR-based!!!
        auto hasher = DPEntry::Hasher;
        static_assert(decltype(hasher)::is_XOR_hashing);
        const size_t new_hash = hasher(nodes.subspan(query_size), hash);
        // construct and return the new query
        // NOTE: this also re-initializes the weak components and roots, since both of those may be very different in the new Query
        return Query(nodes.subspan(0, query_size), new_non_roots, new_hash, degrees);
      }

      // --------------------------- component-map stuff -----------------------------------

      // translate a DisjointSetForest into a mapping in which each representative is mapped to its weak component (a NodeVec) and the number of non-roots
      // NOTE: we keep the relative order of the nodes, so the non-root ones preceed the root ones
      // NOTE: this needs comps to be finalized (non-fixed roots with out-edges added)
      auto get_comp_map_except_fixed() const {
        using ComponentAndNum = std::pair<NodeVec, size_t>;
        using Output = NodeMap<ComponentAndNum>;

        // step 1: compute the non-root parts of the components
        Output out_map;
        for(const NodeDesc u: get_non_roots()) {
          const NodeDesc rep = comps.representative(u);
          append(out_map[rep].first, u);
        }
        // step 2: set the non-root sizes
        for(auto& [u, C]: out_map)
          C.second = C.first.size();
        // step 3: add the non-fixed roots
        for(const NodeDesc u: get_non_fixed_roots()) {
          const NodeDesc rep = comps.representative(u);
          append(out_map[rep].first, u);
        }
        assert(out_map.size() > 1);
        return out_map;
      }

      void add_non_fixed_roots_to_comps() {
        DEBUG5(std::cout << "adding the remaining non-fixed roots " << get_non_fixed_roots() << " to the weak-components\n");
        comps.add_out_edges(get_non_fixed_roots());
      }

      friend std::ostream& operator<<(std::ostream& os, const Query& Q) {
        os << "\t"<< Q.nodes <<"\n";
        os << Q.non_roots<<" non-roots\t"<< Q.get_non_roots() << "\n";
        os << "and roots\t"<< Q.get_roots() << "\n";
        return os;
      }
    };

    // makes a recursive query for the downwards-closed set that has the root at root_pos last
    // returns the hash of the DPEntry, as well as the scanwidth
    auto recurse_for(Query& Q, const size_t root_pos) {
      DEBUG4(std::cout << "recursing for root at pos "<< root_pos <<"\n");
      DEBUG4(std::cout << "recursing for "<< Q.nodes[root_pos] <<"\n");

      // save the root
      const NodeDesc rt = Q.nodes[root_pos];
      // prepare the query: shift node at root_pos to the last position and test if its children are now roots

      const DPEntry& opt_answer = query(Q.prepare_new_query(root_pos));
      
      DEBUG4(std::cout << "prefix "<< opt_answer.get_ex() <<" is optimal for "<<Q.nodes<<"\n");

      // copy the dynamic scanwidth of the entry
      auto [sw, ds] = opt_answer.get_dynamic_scanwidth();
      // add the root at the end
      sw = std::max(sw, ds.update_sw(rt));

      return std::pair{opt_answer.hash(), sw};
    }

  public:

    ScanwidthDP2(Network& _N): N{_N} {}

    auto lookup_by_hash(const size_t hash) { return dp_table.emplace(hash); }

    // query a downwards-closed list X of nodes
    // NOTE: we assume that the roots of the set are the last items in the set
    const DPEntry& query(Query Q) {
      DEBUG4(std::cout << "querying "<< Q << "\n");

      // since the DPTable-lookup is by hash only, we don't have to have a filled vector for the lookup
      auto [iter, created] = lookup_by_hash(Q.hash);
      if(created) {
        DEBUG4(std::cout << "no entry for this query yet...\n");
        // if the entry didn't exist before, we will compute it from previous entries
        // to this end, we first set the extension in the entry to the correct set of nodes
        // all queries will be made via spans into this extension
        // NOTE: this means that this table entry is present, but not yet in a correct state -- this has to be taken into account for parallel execution!

        // COPY the nodes in Q's span into the new DPEntry
        iter->ex = Q.nodes;
        // update the nodes-span of Q to point into the new DPEntry
        Q.nodes = iter->ex;
        // the hash should not change since we initialized it with Q.hash
        assert(iter->hash_correct());
        
        // if all (but one) nodes in Q are roots, then the order doesn't matter and we can just compute its scanwidth
        if(Q.non_roots > 1) {
          // finalize weak components by adding non-fix roots (has been deferred up to now, but we need it to determine the number of components)
          Q.add_non_fixed_roots_to_comps();
          // if X has multiple connected components, then we recurse for each one individually
          if(Q.comps.num_components() == 1) {
            DEBUG4(std::cout << "everything is weakly connected, so we'll recurse for all "<<Q.all_but_fixed_roots - Q.non_roots<<" non-fixed-roots: ");
            DEBUG4(std::cout << Q.get_non_fixed_roots() << '\n');

            assert(Q.all_but_fixed_roots > Q.non_roots); // this fails only if all roots are fix which, by design, should never happen
            // for each root u of the downwards-closed set X in the network, recurse for X-u and add u in the end of the resulting extension
            const size_t last_root_idx = Q.all_but_fixed_roots - 1;
            const size_t non_roots = Q.non_roots;
            // we consider the last node fix, so the non-fix roots decrease by 1
            //const size_t non_fixed_roots = last_root_idx - non_roots;
                      
            // for the first recursive call, there is no need to copy anything
            auto best = recurse_for(Q, last_root_idx);
            // keep track of the best entry together with its scanwidth
            // for each non-fixed root, make that root last in Q and recurse -- keep the best entry among all computed ones
            for(size_t i = last_root_idx; i != non_roots;) {
              auto entry = recurse_for(Q, --i);
              if(entry.second < best.second)
                best = std::move(entry);
            }
            // overwrite the prefix of *iter (represented by Q.nodes) by the extension of the best entry
            const auto [best_iter, success] = lookup_by_hash(best.first);
            assert(not success); // the hash of best should still be in the DPTable, please
            iter->replace_prefix(*best_iter);
          } else { // X is not weakly connected, so recurse for each connected component separately
            DPEntry tmp{iter->hash()};
            for(auto& [u, C_pair]: Q.get_comp_map_except_fixed()) {
              auto& [C, C_non_root] = C_pair;
              DEBUG4(std::cout << "querying connected component "<<C<<" with roots "<<(C | std::views::drop(C_non_root))<<"\n");
              const DPEntry& C_entry = query(Query{NodeSpan{C}, C_non_root});
              append(tmp.ex, C_entry.ex);
              DEBUG4(std::cout << "resulting extension is now: " << tmp.ex <<'\n');
            }
            // add the fixed roots back in, and recompute scanwidth of the whole thing
            append(tmp.ex, Q.get_fixed_roots());
            tmp.recompute_sw();
            // the hash of tmp should now be correct
            assert(tmp.hash_correct());
            *iter = std::move(tmp);
          }
        } else iter->recompute_sw();
      }
      return *iter;
    }

    // NOTE: you can pass either an extension or a callable to register nodes in order
    //       if you pass any iterable, then we will append each node's NodeData to it in order
    template<bool include_root = false, class RegisterNode>
    void compute_min_sw_extension_no_bridges(RegisterNode&& _register_node) {
      DEBUG4(std::cout << "computing scanwidth of block:\n"<<ExtendedDisplay(N)<<" (low mem: "<< low_memory_version <<")\n");
      if(N.num_nodes() > 1){
        if(N.num_roots() != 1) throw mstd::Unimplemented{"cannot deal with multiple roots yet"};
        //const NodeVec N_nodes = N.nodes_postorder().template to_container<NodeVec>();
        NodeVec N_nodes(N.nodes_postorder().template to_container<NodeVec>());
        const DPEntry& opt_sol = query(Query{NodeSpan{N_nodes}, N_nodes.size() - 1});
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
