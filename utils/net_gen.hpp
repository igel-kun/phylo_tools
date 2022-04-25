
#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "random.hpp"
#include "tags.hpp"

namespace PT{

  //NOTE: in a binary network, we have n = t + r + l, but also l + r - 1 = t (together, n = 2t + 1 and n = 2l + 2r - 1)

  uint32_t l_from_nr(const uint32_t n, const uint32_t r)
  {
    if(n % 2 == 0) throw std::logic_error("cannot generate binary network with even number of nodes");
    if(n < 2*r + 1) throw std::logic_error("need at least "+std::to_string(2*r+1)+" nodes (vs "+std::to_string(n)+" given) in a binary network with "+std::to_string(r)+" reticulations/leaves");
    return (n - 2*r + 1) / 2;
  }

  uint32_t n_from_rl(const uint32_t r, const uint32_t l)
  {
    if(l == 0) throw std::logic_error("cannot generate network without leaves");
    return 2*r + 2*l - 1;
  }

  uint32_t r_from_nl(const uint32_t n, const uint32_t l)
  {
    return l_from_nr(n,l);
  }


  struct sequential_taxon_name {
    uint32_t count = 0;

    operator std::string() { return operator()(); }

    template<class... Args>
    std::string operator()(Args&&... args) { return to_string(count++); }
    
    static std::string to_string(const uint32_t x) {
      if(x >= 26)
        return to_string(x/26) + ('a' + (x % 26));
      else
        return std::string("") + ('a' + x);
    }
  };


  //! generate random (not necessarily binary) tree
  template<StrictTreeType Tree, DataExtracterType MakeData = DataExtracter<Tree>>
  void generate_random_tree(Tree& T,
                            const uint32_t num_internal,
                            const uint32_t num_leaves,
                            MakeData make_data = MakeData())
  {  
    if(num_leaves == 0) throw std::logic_error("cannot construct tree without leaves");
    if(num_internal == 0) throw std::logic_error("cannot construct tree without internal nodes");

    //const uint32_t num_nodes = num_leaves + num_internal;
    const uint32_t num_in_edges = num_leaves + num_internal - 1;
    const uint32_t min_out_edges = 2 * num_internal;
    if(num_in_edges < min_out_edges)
      throw std::logic_error("there is no tree with " + std::to_string(num_internal) + " internal nodes and " +
            std::to_string(num_leaves) + " leaves \
            (total in-degree == " + std::to_string(num_in_edges) + " vs total out-degree >= " + std::to_string(min_out_edges) + ")");

    const NodeDesc rt = T.add_root(make_data);
    
    NodeSet current_leaves; // nodes that can accept in-degree (should contain at least 2 nodes at all times)
    append(current_leaves, rt);
    for(size_t i = 0; i != num_internal; ++i){
      // each time we turn a leaf into an internal node, we have to create 2 leaves, so we need to reserve 1 leaf for each internal_to_go
      const size_t internals_to_go = num_internal - i - 1;
      const size_t leaves_to_go = num_leaves - current_leaves.size() + 1;
      const size_t max_degree = leaves_to_go - internals_to_go;
      const size_t min_degree = (internals_to_go == 0) ? leaves_to_go : 2;
      const size_t degree = min_degree + throw_die(max_degree - min_degree + 1);
      const auto it = get_random_iterator(current_leaves);
      const NodeDesc u = *it;
      std::cout << "adding ["<<min_degree<<":"<<max_degree<<"] --> "<<degree<<" leaves to "<<u<<'\n';
      current_leaves.erase(it);
      for(size_t j = 0; j != degree; ++j) {
        const NodeDesc v = T.create_node(make_data);
        T.add_child(u, v, make_data);
        append(current_leaves, v);
      }
    }
  }


  //! generate labels
  template<StrictTreeType Tree, bool leaf_labels_only = false> requires (Tree::has_node_labels)
  void generate_labels(Tree& T, const float multilabel_density = 0.0f) {
#warning "TODO: implement multi-labels"
    assert(multilabel_density >= 0.0f   && multilabel_density < 1.0f);

    sequential_taxon_name get_label;
    for(const NodeDesc u: T.nodes()) {
      if constexpr (leaf_labels_only)
        if(!T.is_leaf(u)) continue;
      T.label(u) = get_label();
    }
  }

  template<StrictTreeType Tree>
  void generate_labels(const leaf_labels_only_tag, Tree& T, const float multilabel_density = 0.0f) {
    generate_labels<Tree, true>(T, multilabel_density);
  }
  template<StrictTreeType Tree>
  void generate_leaf_labels(Tree& T, const float multilabel_density = 0.0f) {
    generate_labels<Tree, true>(T, multilabel_density);
  }

  //! add a number of random edges to a given network, introducing new_tree_nodes new tree nodes and new_reticulations new reticulations
  //NOTE: this may result in a non-binary network
  //NOTE: if N is a tree, new_reticulations may not be zero, but new_tree_nodes may be zero (in this case, we're re-using existing tree nodes)
  //NOTE: if new_tree_nodes == new_reticulations == num_edges, then no old node will be incident with a new edge (old nodes maintain their degrees)
  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void add_random_edges(Net& N, uint32_t new_tree_nodes, uint32_t new_reticulations, uint32_t num_edges, ExtractData&& extracter = ExtractData()) {
    if(num_edges > 0){
      if(N.num_edges() < 2)
        throw std::logic_error("cannot add edges to a tree/network with less than 2 edges");
      if(new_tree_nodes > num_edges)
        throw std::logic_error("cannot add " + std::to_string(new_tree_nodes) + " new tree nodes with only " + std::to_string(num_edges) + " new edges");
      if(new_reticulations > num_edges)
        throw std::logic_error("cannot add " + std::to_string(new_reticulations) + " new reticulations with only " + std::to_string(num_edges) + " new edges");

      NodeSet tree_nodes, retis;
      for(const NodeDesc u: N.nodes())
        if(N.is_reti(u))
          append(retis, u);
        else if(!N.is_leaf(u))
          append(tree_nodes, u);

      if(retis.empty() && !new_reticulations)
        throw std::logic_error("cannot add " + std::to_string(num_edges) + " edges without introducing a reticulation");

      std::cout << "adding "<<num_edges<<" new edges to\n"<<N<<"\n";
      while(num_edges){
        if(new_reticulations){
          const auto edges = N.edges();
          const auto uv_iter = get_random_iterator(edges, N.num_edges());
          const NodeDesc u = uv_iter->tail();
          auto& v = uv_iter->head();
          //const NodeDesc u = uv_iter->first;
          //const NodeDesc v = uv_iter->second;
          if(new_tree_nodes){
            const auto xy_iter = get_random_iterator_except(edges, uv_iter, N.num_edges());
            const NodeDesc x = xy_iter->tail();
            auto& y = xy_iter->head();
            const bool reverse_st = N.has_path(y,u);
            DEBUG3(std::cout << "rolled nodes: "<<u<<" "<<v<<" and "<<x<<" "<<y<<"\t "<<y<<"-"<<u<<"-path? "<<reverse_st<<'\n');
            NodeDesc s = Net::create_node(extracter);
            NodeDesc t = Net::create_node(extracter);

            if constexpr (!ExtractData::ignoring_edge_data) {
              N.subdivide_edge(*uv_iter, s, [&](auto&&, auto& data){ data = extracter.get_edge_data(s, v);});
              N.subdivide_edge(*xy_iter, t, [&](auto&&, auto& data){ data = extracter.get_edge_data(t, y);});
            } else {
              N.subdivide_edge(*uv_iter, s);
              N.subdivide_edge(*xy_iter, t);
            }
            if(reverse_st) std::swap(s,t);
            DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
            N.add_edge(s, t, extracter); --num_edges;
            append(tree_nodes, s);  --new_tree_nodes;
            append(retis, t);       --new_reticulations;
          } else {
            if(u != N.root()){
              NodeDesc s;
              const NodeDesc t = Net::create_node(extracter);
              if constexpr (!ExtractData::ignoring_edge_data)
                N.subdivide_edge(*uv_iter, t, [&](auto&&, auto& data){ data = extracter.get_edge_data(s, v);});
              else N.subdivide_edge(*uv_iter, t);
              do s = *(get_random_iterator(tree_nodes)); while((s != u) && !N.has_path(v, s));
              DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
              N.add_edge(s, t, extracter);    --num_edges;
              append(retis, t);       --new_reticulations;
            }
          }
        } else {
          const NodeDesc t = *(get_random_iterator(retis));
          if(new_tree_nodes){
            NodeDesc s;
            while(1){
              const auto xy_iter = get_random_iterator(N.edges(), N.num_edges());
              const NodeDesc x = xy_iter->tail();
              auto& y = xy_iter->head();
              if((t != y) && !N.has_path(t, x)) {
                s = Net::create_node(extracter);
                if constexpr (!ExtractData::ignoring_edge_data)
                  N.subdivide_edge(*xy_iter, s, [&](auto&&, auto& data){ data = extracter.get_edge_data(s, y);});
                else N.subdivide_edge(*xy_iter, s);
                break;
              }
            }
            DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
            N.add_edge(s, t, extracter); --num_edges;
            append(tree_nodes, s);  --new_tree_nodes;
          } else {
            const NodeDesc s = *(get_random_iterator(tree_nodes));
            if(!N.has_path(t,s)){
              DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
              N.add_edge(s, t, extracter);  --num_edges;
            }
          }
        }
      }
    }
  }


  //! generate a random network from number of: tree nodes, retis, and leaves
  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_trl(Net& N,
                            const uint32_t num_tree_nodes,
                            const uint32_t num_retis,
                            const uint32_t num_leaves,
                            const float multilabel_density,
                            ExtractData&& extracter = ExtractData())
  {
#warning "implement multi-labels"
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

    std::cout << "creating network with "<<num_nodes<<" nodes ("<<num_internal<<" internal, "<<num_retis<<" retis, "<<num_leaves<<" leaves)\n";

    std::unordered_map<NodeDesc, uint32_t> dangling;

    uint32_t reti_count = 0;
    uint32_t tree_count = 0;
    // initialize with a root node
    const NodeDesc new_root = N.add_root(extracter);
    std::cout << "created node "<<new_root<<" at "<<N[new_root].data<<"\n";
    dangling.try_emplace(new_root, 2);
    for(uint32_t i = 1; i < num_internal; ++i){
      std::cout << "remaining dangling: "<<dangling.size() << '\n';
      const uint32_t num_unsatisfied = dangling.size();
      const auto parent_it = get_random_iterator(dangling);
      const NodeDesc u = parent_it->first;
      const NodeDesc v = Net::create_node(extracter);
      N.add_child(u, v, extracter);

      const bool removed = !decrease_or_remove(dangling, parent_it);
      DEBUG5(std::cout << " node #"<<i<<"\t- "<<reti_count <<" retis & "<<tree_count<<" tree nodes - ");
      
      // the new node v might be a reticulation if there are at least 2 unsatisfied nodes
      if((reti_count < num_retis) &&
             (num_unsatisfied > 1) &&
             throw_bw_die(num_retis - reti_count, num_internal - i)){
        // node v is a reticulation
        // the second incoming edge is from a random unsatisfied node (except last_node)
        std::cout << "reti"<<std::endl;
        const auto dang_it = removed ? get_random_iterator(dangling) : get_random_iterator_except(dangling, parent_it);
        std::cout << "got "<<*dang_it<<std::endl;
        const NodeDesc w = dang_it->first;
        N.add_child(w, v, extracter.get_edge_data(w, v));
        decrease_or_remove(dangling, dang_it);
        dangling[v] = 1;
        ++reti_count;
      } else {
        if(tree_count == num_tree_nodes) throw std::logic_error("using too many tree vertices, this should not happen");
        // node i is a tree vertex
        dangling[v] = 2;
        ++tree_count;
      }
    }
    // satisfy all using the leaves
    for(uint32_t i = num_internal; i < num_nodes; ++i){
      if(dangling.empty()) throw std::logic_error("not enough internal nodes to fit all leaves");
      const auto iter = dangling.begin();
      const NodeDesc u = iter->first;
      const NodeDesc v = Net::create_node(extracter.get_node_data(u));
      if constexpr (ExtractData::custom_node_label_maker) node_of<Net>(v).label() = extracter.get_node_label(v);
      N.add_child(u, v, extracter.get_edge_data(u, v));
      decrease_or_remove(dangling, iter);
      
      DEBUG5(std::cout << " node #"<<i<<" is a leaf"<<std::endl);
    }
    if(!dangling.empty()) throw std::logic_error("not enough leaves to satisfy all internal nodes");
  }

  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_trl(Net& N,
                            const uint32_t num_tree_nodes,
                            const uint32_t num_retis,
                            const uint32_t num_leaves,
                            ExtractData&& extracter = ExtractData())
  {
    generate_random_binary_network_trl(N, num_tree_nodes, num_retis, num_leaves, 0.0f, std::forward<ExtractData>(extracter));
  }


  //! generate a random network from number of: nodes, and retis
  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_nr(Net& N,
                            const uint32_t num_nodes,
                            const uint32_t num_retis,
                            const float multilabel_density,
                            ExtractData&& extracter = ExtractData())
  {
    const uint32_t num_leaves = l_from_nr(num_nodes, num_retis);
    const uint32_t num_tree_nodes = num_nodes - num_retis - num_leaves;
    return generate_random_binary_network_trl(N, num_tree_nodes, num_retis, num_leaves, multilabel_density, std::forward<ExtractData>(extracter));
  }

  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_nr(Net& N,
                            const uint32_t num_nodes,
                            const uint32_t num_retis,
                            ExtractData&& extracter)
  {
    generate_random_binary_network_nr(N, num_nodes, num_retis, 0.0f, std::forward<ExtractData>(extracter));
  }


  //! generate a random network from number of: nodes, and leaves - 
  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_nl(Net& N,
                            const uint32_t num_nodes,
                            const uint32_t num_leaves,
                            const float multilabel_density,
                            ExtractData&& extracter = ExtractData())
  {
    const uint32_t num_retis = r_from_nl(num_nodes, num_leaves);
    const uint32_t num_tree_nodes = num_nodes - num_retis - num_leaves;
    return generate_random_binary_network_trl(N, num_tree_nodes, num_retis, num_leaves, multilabel_density, std::forward<ExtractData>(extracter));
  }

  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_nl(Net& N,
                            const uint32_t num_nodes,
                            const uint32_t num_leaves,
                            ExtractData&& extracter = ExtractData())
  {
    generate_random_binary_network_nl(N, num_nodes, num_leaves, 0.0f, std::forward<ExtractData>(extracter));
  }

  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_rl(Net& N,
                            const uint32_t num_retis,
                            const uint32_t num_leaves,
                            const float multilabel_density,
                            ExtractData&& extracter = ExtractData())
  {
    const uint32_t num_nodes = n_from_rl(num_retis, num_leaves);
    const uint32_t num_tree_nodes = num_nodes - num_retis - num_leaves;
    return generate_random_binary_network_trl(N, num_tree_nodes, num_retis, num_leaves, multilabel_density, std::forward<ExtractData>(extracter));
  }

  template<StrictPhylogenyType Net, class ExtractData = DataExtracter<Net>>
  void generate_random_binary_network_rl(Net& N,
                            const uint32_t num_retis,
                            const uint32_t num_leaves,
                            ExtractData&& extracter = ExtractData())
  {
    generate_random_binary_network_nl(N, num_retis, num_leaves, 0.0f, std::forward<ExtractData>(extracter));
  }


  //! simulate reticulate species evolution
  template<PhylogenyType Net, EdgeContainerType Edges, std::ContainerType Names>
  void simulate_species_evolution(Edges& edges, Names& names, const uint32_t number_taxa, const float recombination_rate)
  {
#warning writeme
  }

  //! simulate reticulate gene evolution
  template<PhylogenyType Net, EdgeContainerType Edges, std::ContainerType Names>
  void simulate_gene_evolution(Edges& edges, Names& names, const uint32_t number_taxa, const float recombination_rate)
  {
#warning writeme
  }

}
