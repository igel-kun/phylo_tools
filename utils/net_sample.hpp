
#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "random.hpp"
#include "tags.hpp"
#include "except.hpp"

namespace PT {


  struct sequential_taxon_name {
    uint32_t count = 0;

    operator std::string() { return operator()(); }

    template<class... Args>
    std::string operator()(Args&&... args) { return to_string(count++); }
    
    static std::string to_string(const uint32_t x) {
      if(x >= 26)
        return to_string(x/26) + static_cast<char>('a' + (x % 26));
      else
        return std::string("") + static_cast<char>('a' + x);
    }
  };


  struct NodeNums {
    int num_tree_nodes = -1;
    int num_retis = -1;
    int num_leaves = -1;
    float multilabel_density = 0.0f;

    //NOTE: in a binary network, we have n = t + r + l, but also l + r - 1 = t (together, n = 2t + 1 and n = 2l + 2r - 1)
    static constexpr int l_from_nr(const int n, const int r) {
      if(n % 2 == 0) throw std::logic_error("cannot generate binary network with even number of nodes");
      if(n < 2*r + 1) throw std::logic_error("need at least "+std::to_string(2*r+1)+" nodes (vs "+std::to_string(n)+" given) in a binary network with "+std::to_string(r)+" reticulations/leaves");
      return (n - 2*r + 1) / 2;
    }

    static constexpr int n_from_rl(const int r, const int l) {
      if(l == 0) throw std::logic_error("cannot generate network without leaves");
      return 2*r + 2*l - 1;
    }

    static constexpr int r_from_nl(const int n, const int l) { return l_from_nr(n,l); }

    // for binary networks: compute numbers from [N]odes & [R]etis or from [N]odes & [L]eaves or from [R]etis & [L]eaves
    // for non-binary, we will always need the number of leaves, reticulations, and tree-nodes
    enum class From { BinNR, BinNL, BinRL, TRL };

    void from_nrl(const int n, const int r, const int l) {
      num_retis = r;
      num_leaves = l;
      num_tree_nodes = n - r - l;
    }
    void from_nr(const int n, const int r) { from_nrl(n, r, l_from_nr(n, r)); }
    void from_nl(const int n, const int l) { from_nrl(n, r_from_nl(n, l), l); }
    void from_rl(const int r, const int l) { from_nrl(n_from_rl(r, l), r, l); }

    NodeNums() = default;

    NodeNums(const int _num_nodes, const int _num_retis, const int _num_leaves, float _ml_density = 0.0f): multilabel_density{_ml_density}
    {
      from_nrl(_num_nodes, _num_retis, _num_leaves);
    }

    NodeNums(const From& from, const int x1, const int x2, float ml_density = 0.0f): multilabel_density{ml_density}
    {
      switch(from) {
        case From::BinNR: from_nr(x1, x2); break;
        case From::BinNL: from_nl(x1, x2); break;
        case From::BinRL: from_rl(x1, x2); break;
        default: assert(false);
      };
    }

    void sanity_check() const {
      if(multilabel_density < 0 || multilabel_density > 1) throw std::logic_error("multilabel density must be between 0 and 1");
      if(num_leaves <= 0) throw std::logic_error("cannot construct network without leaves");
      if(num_tree_nodes <= 0) throw std::logic_error("cannot construct network without tree nodes");

      if((num_nodes() < 0) || (num_internal() < 0) || (num_retis < 0) || (num_leaves < 0) || (num_tree_nodes < 0))
        throw std::logic_error("network geometry implied by given parameters is invalid: "+
            std::to_string(num_tree_nodes)+" tree nodes, "+
            std::to_string(num_retis)+" reticulations, "+
            std::to_string(num_leaves)+" leaves = "+
            std::to_string(num_nodes())+" nodes in total");

      if(min_out_edges() != min_in_edges())
        throw std::logic_error("there is no binary network with " + std::to_string(num_tree_nodes) + " tree nodes, " +
              std::to_string(num_retis) + " reticulations, and " + std::to_string(num_leaves) + " leaves (" +
              std::to_string(min_out_edges()) + " out-degrees vs " + std::to_string(min_in_edges()) + " in-degrees)");
    }

    int num_internal() const { return num_tree_nodes + num_retis; }
    int num_nodes() const { return num_internal() + num_leaves; }
    int min_out_edges() const { return 2 * num_tree_nodes + num_retis; }
    int min_in_edges() const { return (num_tree_nodes - 1) + 2 * num_retis + num_leaves; }
  };

  //! generate random (not necessarily binary) tree
  template<StrictTreeType Tree, EdgeEmplacerType Emplacer>
  void generate_random_tree(Tree& T, const NodeNums nums, Emplacer&& emplacer) {
    nums.sanity_check();
    if(nums.num_retis != 0) throw std::logic_error("a tree cannot contain reticulations");

    const NodeDesc rt = emplacer.create_root();
    
    NodeSet current_leaves; // nodes that can accept in-degree (should contain at least 2 nodes at all times)
    mstd::append(current_leaves, rt);
    for(int i = 0; i != nums.num_internal(); ++i){
      // each time we turn a leaf into an internal node, we have to create 2 leaves, so we need to reserve 1 leaf for each internal_to_go
      const auto internals_to_go = nums.num_internal() - i - 1;
      const auto leaves_to_go = nums.num_leaves - current_leaves.size() + 1;
      const auto max_degree = leaves_to_go - internals_to_go;
      const auto min_degree = (internals_to_go == 0) ? leaves_to_go : 2;
      const auto degree = min_degree + throw_die(max_degree - min_degree + 1);
      const auto it = get_random_iterator(current_leaves);
      const NodeDesc u = *it;
      std::cout << "adding ["<<min_degree<<":"<<max_degree<<"] --> "<<degree<<" leaves to "<<u<<'\n';
      current_leaves.erase(it);
      for(size_t j = 0; j != degree; ++j) {
        const NodeDesc v = emplacer.create_node();
        emplacer.emplace_edge_raw(u, v);
        mstd::append(current_leaves, v);
      }
    }
  }

  template<StrictTreeType Tree>
  void generate_random_tree(Tree& T, const NodeNums nums) {
    generate_random_tree(T, nums, DefaultEdgeEmplacer<Tree, false, void, void>{T});
  }


  //! generate labels
  template<StrictPhylogenyType Net, bool leaf_labels_only = false> requires (Net::has_node_labels)
  void generate_labels(Net& T, const float multilabel_density = 0.0f) {
#warning "TODO: implement multi-labels"
    assert(multilabel_density >= 0.0f   && multilabel_density < 1.0f);

    sequential_taxon_name get_label;
    for(const NodeDesc u: T.nodes()) {
      if constexpr (leaf_labels_only)
        if(!T.is_leaf(u)) continue;
      T.label(u) = get_label();
    }
  }

  template<StrictPhylogenyType Net>
  void generate_labels(const leaf_labels_only_tag, Net& T, const float multilabel_density = 0.0f) {
    generate_labels<Net, true>(T, multilabel_density);
  }
  template<StrictPhylogenyType Net>
  void generate_leaf_labels(Net& T, const float multilabel_density = 0.0f) {
    generate_labels<Net>(T, multilabel_density);
  }

  //! add a number of random edges to a given network, introducing new_tree_nodes new tree nodes and new_reticulations new reticulations
  //NOTE: this may result in a non-binary network
  //NOTE: if N is a tree, new_reticulations may not be zero, but new_tree_nodes may be zero (in this case, we're re-using existing tree nodes)
  //NOTE: if new_tree_nodes == new_reticulations == num_edges, then no old node will be incident with a new edge (old nodes maintain their degrees)
  template<StrictPhylogenyType Net, EdgeEmplacerType Emplacer>
  void add_random_edges(Net& N, uint32_t new_tree_nodes, uint32_t new_reticulations, uint32_t num_edges, Emplacer&& emplacer) {
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
          mstd::append(retis, u);
        else if(!N.is_leaf(u))
          mstd::append(tree_nodes, u);

      if(retis.empty() && !new_reticulations)
        throw std::logic_error("cannot add " + std::to_string(num_edges) + " edges without introducing a reticulation");

      while(num_edges){
        std::cout << "adding "<<num_edges<<" new edges to\n"<<N<<"\n";
        if(new_reticulations){
          const auto edges = N.edges();
          const auto uv_iter = get_random_iterator(edges, N.num_edges());
          const auto uv = *uv_iter;
          const NodeDesc u = uv.tail();
          const auto& v = uv.head();
          //const NodeDesc u = uv_iter->first;
          //const NodeDesc v = uv_iter->second;
          if(new_tree_nodes){
            const auto xy_iter = get_random_iterator_except(edges, uv_iter, N.num_edges());
            assert(xy_iter != uv_iter);
            const auto xy = *xy_iter;
            const NodeDesc x = xy.tail();
            const auto& y = xy.head();
            assert((x != u) or (y != v));
            const bool reverse_st = N.has_path(y,u);
            DEBUG3(std::cout << "rolled nodes: "<<u<<" "<<v<<" and "<<x<<" "<<y<<"\t "<<y<<"-"<<u<<"-path? "<<reverse_st<<'\n');
            NodeDesc s = emplacer.create_node();
            NodeDesc t = emplacer.create_node();

            DEBUG3(std::cout << "adding node "<<s<<" between "<< u << " & "<< v <<'\n');
            DEBUG3(std::cout << "adding node "<<t<<" between "<< x << " & "<< y <<'\n');
            emplacer.subdivide_edge(uv, s);
            emplacer.subdivide_edge(xy, t);
            if(reverse_st) std::swap(s,t);
            DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
            emplacer.emplace_edge_raw(s, t); --num_edges;
            mstd::append(tree_nodes, s);  --new_tree_nodes;
            mstd::append(retis, t);       --new_reticulations;
          } else {
            if(u != N.root()){
              NodeDesc s;
              const NodeDesc t = emplacer.create_node();
              emplacer.subdivide_edge(*uv_iter, t);
              do s = *(get_random_iterator(tree_nodes)); while((s != u) && !N.has_path(v, s));
              DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
              emplacer.emplace_edge_raw(s, t);    --num_edges;
              mstd::append(retis, t);       --new_reticulations;
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
                s = emplacer.create_node();
                emplacer.subdivide_edge(*xy_iter, s);
                break;
              }
            }
            DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
            emplacer.emplace_edge_raw(s, t); --num_edges;
            mstd::append(tree_nodes, s);  --new_tree_nodes;
          } else {
            const NodeDesc s = *(get_random_iterator(tree_nodes));
            if(!N.has_path(t,s)){
              DEBUG5(std::cout << "adding edge "<<s<<"-->"<<t<<"\n");
              emplacer.emplace_edge_raw(s, t);  --num_edges;
            }
          }
        }
      }
    }
  }

  template<StrictPhylogenyType Net>
  void add_random_edges(Net& N, uint32_t new_tree_nodes, uint32_t new_reticulations, uint32_t num_edges) {
    // for the emplacer, set Source and Target phylo to Net, because we may want to extract edge-data from Net when subdividing...
    add_random_edges(N, new_tree_nodes, new_reticulations, num_edges, DefaultEdgeEmplacer<Net, false, Net, void>{N});
  }


  //! generate a random network from number of: tree nodes, retis, and leaves
  template<StrictPhylogenyType Net, EdgeEmplacerType Emplacer>
  void generate_random_binary_network(Net& N, const NodeNums nums, Emplacer&& emplacer) {
#warning "TODO: implement multi-labels"
    nums.sanity_check();
/*
    const uint32_t num_tree_nodes = num.num_tree_nodes;
    const uint32_t num_nodes = num_internal + num_leaves;
*/
    const uint32_t num_internal = nums.num_internal();
    DEBUG2(std::cout << "creating network with "<<nums.num_nodes()<<" nodes ("<<nums.num_internal()<<" internal, "<<nums.num_retis<<" retis, "<<nums.num_leaves<<" leaves)\n");

    std::unordered_map<NodeDesc, uint32_t> dangling;

    int reti_count = 0;
    int tree_count = 0;
    // initialize with a root node
    const NodeDesc new_root = emplacer.create_root();
    DEBUG5(std::cout << "created node "<<new_root<<"\n");
    append(dangling, new_root, 2);
    for(uint32_t i = 1; i < num_internal; ++i){
      std::cout << "remaining dangling: "<<dangling.size() << '\n';
      const uint32_t num_unsatisfied = dangling.size();
      const auto parent_it = get_random_iterator(dangling);
      const NodeDesc u = parent_it->first;
      const NodeDesc v = emplacer.create_node();
      emplacer.emplace_edge_raw(u, v);

      const bool removed = !mstd::decrease_or_remove(dangling, parent_it);
      DEBUG5(std::cout << " node #"<<i<<"\t- "<<reti_count <<" retis & "<<tree_count<<" tree nodes - ");
      
      // the new node v might be a reticulation if there are at least 2 unsatisfied nodes
      if((reti_count < nums.num_retis) &&
             (num_unsatisfied > 1) &&
             throw_bw_die(nums.num_retis - reti_count, num_internal - i)){
        // node v is a reticulation
        // the second incoming edge is from a random unsatisfied node (except last_node)
        std::cout << "reti"<<std::endl;
        const auto dang_it = removed ? get_random_iterator(dangling) : get_random_iterator_except(dangling, parent_it);
        std::cout << "got "<<*dang_it<<std::endl;
        const NodeDesc w = dang_it->first;

        emplacer.emplace_edge_raw(w, v);
        mstd::decrease_or_remove(dangling, dang_it);
        dangling[v] = 1;
        ++reti_count;
      } else {
        if(tree_count == nums.num_tree_nodes) throw std::logic_error("using too many tree vertices, this should not happen");
        // node i is a tree vertex
        dangling[v] = 2;
        ++tree_count;
      }
    }
    // satisfy all using the leaves
    for(int i = num_internal; i < nums.num_nodes(); ++i){
      if(dangling.empty()) throw std::logic_error("not enough internal nodes to fit all leaves");
      const auto iter = dangling.begin();
      const NodeDesc u = iter->first;
      const NodeDesc v = emplacer.create_node();
      emplacer.emplace_edge_raw(u, v);
      mstd::decrease_or_remove(dangling, iter);
      
      DEBUG5(std::cout << " node #"<<i<<" is a leaf"<<std::endl);
    }
    if(!dangling.empty()) throw std::logic_error("not enough leaves to satisfy all internal nodes");
  }

  template<StrictPhylogenyType Net>
  void generate_random_binary_network(Net& N, NodeNums nums) {
    // NOTE: there is no need for the emplacer to track roots, we'll do this by hand; also, we don't need node translations...
    generate_random_binary_network(N, nums, DefaultEdgeEmplacer<Net, false, void, void>{N});
  }

  //! simulate reticulate species evolution
  template<PhylogenyType Net, EdgeContainerType Edges, mstd::ContainerType Names>
  void simulate_species_evolution(Edges& edges, Names& names, const uint32_t number_taxa, const float recombination_rate)
  {
#warning "writeme"
  }

  //! simulate reticulate gene evolution
  template<PhylogenyType Net, EdgeContainerType Edges, mstd::ContainerType Names>
  void simulate_gene_evolution(Edges& edges, Names& names, const uint32_t number_taxa, const float recombination_rate)
  {
#warning "writeme"
  }

}
