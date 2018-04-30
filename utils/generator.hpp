
#pragma once

#include "utils/iterable_bitset.hpp"

namespace TC{

  //! return the result of a coin flip whose 1-side has probability 'probability' of coming up
  inline bool toss_coin(const double probability = 0.5)
  {
    return ((double)rand() / RAND_MAX) <= probability;
  }
  //! return the result of a 0/1-die with 'good_sides' good sides among its 'sides' sides
  inline bool throw_bw_die(const double good_sides, const double sides)
  {
    return toss_coin(good_sides / sides);
  }
  //! return the result of throwing a die with 'sides' sides [0,sides-1]
  inline uint32_t throw_die(const uint32_t sides = 6)
  {
    return rand() % sides; // yadda yadda, it's not 100% uniform
  }
  //! draw k integers from [0,n-1]
  std::iterable_bitset draw(uint32_t k, const uint32_t n)
  {
    assert(k <= n);
    std::iterable_bitset result(n);
    while(k > 0)
      result.set_kth_unset(throw_die(k--));
    return result;
  }

  //! generate a random network allowing or not: reticulations, multiple labels
  template<class _Network>
  void generate_random_bw_network(Edgelist& el,
                                      NameVec& names,
                                      const uint32_t num_tree_nodes,
                                      const uint32_t num_reticulations,
                                      const uint32_t num_taxa,
                                      const float multilabel_density)
  {
    assert(multilabel_density >= 0   && multilabel_density < 1);
    
    const uint32_t num_internal = num_tree_nodes + num_reticulations;
    const uint32_t num_nodes = num_internal + num_taxa;
    const uint32_t num_edges = (num_taxa + 3 * num_internal - 1) / 2;
    const uint32_t reti_edges = num_reticulations * 2;
    assert(num_internal >= num_taxa - 1);
    
    std::iterable_bitset unsatisfied(num_nodes);

    uint32_t reti_count = 0;
    uint32_t leaf_count = 0;
    uint32_t tree_count = 0;
    for(uint32_t i = 0; i < num_nodes; ++i){
      const uint32_t num_unsatisfied = unsatisfied.count();
      // the new node i might be a reticulation if there are at least 2 unsatisfied nodes
      if((reti_count < num_reticulations) &&
          (num_unsatisfied > 1) &&
          throw_bw_die(num_reticulations - reti_count, num_nodes - i)){
        // node i is a reticulation
#warning continue here
      } else if((leaf_count < num_taxa) &&
                (num_unsatisfied > (i == num_nodes - 1 ? 0 : 1)) &&
                (throw_bw_die(num_leaves - leaf_count, num_nodes - i)){
        // node i is a leaf
#warning continue here

      } else {
        // node i is a tree vertex
#warning continue here
      }
    }

    uint32_t num_vertices = number_taxa;

    while(num_vertices < number_internal_nodes + number_taxa){
    }
    
  }

  //! simulate reticulate species evolution
  template<class _Network>
  void simulate_species_evolution(Edgelist& el, NameVec& names, const uint32_t number_taxa, const float recombination_rate)
  {
  }

  //! simulate reticulate gene evolution
  template<class _Network>
  void simulate_gene_evolution(Edgelist& el, NameVec& names, const uint32_t number_taxa, const float recombination_rate)
  {
  }

}
