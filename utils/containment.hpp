

#pragma once

#include <vector>
#include "utils.hpp"
#include "config.hpp"
#include "label_matching.hpp"
#include "induced_tree.hpp"
#include "matching.hpp"

namespace PT {
 
  //NOTE: if the host is single-labelled, we'll be happy to have the DisplayTable point to singleton_sets instead of vectors of nodes
  template<class Host>
  using _NodeList = LabelNodeStorage<typename Host::LabelTag, std::vector>;
  // our table assigns each node in the guest a list of nodes in the host
  template<class Host, template<class> class _NodeMap>
  using _DisplayTable = _NodeMap<_NodeList<Host>>;
  // we'll need some infos on the nodes, in particular, their depth (dist to root) and some order number
  template<class Host>
  using _NodeInfos = InducedSubtreeInfoMap<Host>;
 

  // a containment checker, testing if a (possibly multi-labelled) host-tree contains a single-labeled guest tree
  template<class Host, class Guest, bool leaf_labels_only = true,
    class = std::enable_if_t<Guest::is_single_labeled>,
    class = std::enable_if_t<Guest::is_declared_tree>>
  class TreeInTreeContainment
  {
  public:
    using NodeList = _NodeList<Host>;
    using NodeInfos = _NodeInfos<Host>;
    using DisplayTable = _DisplayTable<Host, Guest::template NodeMap>;
    using LabelMatching = LabelMatchingFromNets<Guest, Host, std::vector>;
   
  protected:
    const Guest& guest;
    const Host& host;

    std::shared_ptr<NodeInfos> node_infos;
    std::shared_ptr<LabelMatching> lmatch;
    DisplayTable table;
    
    const bool lmatch_temporary; // indicate whether the label matching is temporary, that is, whether we can move out of it

  public:

    TreeInTreeContainment(const Host& _host,
                          const Guest& _guest,
                          std::shared_ptr<NodeInfos> _node_infos = {},
                          std::shared_ptr<LabelMatching> _lmatch = {},
                          const bool _lmatch_temporary = false): // indicate whether we can move out of of label matching
      guest(_guest),
      host(_host),
      node_infos(std::move(_node_infos)),
      lmatch(std::move(_lmatch)),
      lmatch_temporary(_lmatch_temporary || !lmatch)
    {
      if(!lmatch) lmatch = std::make_shared<LabelMatching>(leaf_labels_only ?
                                      get_leaf_label_matching<Guest, Host, std::vector>(guest, host) :
                                      get_label_matching<Guest, Host, std::vector>(guest, host));
      make_node_infos(_host, node_infos);
      std::cout << "order numbers: ";
      for(const auto& x: *node_infos) std::cout << x.first << ":" << x.second.order_number << " "; std::cout << "\n";
      construct_base_cases();
    }


    // lookup where u (guest node) could be hosted; if u is not in the DP table yet, compute the entry
    const NodeList& who_displays(const Node u)
    {
      std::cout << "who displays "<<u<<"? ";
      const auto [iter, success] = table.try_emplace(u);
      if(success){
        std::cout << "\n";
        compute_possibilities(u, iter->second);
      } else std::cout << iter->second << "\n";
      return iter->second;
    }
    
    bool displayed() { return !who_displays(guest.root()).empty(); }

  protected:
    // in the subtree induced by the child possibilities, we'll need to keep track of which child of u can be displayed by one of our own children
    using MatchingPossibilities = HashMap<Node, std::unordered_bitset>;
    struct induced_subtree_infos {
      MatchingPossibilities nodes_for_poss;
      // try to register a new poss type; if the node has not already seen a poss of this type,
      //    then return the new # children of u for whom we have a possible child
      size_t register_child_poss(const Node child, const Node u_child)
      {
        const auto [it, success] = nodes_for_poss.try_emplace(u_child);
        it->second.set(child);
        std::cout << "marking that "<<child<<" displays "<<u_child<<" poss' of "<<u_child<<" currently: "<<it->second<<" (size "<<it->second.size()<<")\n";
        return success ? nodes_for_poss.size() : 0;
      }
      // mark the node uninteresting (by clearing node_for_poss), return whether it was already cleared before
      bool mark_uninteresting()
      {
        if(nodes_for_poss.empty()) return true;
        nodes_for_poss.clear();
        return false;
      }
    };

    std::function<bool(const Node,const Node)> sort_by_order = [&](const Node a, const Node b) {
      return node_infos->at(a).order_number > node_infos->at(b).order_number;
    };

    // construct the base-cases of the DP by matching leaf-labels (remember to tell the matching to store host-labels in a NodeVec (so we can move))
    void construct_base_cases()
    {
      for(auto& node_pair: seconds(*lmatch)){
        assert(node_pair.first.size() == 1);
        std::flexible_sort(node_pair.second.begin(), node_pair.second.end(), sort_by_order);
        std::cout << "base case: "<<node_pair<<"\n";
        if(lmatch_temporary)
          table.try_emplace(front(node_pair.first), std::move(node_pair.second));
        else
          table.try_emplace(front(node_pair.first), node_pair.second);
      }
    }
    
    void compute_possibilities(const Node u, NodeList& poss)
    {
      // step 1: get the subtree of host induced by the nodes that the children map to
      const NodeVec child_poss = merge_child_poss(u);
      if(!child_poss.empty()){
        std::cout << "building tree induced by "<<child_poss<<"\n";
        CompatibleRWTree<Host, induced_subtree_infos> induced_subhost(policy_noop, host, child_poss, node_infos);

        // step 2: find all nodes v such that each child of u has a possibility that is seen by a distinct leaf of v
        // register the possibilities for all but one child of u
        for(const Node u_child: guest.children(u)){
          for(Node v_child: who_displays(u_child)){
            // move upwards from v_child until we reach a node that's already seen a possibility for u_child
            Node v_parent = induced_subhost.parent(v_child);
            while(induced_subhost[v_parent].register_child_poss(v_child, u_child) && (v_parent != induced_subhost.root())){
              v_child = v_parent;
              v_parent = induced_subhost.parent(v_parent);
            }
          }
        }
        
        // step 3: go through the induced_subhost in postorder(!) and check containment for eligible nodes
        for(Node v: induced_subhost.dfs().postorder()){
          const auto& v_infos = induced_subhost[v];
          if(v_infos.nodes_for_poss.size() == guest.out_degree(u)){ // if all children of u have a child of u that can display them
            std::cout << "making bipartite matching from "<<v_infos.nodes_for_poss<<"\n";
            if(perfect_child_matching(v_infos.nodes_for_poss)){ // if each child of v can be displayed by a different child of u
              // H_v displays G_u \o/ - register and mark all ancestors uninteresting, so we don't run matching on them in the future
              poss.push_back(v);
              // if someone else already marked v unintersting, all ancestors are already marked as well
              while((v != induced_subhost.root()) && !induced_subhost[v = induced_subhost.parent(v)].mark_uninteresting());
            }
          }
        }
        // make sure the nodes are in the correct order
        std::flexible_sort(poss.begin(), poss.end(), sort_by_order);
      } else poss.clear();

      std::cout << "found that "<< u << " is displayed at "<<poss<<"\n";
    }

    // return whether all children of u can be matched using the possibilities given
    bool perfect_child_matching(const MatchingPossibilities& poss)
    {
      return bipartite_matching<Node, MatchingPossibilities>(poss).maximum_matching().size() == poss.size();
    }

    // merge the mapping possibilities of all childs into one vector; unless one of the children cannot be mapped, in which case return the empty vector
    NodeVec merge_child_poss(const Node u)
    {
      // if u is a leaf, it should be managed by the base case, unless its label is not in the host, in which case it's not displayed
      NodeVec poss;
      std::cout << "merging possibilities of "<<guest.children(u)<<"\n";
      if(!guest.is_leaf(u)){
        // for degree up to x, merge the child possibilities by linear "inplace_merge", otherwise, merge via iterator-queue in O(n log deg)
        if(guest.out_degree(u) > config::vector_queue_merge_threshold){
          const auto sort_iter_by_order = [&](const auto& a, const auto& b) -> bool {  return sort_by_order(*a, *b); };
          using NodeIter = std::auto_iter<typename NodeList::const_iterator>;
          
          std::priority_queue<NodeIter, std::vector<NodeIter>, decltype(sort_iter_by_order)> iter_queue(sort_iter_by_order);
          size_t total_size = 0;
          for(const Node v: guest.children(u)){
            const auto& child_poss = who_displays(v);
            if(!child_poss.empty()){
              iter_queue.push(child_poss);
              total_size += child_poss.size();
            } else {
              poss.clear();
              return poss;
            }
          }
          poss.reserve(total_size);
          while(!iter_queue.empty()) {
            auto next_iter = iter_queue.top(); iter_queue.pop();
            poss.push_back(*next_iter);
            if(++next_iter) iter_queue.push(next_iter);
          }
        } else {
          for(const Node v: guest.children(u)) {
            const NodeList& v_poss = who_displays(v);
            if(!v_poss.empty()){
              const size_t old_size = poss.size();
              std::cout << "merging "; for(const auto& x: poss) std::cout << x <<":"<<node_infos->at(x).order_number<<" ";
              std::cout << "\t&\t";  for(const auto& x: v_poss) std::cout << x <<":"<<node_infos->at(x).order_number<<" "; std::cout << "\n";
              
              poss.insert(poss.end(), v_poss.begin(), v_poss.end());
              std::inplace_merge(poss.begin(), poss.begin() + old_size, poss.end(), sort_by_order);
            } else {
              poss.clear();
              return poss;
            }
          }
        }
      }
      return poss;
    }


  };

  // a containment checker, testing if a single-labelled component-visible(!)  host-network contains a single-labeled guest tree
  //NOTE: this can be used to solve multi-labeled host networks: just add a reticulation for each multiply occuring label
  template<class Host, class Guest, bool leaf_labels_only = true,
    class = std::enable_if_t<Guest::is_single_labeled>,
    //class = std::enable_if_t<Host::is_single_labeled>,
    class = std::enable_if_t<Guest::is_declared_tree>>
  class TreeInNetContainment
  {
    // we'll work with mutable copies of the network & tree, which can be given by move
    using RWHost = CompatibleRW<Host>;
    using RWGuest = CompatibleRW<Guest>;

    using HostEdge = typename RWHost::Edge;
    using NodeList = _NodeList<RWHost>;
    using DisplayTable = _DisplayTable<RWHost, RWGuest::template NodeMap>;
    using LabelMatching = LabelMatchingFromNets<RWGuest, RWHost, std::vector>;
    using LabelToGuest = std::unordered_map<typename RWGuest::LabelType, Node>;

    RWHost host;
    RWGuest guest;

    LabelToGuest label_to_guest;

    using MulSubtree = CompatibleRO<CompatibleMulTree<Host>>;
    using TreeChecker = TreeInTreeContainment<MulSubtree, RWGuest, leaf_labels_only>;

    void init()
    {
      std::cout << "inversing guest labeling...\n";
      for(const auto& lpair: guest.labels())
        if(!lpair.second.empty())
          if(!label_to_guest.try_emplace(lpair.second, lpair.first).second)
            throw std::logic_error("cannot check containment of multi-labeled guest trees");
      std::cout << "applying cherry reduction...\n";
      cherry_reduction();
      std::cout << "applying visible-component reduction...\n";      
      vis_comp_reduction();
    }
  public:

    // initialization allows moving Host and Guest
    //NOTE: to allow deriving Host and Guest template parameters, we're not using forwarding references here, but 4 different constructors
    TreeInNetContainment(const Host& _host, const Guest& _guest): host(_host), guest(_guest) { init(); }
    TreeInNetContainment(const Host& _host, Guest&& _guest): host(_host), guest(std::move(_guest)) { init(); }
    TreeInNetContainment(Host&& _host, const Guest& _guest): host(std::move(_host)), guest(_guest) { init(); }
    TreeInNetContainment(Host&& _host, Guest&& _guest): host(std::move(_host)), guest(std::move(_guest)) { init(); }

    bool cherry_reduction()
    {
      return true;
    }

    // unzip the reticulations below x to create a MuL-tree; also return a leaf that is visible from x
    MulSubtree unzip_retis(const Node x, Node& visible_leaf, typename TreeChecker::NodeInfos& node_infos, typename TreeChecker::LabelMatching& lmatch)
    {
      std::cout << "unzipping reticulations...\n";
      EdgeVec edges;
      typename MulSubtree::LabelMap labels;

      // to construct the multi-labeled tree, we use a special edge-traversal of the host without a SeenSet, so reticulations are visited multiple times
      size_t node_count = 1;
      HashMap<Node, Node> host_to_subtree; // track translation
      std::unordered_bitset visible; // track visibility
      append(host_to_subtree, x, 0); // translate the root to 0
      node_infos.try_emplace(0, 0, 0); // distance to root and the order number of x are 0
      visible.set(x);

      MetaTraversal<const RWHost, void, EdgeTraversal> my_dfs(host);
      for(const auto uv: my_dfs.preorder(x)){
        const Node u = uv.tail();
        Node v = uv.head();
        if(!host.is_reti(u)){
          // skip reticulation chains
          while(host.is_reti(v)) v = front(host.children(v));
          std::cout << "got edge "<<u<<"->"<<v<<"\n";
          // translate u & v
          const Node st_u(host_to_subtree.at(u));
          const Node st_v(node_count++);
          std::cout << "translated to "<<st_u<<"->"<<st_v<<" in the subtree\n";
          // register the new node-translation
          host_to_subtree[v] = st_v;
          // add the edge to the subtree
          append(edges, st_u, st_v);
          // set distance to root and order number of st_v
          node_infos.try_emplace(st_v, node_infos.at(st_u).dist_to_root + 1, st_v); // st_v is its own order number as we're building in preorder
          // register the label if v has one
          const auto& vlabel = host.label(v);
          if(!leaf_labels_only || host.is_leaf(v))
            if(!vlabel.empty()) {
              // register label in the label map
              append(labels, st_v, vlabel);
              // register label and st_v in the label matching
              const auto [iter, succ] = lmatch.try_emplace(vlabel);
              if(succ) iter->second.first = label_to_guest.at(vlabel);
              append(iter->second.second, st_v);
              std::cout << "matched labels: "<<iter->second<<"\n";
            }
          // check if v is visible from x
          if(!visible.test(v)){
            for(const Node pv: host.parents(v))
              if(!visible.test(pv)) goto invis;
            visible.set(v);
            // if v is a leaf and visible, register it
            if(host.is_leaf(v)) visible_leaf = v;
          invis:;
          }
        } else {
          // if u is a reti, v should not be an inner tree node (otherwise, there is a deeper tree component!)
          assert(!host.is_inner_tree_node(v));
        }
      }
      return MulSubtree(edges, labels);
    }

    bool check_comp_tree_root(const Node u)
    {
      // step 1: unzip the lowest reticulations and get a visible leaf
      auto node_infos = std::make_shared<typename TreeChecker::NodeInfos>();
      auto lmatch = std::make_shared<typename TreeChecker::LabelMatching>();
      Node vis_leaf = NoNode;
      const MulSubtree subtree = unzip_retis(u, vis_leaf, *node_infos, *lmatch);
      std::cout << "\nunzipped to:\n"<<subtree<<"[visible leaf: "<<vis_leaf<<" ("<<host.label(vis_leaf)<<")]\n";
      if(vis_leaf != NoNode){
        // step 2: get the highest ancestor v of vis_leaf in T s.t. T_v is still displayed by N_u
        TreeChecker subtree_contain(subtree, guest, node_infos, lmatch, true); // note: the checker may move out of the label matching
        std::cout << "constructed Tree-in-Tree checker\n";
        const auto& vlabel = host.label(vis_leaf);
        Node& v = label_to_guest.at(vlabel);
        Node pv = guest.parent(v);
        std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
        while(!subtree_contain.who_displays(pv).empty()) {
          v = pv;
          if(pv == guest.root()) break;
          pv = guest.parent(pv);
          std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
        }
        std::cout << pv << " is no longer displayed (or it's the root); last node displayed by "<<u<<" is "<<v<<" (label "<<vlabel<<")\n";
        // step 3: replace both N_u and T_v by a leaf with the label of vis_leaf
        const auto& hC = host.children(u);
        while(!hC.empty()) {
          const Node x = front(hC);
          if(host.is_reti(x)) host.remove_edge(u, x); else host.remove_subtree(x);
        }
        std::cout << "pruned host:\n"<<host<<"\n";
        const auto& gC = guest.children(v);
        while(!gC.empty()) guest.remove_subtree(front(gC));
        std::cout << "pruned guest:\n"<<guest<<"\n";
        // step 4: update labels & label maps
        std::cout << "marking "<<v<<" (guest) & "<<u<<" (host) with label "<<vlabel<<"\n";
        guest.label(v) = vlabel;
        label_to_guest[vlabel] = v;
        host.label(u) = vlabel;
        return true;
      } else throw std::logic_error("host is not tree-component visible");
    }

    // visible tree-component reduction: find a lowest visible tree component C and reduce it in O(|C|) time; return if network/tree changed
    //NOTE: this relies on the fact that post-order DFS-iterators are not invalidated by any operation on the rooted subtree
    bool vis_comp_reduction()
    {
      bool result = false;
      const Node root = host.root();
      auto dfs = host.dfs().postorder();
      auto u_it = dfs.begin();
      while(1){
        const Node u = *u_it; 
        if(u != root) ++u_it; else break;
        std::cout << "checking node "<< u << "\n";
        if(host.is_inner_tree_node(u)){
          const Node pu = host.parent(u);
          std::cout << u << " is inner tree with parent "<<pu<<"\n";
          if(host.is_reti(pu)){ // u is a tree-component root
            std::cout << "parent "<<pu<<" of "<<u<<" is reti\n";
            result |= check_comp_tree_root(u);
          }
        }
      }
      result |= check_comp_tree_root(root);
      return result;
    }

    bool displayed()
    {
      std::cout << "number of edges: "<<host.num_edges() << " (host) "<<guest.num_edges()<<" (guest)\n";
      if(host.edgeless()){
        if(guest.edgeless())
          return host.label(host.root()) == guest.label(guest.root());
        else
          return false;
      } else {
        assert(false); // not implemented yet
        exit(-1);
      }
    }

  };


}
