
#pragma once

#include "utils/utils.hpp"
#include "utils/types.hpp"
#include "utils/iter_bitset.hpp"

namespace TC{

  //! return the result of a coin flip whose 1-side has probability 'probability' of coming up
  inline bool toss_coin(const double probability = 0.5)
  {
    return ((double)rand() / RAND_MAX) <= probability;
  }
  //! return the result of throwing a die with 'sides' sides [0,sides-1]
  inline uint32_t throw_die(const uint32_t sides = 6)
  {
    return rand() % sides; // yadda yadda, it's not 100% uniform
  }
  //! return the result of a 0/1-die with 'good_sides' good sides among its 'sides' sides
  inline bool throw_bw_die(const uint32_t good_sides, const uint32_t sides)
  {
    return throw_die(sides) < good_sides;
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

  std::string sequential_taxon_name(const uint32_t x)
  {
    if(x >= 26)
      return sequential_taxon_name(x/26) + (char)('a' + (x % 26));
    else
      return std::string("") + (char)('a' + x);
  }

  template<class Container>
  typename Container::iterator get_random_iterator(Container& c)
  {
    typename Container::iterator i = c.begin();
    for(uint32_t k = throw_die(c.size()); k > 0; --k) ++i;
    return i;
  }
  template<class Container>
  void decrease_or_remove(Container& c, const typename Container::iterator& it)
  {
    if(it->second == 1) c.erase(it); else --it->second;
  }

  //! generate a random network
  void generate_random_binary_edgelist(Edgelist& el,
                                       NameVec& names,
                                       const uint32_t num_tree_nodes,
                                       const uint32_t num_retis,
                                       const uint32_t num_leaves,
                                       const float multilabel_density)
  {
#warning implement multi-labels
    assert(multilabel_density >= 0   && multilabel_density < 1);
    
    const uint32_t num_internal = num_tree_nodes + num_retis;
    const uint32_t num_nodes = num_internal + num_leaves;
    //const uint32_t num_edges = (num_leaves + 3 * num_internal - 1) / 2;
    //const uint32_t reti_edges = num_retis * 2;
    assert(num_internal >= num_leaves - 1);
    
    std::unordered_map<uint32_t, unsigned char> dangling;

    uint32_t reti_count = 0;
    uint32_t tree_count = 0;
    names.resize(num_nodes);
    // initialize with a root node
    dangling[0] = 2;
    for(uint32_t i = 1; i < num_internal; ++i){
      
      const uint32_t num_unsatisfied = dangling.size();
      const auto parent_it = get_random_iterator(dangling);
      el.push_back({parent_it->first, i});
      decrease_or_remove(dangling, parent_it);
      DEBUG5(std::cout << " node #"<<i<<"\t- "<<reti_count <<" retis & "<<tree_count<<" tree nodes - ");
      DEBUG4(std::cout << "adding edge "<<el.back()<<std::endl);
      
      // the new node i might be a reticulation if there are at least 2 unsatisfied nodes
      if((reti_count < num_retis) &&
             (num_unsatisfied > 1) &&
             throw_bw_die(num_retis - reti_count, num_internal - i)){
        // node i is a reticulation
        // the second incoming edge is from a random unsatisfied node (except last_node)
        const auto dang_it = get_random_iterator(dangling);
        el.push_back({dang_it->first, i});
        decrease_or_remove(dangling, dang_it);
        dangling[i] = 1;
        ++reti_count;
      } else {
        if(tree_count == num_tree_nodes) throw std::logic_error("using too many tree vertices, this should not happen");
        // node i is a tree vertex
        dangling[i] = 2;
        ++tree_count;
      }
    }
    // satisfy all using the leaves
    for(uint32_t i = num_internal; i < num_nodes; ++i){
      const uint32_t parent = dangling.begin()->first;
      // WTF stl, why is there no front() / pop_front() for unordered_maps??? (see https://stackoverflow.com/questions/16981600/why-no-front-method-on-stdmap-and-other-associative-containers-from-the-stl)
      el.push_front({parent, i});
      decrease_or_remove(dangling, dangling.begin());
      names[i] = sequential_taxon_name(i - num_internal);
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
