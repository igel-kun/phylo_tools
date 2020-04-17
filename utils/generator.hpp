
#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "random.hpp"

namespace PT{

  //NOTE: in a binary network, we have n = t + r + l, but also l + r - 1 = t (together, n = 2t + 1 and n = 2l + 2r - 1)

  uint32_t l_from_nr(const uint32_t n, const uint32_t r)
  {
    if(n % 2 == 0) throw std::logic_error("binary networks must have an odd number of vertices");
    if(n < 2*r + 1) throw std::logic_error("need at least "+std::to_string(2*r+1)+" nodes (vs "+std::to_string(n)+" given) in a binary network with "+std::to_string(r)+" reticulations/leaves");
    return (n - 2*r + 1) / 2;
  }

  uint32_t n_from_rl(const uint32_t r, const uint32_t l)
  {
    if(l == 0) throw std::logic_error("networks should have leaves");
    return 2*r + 2*l - 1;
  }

  uint32_t r_from_nl(const uint32_t n, const uint32_t l)
  {
    return l_from_nr(n,l);
  }


  std::string sequential_taxon_name(const uint32_t x)
  {
    if(x >= 26)
      return sequential_taxon_name(x/26) + (char)('a' + (x % 26));
    else
      return std::string("") + (char)('a' + x);
  }


  //! generate a random network from number of: tree nodes, retis, and leaves
  template<class EdgeContainer = EdgeVec, class NameContainer = NameVec>
  void generate_random_binary_edgelist_trl(EdgeContainer& edges,
                                           NameContainer& names,
                                           const uint32_t num_tree_nodes,
                                           const uint32_t num_retis,
                                           const uint32_t num_leaves,
                                           const float multilabel_density = 0.0f)
  {
#warning implement multi-labels
    assert(multilabel_density >= 0   && multilabel_density < 1);

    if(num_leaves == 0) throw std::logic_error("cannot construct network without leaves");
    if(num_tree_nodes == 0) throw std::logic_error("cannot construct network without tree nodes");
    
    const uint32_t num_internal = num_tree_nodes + num_retis;
    const uint32_t num_nodes = num_internal + num_leaves;
    //const uint32_t num_edges = (num_leaves + 3 * num_internal - 1) / 2;
    //const uint32_t reti_edges = num_retis * 2;
    if(num_internal < num_leaves - 1) throw std::logic_error("there is no network with "+std::to_string(num_tree_nodes)+" tree nodes, "+std::to_string(num_retis)+" reticulations, and "+std::to_string(num_leaves)+" leaves");
    
    std::unordered_map<uint32_t, unsigned char> dangling;

    uint32_t reti_count = 0;
    uint32_t tree_count = 0;
    names.resize(num_nodes);
    // initialize with a root node
    dangling[0] = 2;
    for(uint32_t i = 1; i < num_internal; ++i){
      const uint32_t num_unsatisfied = dangling.size();
      const auto parent_it = get_random_iterator(dangling);
      edges.emplace_back(parent_it->first, i);
      const bool removed = !decrease_or_remove(dangling, parent_it);
      DEBUG5(std::cout << " node #"<<i<<"\t- "<<reti_count <<" retis & "<<tree_count<<" tree nodes - ");
      DEBUG4(std::cout << "adding edge "<<edges.back()<<std::endl);
      
      // the new node i might be a reticulation if there are at least 2 unsatisfied nodes
      if((reti_count < num_retis) &&
             (num_unsatisfied > 1) &&
             throw_bw_die(num_retis - reti_count, num_internal - i)){
        // node i is a reticulation
        // the second incoming edge is from a random unsatisfied node (except last_node)
        std::cout << "reti"<<std::endl;
        const auto dang_it = removed ? get_random_iterator(dangling) : get_random_iterator_except(dangling, parent_it);
        std::cout << "got "<<*dang_it<<std::endl;
        edges.emplace_back(dang_it->first, i);
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

      // WTF stl, why is there no front() / pop_front() for unordered_maps??? (see https://stackoverflow.com/questions/16981600/why-no-front-method-on-stdmap-and-other-associative-containers-from-the-stl)
      const uint32_t parent = dangling.begin()->first;
      
      edges.emplace_back(parent, i);
      decrease_or_remove(dangling, dangling.begin());
      names[i] = sequential_taxon_name(i - num_internal);
      
      DEBUG5(std::cout << " node #"<<i<<" is a leaf with name "<<names[i]<<" - adding edge "<<edges.back()<<std::endl);
    }
  }

  //! generate a random network from number of: nodes, and retis
  template<class EdgeContainer = EdgeVec, class NameContainer = NameVec>
  void generate_random_binary_edgelist_nr(EdgeContainer& edges,
                                          NameContainer& names,
                                          const uint32_t num_nodes,
                                          const uint32_t num_retis,
                                          const float multilabel_density = 0.0f)
  {
    const uint32_t num_leaves = l_from_nr(num_nodes, num_retis);
    const uint32_t num_tree_nodes = num_nodes - num_retis - num_leaves;
    return generate_random_binary_edgelist_trl(edges, names, num_tree_nodes, num_retis, num_leaves, multilabel_density);
  }

  //! generate a random network from number of: nodes, and leaves
  template<class EdgeContainer = EdgeVec, class NameContainer = NameVec>
  void generate_random_binary_edgelist_nl(EdgeContainer& edges,
                                          NameContainer& names,
                                          const uint32_t num_nodes,
                                          const uint32_t num_leaves,
                                          const float multilabel_density = 0.0f)
  {
    return generate_random_binary_edgelist_nr(edges, names, num_nodes, num_leaves, multilabel_density);
  }
  //! generate a random network from number of: retis, and leaves
  template<class EdgeContainer = EdgeVec, class NameContainer = NameVec>
  void generate_random_binary_edgelist_rl(EdgeContainer& edges,
                                          NameContainer& names,
                                          const uint32_t num_retis,
                                          const uint32_t num_leaves,
                                          const float multilabel_density = 0.0f)
  {
    const uint32_t num_nodes = n_from_rl(num_retis, num_leaves);
    const uint32_t num_tree_nodes = num_nodes - num_retis - num_leaves;
    return generate_random_binary_edgelist_trl(edges, names, num_tree_nodes, num_retis, num_leaves, multilabel_density);
  }


  //! simulate reticulate species evolution
  template<class _Network>
  void simulate_species_evolution(EdgeVec& edges, NameVec& names, const uint32_t number_taxa, const float recombination_rate)
  {
  }

  //! simulate reticulate gene evolution
  template<class _Network>
  void simulate_gene_evolution(EdgeVec& edges, NameVec& names, const uint32_t number_taxa, const float recombination_rate)
  {
  }

}
