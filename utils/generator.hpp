
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


  struct sequential_taxon_name
  {
    uint32_t count = 0;

    operator std::string() { return operator()(count++); }

    std::string operator()(const uint32_t x) const { return to_string(x); }
    
    static std::string to_string(const uint32_t x)
    {
      if(x >= 26)
        return to_string(x/26) + (char)('a' + (x % 26));
      else
        return std::string("") + (char)('a' + x);
    }
  };


  //! generate random (not necessarily binary) tree
  template<class EdgeContainer, class NameContainer>
  void generate_random_tree(EdgeContainer& edges, NameContainer& names,
                            const uint32_t num_internal,
                            const uint32_t num_leaves,
                            const float multilabel_density = 0.0f)
  {
#warning implement multi-labels
    assert(multilabel_density >= 0   && multilabel_density < 1);
    
    if(num_leaves == 0) throw std::logic_error("cannot construct tree without leaves");
    if(num_internal == 0) throw std::logic_error("cannot construct tree without internal nodes");

    const uint32_t num_nodes = num_leaves + num_internal;
    const uint32_t num_in_edges = num_leaves + num_internal - 1;
    const uint32_t min_out_edges = 2 * num_internal;
    if(num_in_edges < min_out_edges)
      throw std::logic_error("there is no tree with " + std::to_string(num_internal) + " internal nodes and " +
            std::to_string(num_leaves) + " leaves \
            (in-degree == " + std::to_string(num_in_edges) + " vs out-degree >= " + std::to_string(min_out_edges) + ")");

    std::unordered_set<Node> free_nodes; // nodes that can accept in-degree (should contain at least 2 nodes at all times)
    sequential_taxon_name sqn;
    for(Node u = 0; u != num_leaves; ++u){
      append(free_nodes, u);
      names.try_emplace(u, sqn(u));
    }
    for(Node u = num_leaves; u != num_nodes; ++u){
      const uint32_t nodes_left = num_nodes - u;
      assert(free_nodes.size() + 1 >= nodes_left);
      const uint32_t max_degree = free_nodes.size() - (nodes_left - 1);
      const uint32_t min_degree = (u == num_nodes - 1 ? max_degree : 2);
      const uint32_t degree = min_degree + throw_die(max_degree - min_degree + 1);
      for(uint32_t i = 0; i != degree; ++i){
        const auto it = get_random_iterator(free_nodes);
        append(edges, u, *it);
        free_nodes.erase(it);
      }
    }
  }

  //! add a number of random edges to a given network, introducing new_tree_nodes new tree nodes and new_reticulations new reticulations
  //NOTE: this may result in a non-binary network
  //NOTE: if N is a tree, new_reticulations may not be zero, but new_tree_nodes may be zero (in this case, we're re-using existing tree nodes)
  //NOTE: if new_tree_nodes == new_reticulations == num_edges, then no old node will be incident with a new edge (old nodes maintain their degrees)
  template<class Network>
  void add_random_edges(Network& N, uint32_t new_tree_nodes, uint32_t new_reticulations, uint32_t num_edges)
  {
    if(N.num_edges() < 2)
      throw std::logic_error("cannot add edges to a tree/network with less than 2 edges");
    if(new_tree_nodes > num_edges)
      throw std::logic_error("cannot add" + std::to_string(new_tree_nodes) + " new tree nodes with only " + std::to_string(num_edges) + " new edges");
    if(new_reticulations > num_edges)
      throw std::logic_error("cannot add" + std::to_string(new_reticulations) + " new reticulations with only " + std::to_string(num_edges) + " new edges");

    HashSet<Node> tree_nodes, retis;
    for(const Node u: N) if(N.is_reti(u)) append(retis, u); else if(!N.is_leaf(u)) append(tree_nodes, u);

    if(retis.empty() && !new_reticulations)
      throw std::logic_error("cannot add" + std::to_string(num_edges) + " edges without introducing a reticulation");

    while(num_edges){
      unsigned char reti_diff = 0;
      unsigned char tree_diff = 0;

      if(new_reticulations){
        const auto [u, v] = get_random_iterator(N.edges())->as_pair();
        if(new_tree_nodes){
          const auto [x, y] = get_random_iterator(N.edges())->as_pair();
          if(u != x){
            Node s = N.subdivide(u, v);
            Node t = N.subdivide(x, y);
            if(N.has_path(y, u)) std::swap(s, t);
            N.add_edge(s, t);       --num_edges;
            append(tree_nodes, s);  --new_tree_nodes;
            append(retis, t);       --new_reticulations;
          }
        } else {
          if(u != N.root()){
            const Node t = N.subdivide(u, v);
            Node s;
            do s = *(get_random_iterator(tree_nodes)); while((s != u) && !N.has_path(v, s));
            N.add_edge(s, t);       --num_edges;
            append(retis, t);       --new_reticulations;
          }
        }
      } else {
        const Node t = *(get_random_iterator(retis));
        if(new_tree_nodes){
          Node s;
          while(1){
            const auto [x, y] = get_random_iterator(N.edges())->as_pair();
            if((t != y) && !N.has_path(t, x)) {
              s = N.subdivide(x, y);
              break;
            }
          }
          N.add_edge(s, t);       --num_edges;
          append(tree_nodes, s);  --new_tree_nodes;
        } else {
          const Node s = *(get_random_iterator(tree_nodes));
          if(!N.has_path(t,s)){
            N.add_edge(s, t);       --num_edges;
          }
        }
      }
    }
  }


  //! generate a random network from number of: tree nodes, retis, and leaves
  template<class EdgeContainer, class NameContainer>
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

    const uint32_t min_out_edges = 2 * num_tree_nodes + num_retis;
    const uint32_t min_in_edges = (num_tree_nodes - 1) + 2 * num_retis + num_leaves;
    if(min_out_edges != min_in_edges)
      throw std::logic_error("there is no binary network with " + std::to_string(num_tree_nodes) + " tree nodes, " +
            std::to_string(num_retis) + " reticulations, and " +
            std::to_string(num_leaves) + " leaves (" + std::to_string(min_out_edges) + " out-degrees vs " + std::to_string(min_in_edges) + " in-degrees)");

    std::unordered_map<uint32_t, unsigned char> dangling;

    uint32_t reti_count = 0;
    uint32_t tree_count = 0;
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
    sequential_taxon_name sqn;
    for(uint32_t i = num_internal; i < num_nodes; ++i){

      // WTF stl, why is there no front() / pop_front() for unordered_maps??? (see https://stackoverflow.com/questions/16981600/why-no-front-method-on-stdmap-and-other-associative-containers-from-the-stl)
      const uint32_t parent = dangling.begin()->first;
      
      edges.emplace_back(parent, i);
      decrease_or_remove(dangling, dangling.begin());
      append(names, i, sqn(i - num_internal));
      
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
