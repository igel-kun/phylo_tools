
// this file contains dynamic programming approaches for hardwired/softwired/parental parsimony on networks
// with respect to the node-scanwidth of a given layout
// NOTE: in the worst case, this is equal to the reticulation number of the network (for a trivial layout) plus one

#pragma once

#include <array>
#include "tight_int.hpp"
#include "singleton.hpp"
#include "static_capacity_vector.hpp"
#include "extension.hpp"

namespace PT {
  

  // ==================== HARDWIRED PARSIMONY ========================
  // the DP is indexed by a mapping of character-states to parents p of incomplete reticulations r
  //    s.t. r is 'to the right' of p in a given linear extension
  template<class Character>
  struct HW_DP_Bag {
    using Nodes = NodeVec;
    using Index = std::vector<Character>;

    NodeMap<size_t> node_to_index;
    NodeVec index_to_node;
    
    // an entry maps character states for the cut-nodes to a cost
    std::unordered_map<Index, size_t> costs;

    HW_DP_Bag() = default;

    void set_state_in(const NodeDesc u, Index& index, const Character& c) const {
      assert(test(node_to_index, u));
      const size_t i = node_to_index.at(u);
      index[i] = c;
    }

    size_t size() const { return index_to_node.size(); }

    // constructor to set up node_to_index and index_to_node
    template<NodeContainerType SW_Nodes>
    HW_DP_Bag(const SW_Nodes& sw_nodes){
      index_to_node.reserve(sw_nodes.size());
      for(const NodeDesc u: sw_nodes) {
        append(node_to_index, u, index_to_node.size());
        append(index_to_node, u);
      }
    }
  };

  template<class T>
  struct _CharacterSetRefOf {};
  template<NodeMapType GetCharacterState>
  struct _CharacterSetRefOf<GetCharacterState> {
    using type = mstd::mapped_type_of_t<GetCharacterState>&;
    using const_type = const mstd::mapped_type_of_t<GetCharacterState>&;
  };
  template<NodeFunctionType GetCharacterState>
  struct _CharacterSetRefOf<GetCharacterState> {
    using type = std::invoke_result_t<GetCharacterState, NodeDesc>;
    using const_type = std::invoke_result_t<const GetCharacterState, NodeDesc>;
  };
  template<class GetCharacterState>
  using CharacterSetRefOf = typename _CharacterSetRefOf<GetCharacterState>::type;
  template<class GetCharacterState>
  using CharacterSetConstRefOf = typename _CharacterSetRefOf<GetCharacterState>::const_type;


  template<PhylogenyType Phylo, class GetCharacterState>
    requires (mstd::IterableType<CharacterSetRefOf<GetCharacterState>>)
  struct Parsimony_HW_DP {
    using PlainPhylo = std::remove_cvref_t<Phylo>;
    using CharSetRef = CharacterSetRefOf<GetCharacterState>;
    using CharSetConstRef = CharacterSetConstRefOf<GetCharacterState>;
    using Char = mstd::value_type_of_t<CharSetRef>;
    using Bag = HW_DP_Bag<Char>;
    using Index = typename Bag::Index;
    using ExtIndex = size_t;

    struct CharHistogram {
      std::unordered_map<Char, size_t> data;
      size_t num_items = 0;

      CharHistogram(const Index& index): data{}, num_items{index.size()}
      { for(const auto& c: index) ++(data[c]); }

      size_t lookup(const Char c) const { return_map_lookup(data, c, 0); }

      size_t cost_of_state(const Char state) const { return num_items - lookup(state); }
    };

    Phylo N;
    Extension ext;
    GetCharacterState cs;
    size_t num_states;

  protected:
    mutable NodeMap<Bag> dp_table;
    NodeMap<NodeSet> highest_children_of;

  public:
    template<class PhyloInit, class ExtInit, class CSInit>
    Parsimony_HW_DP(PhyloInit&& _N, ExtInit&& _ext, CSInit&& _cs, size_t _num_states):
      N{std::forward<PhyloInit>(_N)}, ext{std::forward<ExtInit>(_ext)}, cs{std::forward<CSInit>(_cs)}, num_states{_num_states}
    { create_all_bags(); }

  protected:
    void create_all_bags() {
      std::cout << "\n\n============== Hardwired Parsimony =====================\n"<<ExtendedDisplay(N) << "\n";
      const auto sw_nodes = ext.template get_sw_nodes_map<PlainPhylo>(highest_children_of);
      for(const NodeDesc u: ext) {
        append(dp_table, u, sw_nodes.at(u));
        std::cout << "created bag for "<<u<<": \t"<<dp_table.at(u).index_to_node<<"\n";
      }
    }

    bool has_states(const NodeDesc u) const { return mstd::test(cs, u); }
    CharSetConstRef states_of(const NodeDesc u) const { return mstd::lookup(cs, u); }

    void for_each_state_of(const NodeDesc u, auto&& f) const { 
      if(has_states(u)) {
        for(const auto& c: states_of(u)) f(c);
      } else for(size_t i = 0; i != num_states; ++i) f(i);
    }

    // remove all nodes from the index that do not contribute to the node-scanwidth of the given bag
    Index prepare_index(const Index& index, const Bag& parent_bag, const Bag& child_bag) const {
      Index result;
      result.reserve(child_bag.size());
      for(const NodeDesc x: child_bag.index_to_node) {
        std::cout << "node "<<x<<" in bag "<< child_bag.index_to_node<<"\n";
        const auto iter = parent_bag.node_to_index.find(x);
        if(iter != parent_bag.node_to_index.end()) {
          const size_t x_index_in_parent = iter->second;
          result.emplace_back(index.at(x_index_in_parent));
        } else result.emplace_back();
      }
      return result;
    }


  public:

    size_t score_for(const NodeDesc u, const Index& index) const {
      std::cout << "node "<<u<<" - checking index "<<index<<"\n";
      auto& u_bag = dp_table[u];
      auto [iter, success] = append(u_bag.costs, index);
      size_t& cost = iter->second;
      if(success) {
        // index was not found in the table, so compute its cost using the previous table entry
        // step 1: compute the histogram of character-states for the parents of u
        const CharHistogram hist{index};
        // step 2: add u with all possible character states
        size_t sub_cost = std::numeric_limits<size_t>::max();
        for_each_state_of(u, [&](const Char state){
            const auto hc_iter = highest_children_of.find(u);
            size_t children_cost = 0;
            if(hc_iter != highest_children_of.end()) {
              for(const NodeDesc v: hc_iter->second) {
                const auto& v_bag = dp_table.at(v);
                // step 1: translate the index for v's bag, leaving a spot for u
                Index sub_index = prepare_index(index, u_bag, v_bag);
                const size_t u_index = v_bag.node_to_index.at(u);

                std::cout << "setting state "<<state<<" for "<<u<<":\n";
                sub_index[u_index] = state;
                std::cout << "recursive call for "<< v <<" and index "<<sub_index<<"\n";
                children_cost += score_for(v, sub_index);
              }
            }
            // step 3: add the cost between u and its parents, depending on u's new state
            const size_t parents_cost = hist.cost_of_state(state);
            sub_cost = std::min(sub_cost, children_cost + parents_cost);
        });
        cost = sub_cost;
      }
      std::cout << "\tfound that cost["<<u<<", "<<index<<"] = "<<cost<<"\n";
      return cost;
    }

    size_t score() const {
      std::cout << "computing parsimony score via the extension "<<ext<<"\n";
      return score_for(ext.back(), Index{});
    }
  };


  template<PhylogenyType _Phylo, class _CharacterStates>
  auto make_parsimony_HW_DP(_Phylo&& N, const Extension& ext, _CharacterStates&& cs, const size_t num_states) {
    // if phylo/character_states are passed-in by ralue-reference, then we will store our own 'copy'
    constexpr bool own_phylo = std::is_rvalue_reference_v<_Phylo&&>;
    constexpr bool own_cs = std::is_rvalue_reference_v<_CharacterStates&&>;
    using PlainPhylo = std::remove_reference_t<_Phylo>;
    using PlainCS = std::remove_reference_t<_CharacterStates>;
    using Phylo = std::conditional_t<own_phylo, PlainPhylo, PlainPhylo&>;
    using CS = std::conditional_t<own_cs, PlainCS, PlainCS&>;

    return Parsimony_HW_DP<Phylo, CS>(std::forward<_Phylo>(N), ext, std::forward<_CharacterStates>(cs), num_states);
  }

}
