
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

  template<NodeMapType CharacterStates>
  using CharacterSetOf = std::conditional_t<mstd::ContainerType<mstd::mapped_type_of_t<CharacterStates>>,
        mstd::mapped_type_of_t<CharacterStates>, mstd::singleton_set<mstd::optional_by_invalid<mstd::mapped_type_of_t<CharacterStates>>>>;
  template<NodeMapType CharacterStates>
  using CharacterOf = mstd::value_type_of_t<CharacterSetOf<CharacterStates>>;

  template<PhylogenyType Phylo, NodeMapType CharacterStates>
  struct Parsimony_HW_DP {
    using PlainPhylo = std::remove_cvref_t<Phylo>;
    using CharMapItem = mstd::mapped_type_of_t<CharacterStates>;
    using CharSet = CharacterSetOf<CharacterStates>;
    using Char = CharacterOf<CharacterStates>;
    using Bag = HW_DP_Bag<Char>;
    using Index = typename Bag::Index;
    using ExtIndex = size_t;
    using CharHistogram = std::unordered_map<Char, size_t>;

    static_assert(mstd::ArithmeticType<Char>);

    Phylo N;
    Extension ext;
    CharacterStates cs;
    size_t num_states;


    template<class PhyloInit, class ExtInit, class CSInit>
    Parsimony_HW_DP(PhyloInit&& _N, ExtInit&& _ext, CSInit&& _cs, size_t _num_states):
      N{std::forward<PhyloInit>(_N)}, ext{std::forward<ExtInit>(_ext)}, cs{std::forward<CSInit>(_cs)}, num_states{_num_states}
    {
      create_all_bags();
    }

    static CharHistogram make_histogram(const Index& index) {
      CharHistogram result;
      for(const auto& c: index)
        ++(result[c]);
      return result;
    }

  protected:
    mutable std::unordered_map<NodeDesc, Bag> dp_table = {};

    void create_all_bags() {
      const auto sw_nodes = ext.template sw_nodes_map<PlainPhylo>();
      for(const NodeDesc u: ext)
        append(dp_table, u, sw_nodes.at(u));
    }

    template<class States>
    void for_each_state(const States& states, auto&& f) const { 
      if constexpr (mstd::IterableType<States>) {
        for(const auto& c: states) f(c);
      } else f(states);
    }
    void for_each_state_of(const NodeDesc u, auto&& f) const { 
      const auto iter = cs.find(u);
      if(iter != cs.end()){
        for_each_state(iter->second, f);
      } else for(size_t i = 0; i != num_states; ++i) f(i);
    }

  public:

    size_t score_for(const ExtIndex i, const Index& index) const {
      const NodeDesc u = ext.at(i);
      assert(test(index, u));
      auto& bag = dp_table[u];
      auto [iter, success] = append(bag.costs, index);
      size_t& cost = iter->second;
      // index was not found in the table, so compute its cost using the previous table entry
      if(!node_of<Phylo>(u).is_leaf()) {
        assert(i > 0);
        if(success) {
          size_t sub_cost = std::numeric_limits<size_t>::max();
          const NodeDesc v = ext.at(i-1);
          const auto& prev_bag = dp_table.at(v);
          // step 1: remove all nodes from the index that do not contribute to the node-scanwidth of the previous entry
          Index sub_index;
          sub_index.reserve(prev_bag.size());
          for(const NodeDesc x: prev_bag.index_to_node) {
            if(x != u) {
              assert(test(bag.node_to_index, x));
              const size_t x_index = bag.node_to_index.at(x);
              assert(x_index < index.size());
              sub_index.emplace_back(index.at(x_index));
            } else sub_index.emplace_back();
          }
          // step 2: add u with all possible character states
          for_each_state_of(u, [&](const Char state){
              prev_bag.set_state_in(u, sub_index, state);
              size_t new_cost = score_for(i - 1, sub_index);
              for(const NodeDesc pu: node_of<Phylo>(u).parents()) {
                assert(test(bag.node_to_index, pu));
                const size_t pu_index = bag.node_to_index.at(pu);
                assert(pu_index < index.size());
                new_cost += (index.at(pu_index) != state);
              }
              sub_cost = std::min(sub_cost, new_cost);
            });
      
          cost = sub_cost;
        }
      } else {
        // if u is a leaf, then count for all nodes in the index how often their character appears and derive the cost from the max among them
        const auto hist = make_histogram(index);
        auto max = hist.begin();
        auto u_iter = cs.find(u);
        if(u_iter != cs.end()) {
          for_each_state(u_iter->second, [&](const Char& c){const auto h_it = hist.find(c); assert(h_it != hist.end()); if(h_it->second > max->second) max = h_it; });
        } else max = std::ranges::max_element(hist, [](const auto& lhs, const auto& rhs){ return lhs.second < rhs.second; });
        cost = index.size() - max->second;
      }
      return cost;
    }

    size_t score() const {
      return score_for(ext.size() - 1, Index{});
    }
  };


  template<PhylogenyType _Phylo, NodeMapType _CharacterStates>
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
