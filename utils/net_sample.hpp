
#pragma once

#include <cmath>

#include "utils.hpp"
#include "types.hpp"
#include "random.hpp"
#include "tags.hpp"
#include "except.hpp"
#include "subset_rank.hpp" // for converting between numbers and subsets

namespace PT {


  // This splits out consecutive integers as strings, to use as taxon-names.
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

  // NodeNums can sanitize node-characteristics of a REDUCED network (result of contracting-down out-deg-1 nodes)
  // For example, it can compute the number of reticulations in a binary network with t tree-nodes and l leaves,
  // or check whether a network with a given number of reticulations, leaves, and tree-nodes exists.
  struct ReducedNodeNums {   
    // ------- static stuff --------
    using DegreeBounds = mstd::linear_interval<uint32_t>;

    // for binary networks: compute numbers from [N]odes & [R]etis or from [N]odes & [L]eaves or from [R]etis & [L]eaves
    // for non-binary, we will always need the number of leaves, reticulations, and tree-nodes
    enum class From { NR, NL, RL, TRL };

    static constexpr auto reti_num_from_r(const uint32_t r, const DegreeBounds r_in_deg = {2,2}) { return r * (r_in_deg - 1u); }

    // We can compute the total number #n of nodes, the number #h of hubs, the number #t of tree-nodes, or the number #l of leaves
    //NOTE: In all reduced networks, we have #n = #t + #h + #l.
    //NOTE: The sum of in-degrees must match the sum of out-degrees. In other words,
    //        (1) the sum over all nodes v of in-deg(v) - out-deg(v) must be zero.
    //      Each leaf contributes 1 to (1), the contribution of all reticulation-parts of hubs to (1) is the reticulation number
    //      Thus, (2) #l + reti-number == (#t + #h) * (t_out_deg - 1) + 1  (the last 1 is for the root, which doesn't have in-degree)
    //      Thus: (3) #l == #n - #t - #h == (#t + #h) * (t_out_deg - 1) - reti-num + 1
    //      Also: (4) #t + #h == #n - #l == (#l + reti-num - 1) / (t_out_deg - 1)

    // From (2), we get
    // (5) #l + reti-number == (#n - #l) * (t_out_deg - 1) + 1
    // and, thus: (6) #l * t_out_deg + reti-number == #n * (t_out_deg - 1) + 1
    // and, thus: #l == (#n * (t_out_deg - 1) - reti-number + 1) / t_out_deg
    static constexpr auto l_from_nr(const uint32_t n, const uint32_t reti_num, const DegreeBounds t_out_deg = {2,2}) {
      const auto tmp = (static_cast<double>(n) * (t_out_deg - 1.0) - (reti_num + 1.0));
      return DegreeBounds{(tmp / t_out_deg).shrink_to_int()};
    }

    // From (6), we get: #n == (#l * t_out_deg + reti-number - 1) / (t_out_deg - 1)
    static constexpr auto n_from_rl(const uint32_t reti_num, const uint32_t l, const DegreeBounds t_out_deg = {2,2}) {
      const auto tmp = (static_cast<double>(l) * t_out_deg + (reti_num - 1.0));
      return DegreeBounds{(tmp / (t_out_deg - 1u)).shrink_to_int()};
    }

    // NOTE: this returns the reticulation number, not the number of reticulations! (they are the same for binary networks)
    // From (5), we get: reti-number == (#n - #l) * (t_out_deg - 1) + 1 - #l
    static constexpr auto r_from_nl(const uint32_t n, const uint32_t l, DegreeBounds t_out_deg = {2,2}) {
      return (n - l) * (t_out_deg - 1) + 1 - l;
    }

    // ------- members --------
    int32_t num_nodes;
    int32_t reti_number; // NOTE: reticulation number = sum(in-degrees) - #reticulations, this is equal to #reticulations in binary networks
    int32_t num_leaves;
    float multilabel_density;
    DegreeBounds tree_out_deg = {2,2}; // NOTE: the out-degree bounds are only for tree-nodes

    // ------- construction & desctruction ---------
    ReducedNodeNums() = default;

    ReducedNodeNums(const auto _num_nodes, const auto _reti_number, const auto _num_leaves,
        const float ml_density = 0.0f, const DegreeBounds t_out_deg = {2,2}):
      multilabel_density{ml_density}, tree_out_deg{t_out_deg}
    {
      from_nrl(_num_nodes, _reti_number, _num_leaves);
    }

    ReducedNodeNums(const From& from, const auto x1, const auto x2, const float ml_density = 0.0f, const DegreeBounds t_out_deg = {2,2}):
      multilabel_density{ml_density}, tree_out_deg{t_out_deg}
    {
      switch(from) {
        case From::NR: from_nr(x1, x2); break;
        case From::NL: from_nl(x1, x2); break;
        case From::RL: from_rl(x1, x2); break;
        default: assert(false);
      };
    }

    // ------- operators --------
    // ------- methods: initialization --------
    void from_nrl(const auto n, const auto r, const auto l) {
      num_nodes = n;
      reti_number = r;
      num_leaves = l;
    }
    void from_nr(const auto n, const auto r) { from_nrl(n, r, l_from_nr(n, r, tree_out_deg).unique_int()); }
    void from_nl(const auto n, const auto l) { from_nrl(n, r_from_nl(n, l, tree_out_deg).unique_int(), l); }
    void from_rl(const auto r, const auto l) { from_nrl(n_from_rl(r, l, tree_out_deg).unique_int(), r, l); }

    // ------- methods: query --------
    // for binary networks, we can give the expected numbers of internal nodes, sum of out-degrees and sum of in-degrees
    int32_t num_internal() const { return num_nodes - num_leaves; }
    int32_t num_edges() const { return num_nodes - 1 + reti_number; } // num_nodes - 1 tree edges + reti_num reti-edges
    auto num_tree_nodes() const { return (static_cast<double>(num_leaves + reti_number) / (tree_out_deg - 1)).shrink_to_int(); }

    void sanity_check() const {
      if(not mstd::linear_interval{0,1}.contains(multilabel_density)) throw std::logic_error("multilabel density must be between 0 and 1");
      if(num_internal() < 0 or reti_number < 0 or num_leaves < 0 or num_nodes < 0) {
        throw std::logic_error{
            "network topology implied by given parameters is invalid: " +
            std::to_string(reti_number) + " reticulation number " +
            std::to_string(num_leaves) + " leaves = " +
            std::to_string(num_nodes) + " nodes in total"
        };
      }
            
      if(num_nodes > 0) {
        if(num_leaves < 1) throw std::logic_error("cannot construct network without leaves");
        if(num_tree_nodes().high() < 1) throw std::logic_error("cannot construct network without tree nodes");
      }
    }    
    // ------- methods: modification --------
  };


  // This class builds trees with a given number of labelled and unlabelled leaves.
  // Unlabelled leaves can be merged with other nodes later on to form a network.
  // NOTE: use the Decider to make this class either sample or enumerate. The Deciser is called with a positive uint x and shall return an uint in [0,x).
  //        1. to sample, just have the decider return a random number
  //        2. to enumerate, return fixed numbers and advance them on call of operator() without arguments
  template<StrictPhylogenyType Tree_, class Decider_, class DataExtracter_ = DataExtracter<void>>
  struct TreeBuilder {
    // ------- static stuff --------
    using Network = Tree_;
    using Decider = Decider_;
  
    // we don't need to translate nodes and we'll do our own root-tracking
    using EmplacementHelper = EdgeEmplacementHelper<Network, false, void>;
    using Emplacer = EdgeEmplacer<EmplacementHelper, DataExtracter_>;

    using Index = PT::Degree;
    using IndexPair = std::pair<Index, Index>;
    using IndexVec = std::vector<Index>;

  
    // ------- members --------
  protected:
    ReducedNodeNums target_nums;
    Network* N;
  
    NodeVec labelled_leaves;
    [[ no_unique_address ]] Emplacer emplacer;
    [[ no_unique_address ]] Decider decider;

    // ------- construction & desctruction ---------
  public:
  
    template<class... EmplacerArgs> 
    TreeBuilder(Network& N_, ReducedNodeNums nums, Decider _decider, EmplacerArgs&&... args):
      target_nums{std::move(nums)},
      N{&N_},
      emplacer(N, std::forward<EmplacerArgs>(args)...),
      decider(std::move(_decider))
    {
      // NOTE: currently, we only support an upper-bound on the max-degree, no lower bound
      if(target_nums.tree_out_deg.low() != 2)
        throw mstd::Unimplemented{"lower-bounding out-degree to anything >2"};
    }
 
    template<class First, class... EmplacerArgs> 
      requires (std::is_default_constructible_v<Decider> and not mstd::is_same_v<First, Decider>)
    TreeBuilder(Network& N_, ReducedNodeNums nums, First&& first, EmplacerArgs&&... args):
      TreeBuilder(N_, nums, Decider{}, std::forward<First>(first), std::forward<EmplacerArgs>(args)...)
    {}
  
#warning "TODO: implement multi-labels"
    //! generate labels
    //NOTE: we'll give all nodes in 'labelled_leaves' a label and return the label maker in case the user wants to label additional nodes
    auto generate_leaf() 
      requires (Network::has_node_labels)
    {
      sequential_taxon_name get_label;
      for(const NodeDesc u: labelled_leaves)
        Network::label(u) = get_label();
      return get_label;
    }


    // decrease num_leaves and add a new leaf to N, either dangling from an existing node, or from a newly created node (by subdivision)
    template<NodeContainerType Leaves, NodeContainerType Nodes, mstd::ContainerType Edges>
    void add_leaf(Index& num_leaves, const Index total_num_leaves, IndexPair& last_idx,
        Leaves& leaf_container, Nodes& may_receive_more_children, Edges& edges) {
      if(num_leaves > 0) {
        --num_leaves;
        add_leaf(total_num_leaves, leaf_container, may_receive_more_children, edges, last_idx);
      }
    }
    // add a new leaf to N, either dangling from an existing node, or from a newly created node (by subdivision)
    template<NodeContainerType Leaves, NodeContainerType Nodes, mstd::ContainerType Edges>
    void add_leaf(Index& num_leaves, const Index total_num_leaves, Leaves& leaf_container, Nodes& may_receive_more_children, Edges& edges) {
      add_leaf(num_leaves, total_num_leaves, IndexPair{0u,0u}, leaf_container, may_receive_more_children, edges);
    }

    // add a new leaf to N, either dangling from an existing node, or from a newly created node (by subdivision)
    template<NodeContainerType Leaves, NodeContainerType Nodes, mstd::ContainerType Edges>
    void add_leaf(const Index total_num_leaves, IndexPair& last_idx,
        Leaves& leaf_container, Nodes& may_receive_more_children, Edges& edges) {
      assert(N->num_nodes() + total_num_leaves <= target_nums.num_nodes);
      // choose to either hang from an existing node or edge
      bool hang_from_node;
      if(N->num_nodes() + total_num_leaves < target_nums.num_nodes) {
        if(may_receive_more_children.empty()) {
          // we cannot hang from a node if none will accept more out-degree
          hang_from_node = false;
        } else hang_from_node = decider(0,1);
      } else hang_from_node = true; //  we cannot subdivide edges if we're low on nodes
      
      if(hang_from_node) {
        assert(may_receive_more_children.size() > last_idx.first);
        const Index idx = decider(last_idx.first, may_receive_more_children.size() - 1);
        last_idx.first = idx;
        const NodeDesc u = may_receive_more_children[idx];
        const NodeDesc v = emplacer.create_node();
        emplacer.emplace_edge_raw(u, v);
        append(edges, u, v);
        append(leaf_container, v);
        // if we hit the out-degree limit, then remove u from the list of possible parents
        if(Network::out_degree(u) == target_nums.tree_out_deg.high())
          mstd::quick_erase(may_receive_more_children, idx);
      } else {
        assert(edges.size() > last_idx.second);
        const Index idx = decider(last_idx.second, edges.size() - 1);
        last_idx.second = idx;
        const auto [u, v] = edges[idx];
        // subdivide the edge
        const NodeDesc w = emplacer.create_node();
        const NodeDesc z = emplacer.create_node();
        emplacer.subdivide_edge(edges[idx], w);
        emplacer.emplace_edge_raw(w, z);
        edges[idx].second = w;
        append(edges, w, v);
        append(edges, w, z);
        append(leaf_container, z);
        if(target_nums.tree_out_deg.high() > 2) append(may_receive_more_children, w);
      }
    }

  };


  // this top-down tree builder repeatedly replaces each leaf with a fan of appropriate degree, but cannot handle unlabelled leaves
  template<StrictPhylogenyType Tree_, class Decider_, class DataExtracter_ = DataExtracter<void>>
  struct LeafReplacementTreeBuilder:
    public TreeBuilder<Tree_, Decider_, DataExtracter_>
  {
    using Parent = TreeBuilder<Tree_, Decider_, DataExtracter_>; 
    using Parent::target_nums;
    using Parent::N;
    using Parent::emplacer;
    using Parent::labelled_leaves;
    using Parent::nums;

    INHERIT_ALL_CONSTRUCTORS(LeafReplacementTreeBuilder, Parent);

    void build_phylogeny() {
      const NodeDesc rt = emplacer.create_root();
      
      mstd::append(labelled_leaves, rt);
      for(int32_t internals_to_go = nums.num_internal(); internals_to_go > 0; --internals_to_go) {
        // after declaring one of the current leaves an inner node, how many leaves do we still need to add?
        const auto leaves_to_go = nums.num_leaves - labelled_leaves.size() + 1;
        assert(leaves_to_go > internals_to_go);
        const auto max_degree = std::min(leaves_to_go - internals_to_go + 1, target_nums.tree_out_deg.high()); // every internal node adds at least 1 leaf
        const auto min_degree = std::max(leaves_to_go / internals_to_go, 2);
        assert(min_degree <= max_degree);
        const auto degree = decider(min_degree, max_degree);
        const auto idx = decider(0, labelled_leaves.size() - 1);
        const NodeDesc u = labelled_leaves[idx];
        std::cout << "adding ["<<min_degree<<":"<<max_degree<<"] --> "<<degree<<" leaves to "<<u<<'\n';
        mstd::quick_erase(labelled_leaves, idx);
        for(size_t j = 0; j != degree; ++j) {
          const NodeDesc v = emplacer.create_node();
          emplacer.emplace_edge_raw(u, v);
          mstd::append(labelled_leaves, v);
        }
      }
    }
  };

  // the top-down tree builder repeatedly partitions the leaf-set onto the children and builds their trees recursively
  template<StrictPhylogenyType Tree_, class Decider_, class DataExtracter_ = DataExtracter<void>>
  struct TopDownTreeBuilder:
    public TreeBuilder<Tree_, Decider_, DataExtracter_>
  {
    using Parent = TreeBuilder<Tree_, Decider_, DataExtracter_>; 
    using IndexVec = typename Parent::IndexVec;
    using Index = typename Parent::Index;
    using Parent::target_nums;
    using Parent::N;
    using Parent::decider;
    using Parent::emplacer;

    struct LeafSet {
      IndexVec labelled_leaves;
      uint32_t num_unlabelled_leaves = 0;

      uint32_t size() const { return labelled_leaves.size() + num_unlabelled_leaves; }
    };


    NodeVec labelled_leaves;
    NodeSet unlabelled_leaves;

    INHERIT_ALL_CONSTRUCTORS(TopDownTreeBuilder,Parent);

    // build a subtree at a specific root recursively by partitioning the leaf-set
    void build_subtree_with_root(const NodeDesc root, const LeafSet& leaves) {
      std::vector<LeafSet> children;
      children.reserve(target_nums.tree_out_deg.high());
      
      // partition the labelled leaves
      bool last_i = leaves.labelled_leaves.empty();
      for(Index i = 0; not last_i; ++i) {
        last_i = (i == leaves.labelled_leaves.size());
        const Index leaf_idx = leaves.labelled_leaves[i];
        // check if the current children already saturate the possible out-degrees
        const bool saturated = (children.size() == target_nums.tree_out_deg.high());
        // if the out-degree is not yet saturated, we may add a new child, indicated by choosing children.size() as index
        // NOTE: if we only have a single child yet and this is the last round, then we HAVE TO add a new child
        const Index child_idx = (last_i and children.size() == 1) ? 1 : decider(0, children.size() - saturated);
        if(child_idx == children.size()) {
          append(children, {leaf_idx}, 0);
        } else append(children[child_idx], leaf_idx);
      }

      // partition the unlabelled leaves by selecting (out-deg - 1) many bars among (out-deg + num_unlabelled_leaves - 1) many stars/bars
      if(leaves.num_unlabelled_leaves > 0) {
        // how many children might get only unlabelled leaves?
        const Degree free_out_degree = target_nums.tree_out_deg.high() - children.size();
        const Degree possible_more_children = std::min(free_out_degree, leaves.num_unlabelled_leaves);
        const Degree children_without_labelled_leaves = decider(0, possible_more_children);
        const Degree children_with_labelled_leaves = children.size();
        const Degree num_children = children_with_labelled_leaves + children_without_labelled_leaves;
        // every child without a labelled leaf gets at least one unlabelled leaf, so remove those from the pool
        const Degree available_unlabelled_leaves = leaves.num_unlabelled_leaves - children_without_labelled_leaves;
        if(available_unlabelled_leaves > 0) {
          const uint64_t num_summations = mstd::compute_binom(num_children + available_unlabelled_leaves - 1, num_children - 1);
          assert(num_summations > 1);
          const uint64_t rnk = decider(0, num_summations - 1);
          const auto index_subset = mstd::unrank_subset(rnk, num_children, available_unlabelled_leaves);
          assert(index_subset.size() == num_children);
          assert(std::ranges::is_sorted(index_subset));

          // distribute unlabelled leaves using the indx-subset
          children.resize(num_children);
          children[0].unlabelled_leaves = index_subset[0];
          for(Index i = 1; i < index_subset.size(); ++i)
            children[i].unlabelled_leaves = index_subset[i] - index_subset[i-1] - 1;
        }
        // add the unlabelled leaves that we removed from the pool onto the corresponding children
        for(Index i = children_with_labelled_leaves; i < num_children; ++i)
          children[i].unlabelled_leaves++;
      }

      // finally, do recursive calls
      for(const LeafSet& child_leaves: children) {
        const NodeDesc v = emplacer.create_node();
        emplacer.emplace_edge_raw(root, v);
        assert(child_leaves.size() > 0);
        // if we have a single leaf, then we ARE that leaf
        if(child_leaves.size() == 1) {
          if(child_leaves.num_unlabelled_leaves == 0) {
            const Index leaf_label_index = mstd::front(child_leaves.labelled_leaves);
            labelled_leaves[leaf_label_index] = v;
          } else append(unlabelled_leaves, v);
        } else build_subtree_with_root(v, child_leaves);
      }
    }

    void build_phylogeny(const LeafSet& leaves) {
      labelled_leaves = NodeVec(target_nums.num_leaves, NoNode),
      build_subtree_with_root(emplacer.create_root(), leaves);
    }
  };

  // the leaf-attaching tree builder repeatedly selects either an edge of a node to hang the next leaf from
  template<StrictPhylogenyType Tree_, class Decider_, class DataExtracter_ = DataExtracter<void>>
  struct LeafAttachingTreeBuilder:
    public TreeBuilder<Tree_, Decider_, DataExtracter_>
  {
    using Parent = TreeBuilder<Tree_, Decider_, DataExtracter_>; 
    using typename Parent::LeafSet;
    using typename Parent::Index;
    using IndexPair = std::pair<Index, Index>;
    using Parent::target_nums;
    using Parent::emplacer;
    using Parent::decider;
    using Parent::N;

    NodeVec labelled_leaves; // NOTE: we track the descriptors of the leaves so that we can assign labels at the very end
    NodeSet unlabelled_leaves; 

    INHERIT_ALL_CONSTRUCTORS(LeafAttachingTreeBuilder,Parent);

    // Another method to build trees is to repeatedly subdivide edges and hanging a number of leaves from the new node
    void build_phylogeny(Index num_unlabelled_leaves) {
      assert(num_unlabelled_leaves < target_nums.num_leaves);
      Index num_labelled_leaves = target_nums.num_leaves - num_unlabelled_leaves;
      NodePairVec edges;
      NodeVec may_receive_more_children;
      const NodeDesc root = emplacer.create_root();
      const NodeDesc x = emplacer.create_node();
      emplacer.emplace_edge_raw(root, x);
      append(edges, root, x);
      append(may_receive_more_children, root);
      append(labelled_leaves, x);

      IndexPair last_idx{0,0};
      while(num_unlabelled_leaves + num_labelled_leaves > 0) {
        // for fairness in the distribution, we should switch between labelled and unlabelled leaves
        add_leaf(num_labelled_leaves, labelled_leaves, may_receive_more_children, edges);
        // track the last idx of selected edges for unlabelled leaves to break symmetry
        add_leaf(num_unlabelled_leaves, unlabelled_leaves, may_receive_more_children, edges, last_idx);
      }
    }
  };


  // this class can build networks by first building trees and then identifying some nodes
  // it can be used to sample a random network with given NodeNums or to enumerate all such networks
  // NOTE: by default, we will generate "reduced networks" (see [Pardi & Scornavacca '15]
  //        in short, a network is reduced if its "funnels" (out-deg = 1) are contracted down
  // NOTE: in particular, the networks may have nodes with high in-degree ("hubs"), representing a chain of reticulations with a tree-node below
  // NOTE: all networks with the same reduction display the same sets of trees, so they are 'equivalent' in that regard
  template<class TreeBuilder_>
  struct ReducedNetworkBuilder:
    public TreeBuilder_
  {
    // ------- static stuff --------
    using Parent = TreeBuilder<Tree_, Decider_, DataExtracter_>; 
    using typename Parent::LeafSet;
    using Parent::target_nums;
    using Parent::N;

    // ------- construction --------
    INHERIT_ALL_CONSTRUCTORS(TopDownTreeBuilder,Parent);
   
    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: query --------
    // ------- methods: modification --------
    // given a subtree with unlabelled leaves, construct a network by selecting a non-ancestor to fuse each unlabelled leaf with
    void fuse_unlabelled_leaves(const NodeSet& unlabelled_leaves) {
      if(not unlabelled_leaves.empty()) {
        // all internal nodes and labelled leaves are eligible to receive an incoming edge
        NodeVec receivers;
        for(const NodeDesc u: N->nodes())
          if(not test(unlabelled_leaves, u))
            append(receivers, u);
        for(const NodeDesc u: unlabelled_leaves) {
          const NodeSet u_ancestors(N->nodes_above(u));
          assert(receivers.size() > u_ancestors.size());
          Index index = decider(0, receivers.size() - u_ancestors.size() - 1);
          // skip ancestors
          NodeDesc target = NoNode;
          for(Index i = 0;; ++i) {
            assert(i != receivers.size());
            target = receivers[i];
            if(not test(u_ancestors, target))
              if(index-- == 0) break;
          }
          assert(Network::in_degree(u) == 1);
          emplacer.emplace_edge_raw(Network::parent(u), target);
          N->delete_node(u);
        }
      }
    }

    auto build_phylogeny() {
      // step 1: create a tree with sufficient unlabelled leaves
      Parent::build_tree(num.reticulation_number);
      fuse_unlabelled_leaves(Parent::unlabelled_leaves);
      return *N;
    }
  };


  // this class can build networks by first building trees and then adding random arcs to them (so ONLY TREE-BASED NETWORKS CAN BE BUILT!)
  // it can be used to sample a random network with given NodeNums or to enumerate all such networks
  // NOTE: by default, we will generate "reduced networks" (see [Pardi & Scornavacca '15]
  //        in short, a network is reduced if its "funnels" (out-deg = 1) are contracted down
  // NOTE: in particular, the networks may have nodes with high in-degree ("hubs"), representing a chain of reticulations with a tree-node below
  // NOTE: all networks with the same reduction display the same sets of trees, so they are 'equivalent' in that regard
  template<class TreeBuilder_>
  struct TreeBasedNetworkBuilder:
    public ReducedNetworkBuilder
  {
    // ------- static stuff --------
    using Parent = ReducedNetworkBuilder<TreeBuilder>;
    using typename Parent::LeafSet;
    using Parent::target_nums;
    using Parent::N;

    // ------- construction --------
    INHERIT_ALL_CONSTRUCTORS(TreeBasedNetworkBuilder,Parent);
   
    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: query --------
    // ------- methods: modification --------

    //! add a number of random edges to a given network, introducing new_tree_nodes new tree nodes and new_reticulations new reticulations
    //NOTE: this may result in a non-binary network
    //NOTE: if N is a tree, new_reticulations may not be zero, but new_tree_nodes may be zero (in this case, we're re-using existing tree nodes)
    //NOTE: if new_tree_nodes == new_reticulations == num_edges, then no old node will be incident with a new edge (old nodes maintain their degrees)
    //NOTE: we'll just add some unlabelled leaves to edges/nodes and then fuse them using the parent's fuser
    template<StrictPhylogenyType Net, EdgeEmplacerType Emplacer>
    void add_random_edges(Index num_edges, Index new_tree_nodes) {
      if(num_edges > 0){
        if(N->num_edges() < 2)
          throw std::logic_error("cannot add edges to a tree/network with less than 2 edges");
        if(new_tree_nodes > num_edges)
          throw std::logic_error("cannot add " + std::to_string(new_tree_nodes) + " new tree nodes with only " + std::to_string(num_edges) + " new edges");

        NodePairVec edges;
        edges.reserve(N->num_edges());

        NodeVec may_receive_more_children;
        may_receive_more_children.reserve(target_nums.num_internal());
        
        for(const NodeDesc x: N->nodes()) {
          for(const NodeDesc y: Network::children(x))
            append(edges, x, y);
          if((Network::out_degree(x) > 0) and (Network::out_degree(x) < target_nums.tree_out_deg.high()))
            append(may_receive_more_children, x);
        }
        
        NodeSet unlabelled_leaves;
        unlabelled_leaves.reserve(num_edges);

        IndexPair last_idx{0,0};
        while(num_edges > 0) {
          add_leaf(num_edges, unlabelled_leaves, may_receive_more_children, edges, last_idx);
        }
        Parent::fuse_unlabelled_leaves(unlabelled_leaves);
      }
    }
  };




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
