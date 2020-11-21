

#pragma once

#include <vector>
#include "utils.hpp"
#include "config.hpp"
#include "label_matching.hpp"
#include "induced_tree.hpp"
#include "matching.hpp"
#include "tree_components.hpp"

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
    using LabelMatching = LabelMatchingFromNets<Host, Guest, std::vector>;
   
  protected:
    const Guest& guest;
    const Host& host;

    std::shared_ptr<NodeInfos> node_infos;
    std::shared_ptr<LabelMatching> HG_label_match;
    DisplayTable table;
    
    const bool HG_label_match_temporary; // indicate whether the label matching is temporary, that is, whether we can move out of it

  public:

    TreeInTreeContainment(const Host& _host,
                          const Guest& _guest,
                          std::shared_ptr<NodeInfos> _node_infos = {},
                          std::shared_ptr<LabelMatching> _HG_label_match = {},
                          const bool _HG_label_match_temporary = false): // indicate whether we can move out of of label matching
      guest(_guest),
      host(_host),
      node_infos(std::move(_node_infos)),
      HG_label_match(std::move(_HG_label_match)),
      HG_label_match_temporary(_HG_label_match_temporary || !HG_label_match)
    {
      if(!HG_label_match) HG_label_match = std::make_shared<LabelMatching>(leaf_labels_only ?
                                      get_leaf_label_matching<Host, Guest, std::vector>(host, guest) :
                                      get_label_matching<Host, Guest, std::vector>(host, guest));
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
    struct induced_subtree_infos
    {
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

    std::function<bool(const Node, const Node)> sort_by_order = [&](const Node a, const Node b)
    {
      return node_infos->at(a).order_number < node_infos->at(b).order_number;
    };

    // construct the base-cases of the DP by matching leaf-labels (remember to tell the matching to store host-labels in a NodeVec (so we can move))
    void construct_base_cases()
    {
      for(auto& HG_pair: seconds(*HG_label_match)){
        assert(HG_pair.second.size() == 1); // assert that the guest is single labeled
        std::flexible_sort(HG_pair.first.begin(), HG_pair.first.end(), sort_by_order);
        std::cout << "base case: "<<HG_pair<<"\n";
        if(HG_label_match_temporary)
          table.try_emplace(front(HG_pair.second), std::move(HG_pair.first));
        else
          table.try_emplace(front(HG_pair.second), HG_pair.first);
      }
    }
    
    void compute_possibilities(const Node u, NodeList& poss)
    {
      // step 1: get the subtree of host induced by the nodes that the children map to
      const NodeVec child_poss = merge_child_poss(u);
      if(!child_poss.empty()){
        std::cout << "building tree induced by "<<child_poss<<"\n";
        CompatibleRWTree<Host, induced_subtree_infos> induced_subhost(policy_noop, host, child_poss, node_infos);
        std::cout << "induced tree:\n"<<induced_subhost<<"\n";

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
      if(!guest.is_leaf(u)){
        std::cout << "merging possibilities of "<<guest.children(u)<<"\n";
        // for degree up to x, merge the child possibilities by linear "inplace_merge", otherwise, merge via iterator-queue in O(n log deg)
        if(guest.out_degree(u) > config::vector_queue_merge_threshold){
          using NodeIter = std::auto_iter<typename NodeList::const_iterator>;
          // NOTE: priority_queue outputs the LARGEST element first, so we'll have to reverse sort_by_order by swapping its arguments
          const auto sort_iter_by_order = [&](const auto& a, const auto& b) -> bool {  return sort_by_order(*b, *a); };
          
          std::priority_queue<NodeIter, std::vector<NodeIter>, decltype(sort_iter_by_order)> iter_queue(sort_iter_by_order);
          size_t total_size = 0;
          // for each child v of u, add an auto iter to its possibility list
          for(const Node v: guest.children(u)){
            const auto& child_poss = who_displays(v);
            if(!child_poss.empty()){
              iter_queue.push(child_poss);
              total_size += child_poss.size();
            } else {
              // if v cannot be displayed, then u cannot be displayed, so return the empty possibility vector
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






  // a class that checks for subtrees of the guest to be displayed in a lowest tree component of the host
  template<class MulSubtree, class HG_Label_Match, bool leaf_labels_only = true>  
  class TreeInTreeComponent
  {
    using RWHost = RWNetwork<>;
    using RWGuest = RWTree<>;

    using TreeChecker = TreeInTreeContainment<MulSubtree, RWGuest, leaf_labels_only>;
    using typename TreeChecker::NodeInfos;
    using typename TreeChecker::LabelMatching;


    const RWGuest& guest;
    
    std::shared_ptr<NodeInfos>      node_infos;
    std::shared_ptr<LabelMatching>  SG_label_match;
    
    MulSubtree  subtree;
    TreeChecker subtree_display;


    // unzip the reticulations below u to create a MuL-tree
    //NOTE: this assumes that the cherry rule has been applied exhaustively
    MulSubtree unzip_retis(const RWHost& host, const HGLabelMatching& HG_label_match, const Node u)
    {
      std::cout << "unzipping reticulations under tree component below "<<x<<"...\n";
      EdgeVec edges;
      typename MulSubtree::LabelMap labels;

      // to construct the multi-labeled tree, we use a special edge-traversal of the host without a SeenSet, so reticulations are visited multiple times
      size_t node_count = 1;
      //NOTE: we cannot simply use the node indices of the host since we may see some nodes multiple times in the MulTree...
      //      thus, we'll need a translate map, a label map, and a label matching between host and subtree
      HashMap<Node, Node> host_to_subtree; // track translation
      append(host_to_subtree, u, 0); // translate the root to 0
      node_infos.try_emplace(0, 0, 0); // distance to root and the order number of u are 0

      MetaTraversal<const RWHost, void, EdgeTraversal> my_dfs(host);
      for(const auto uv: my_dfs.preorder(u)){
        const Node u = uv.tail();
        Node v = uv.head();
        if(!host.is_reti(u)){
          // skip reticulation chains
          while(host.out_degree(v) == 1) v = host.any_child(v);
          std::cout << "got edge "<<u<<"->"<<v<<"\n";
          // translate u & v
          const Node st_u = host_to_subtree[u];
          const Node st_v = host_to_subtree[v] = node_count++;
          std::cout << "translated to "<<st_u<<"->"<<st_v<<" in the subtree\n";
          // add the edge to the subtree
          append(edges, st_u, st_v);
          // set distance to root and order number of st_v
          node_infos.try_emplace(st_v, node_infos[st_u].dist_to_root + 1, st_v); // st_v is its own order number as we're building in preorder
          // register the label if v has one
          const auto& vlabel = host.label(v);
          if((!leaf_labels_only || host.is_leaf(v)) && !vlabel.empty()) {
            // register label in the label map
            append(labels, st_v, vlabel);
            // register label and st_v in the label matching
            const auto [iter, succ] = SG_label_match.try_emplace(vlabel);
            if(succ) append(iter->second.second, HG_label_match[vlabel].second);
            append(iter->second.first, st_v);
            std::cout << "matched labels: "<<iter->second<<"\n";
          }
        }
      }
      return {edges, labels, consecutive_tag()};
    }

  public:
    // construct from
    TreeInTreeComponent(const RWHost& _host, const RWGuest& _guest, const HG_Label_Matching& HG_label_match, const Node u):
      guest(_guest),
      node_infos(std::make_shared<NodeInfos>()),
      SG_label_match(std::make_shared<LabelMatching>()),
      subtree(unzip_retis(_host, HG_label_match, u)),
      subtree_contain(subtree, _guest, node_infos, SG_label_match, true) // note: the checker may move out of the label matching
    {
    }

    // get the highest ancestor of v in the guest that is still displayed by the tree-component
    Node get_highest_displayed_ancestor(Node v)
    {
      // step 1: unzip the lowest reticulations
      Node pv = guest.parent(v);
      std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
      while(1) {
        const auto& pv_disp = subtree_contain.who_displays(pv);
        if(pv_disp.empty()) return v;
        v = pv;
        if(pv == guest.root()) return v;
        if((pv_disp.size() == 1) && (front(pv_disp) == subtree.root())) return v;
        pv = guest.parent(pv);
        std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
      }
    }


  };







  // a containment checker, testing if a single-labelled host-network contains a single-labeled guest tree
  //NOTE: if an invisible tree component is encountered, we will branch on which subtree to display in the component
  //NOTE: this can be used to solve multi-labeled host networks: just add a reticulation for each multiply occuring label
  template<class Host, class Guest, bool leaf_labels_only = true,
    class = std::enable_if_t<Guest::is_single_labeled>,
    //class = std::enable_if_t<Host::is_single_labeled>,
    class = std::enable_if_t<Guest::is_declared_tree>>
  class TreeInNetContainment
  {
    // we'll work with mutable copies of the network & tree, which can be given by move
    using RWHost = RWNetwork<>;
    using RWGuest = RWTree<>;
    using CompGraph = RWNetwork<>;

    using MulSubtree = CompatibleRO<CompatibleMulTree<Host>>;
    using TreeChecker = TreeInTreeContainment<MulSubtree, RWGuest, leaf_labels_only>;
    using ComponentInfos = TreeComponentInfos<RWHost>;

    using HostEdge = typename RWHost::Edge;
    using NodeList = _NodeList<RWHost>;
    using DisplayTable = _DisplayTable<RWHost, RWGuest::template NodeMap>;
    using LabelMatching = LabelMatchingFromNets<RWHost, RWGuest, std::vector>;
    using LabelToGuest = std::unordered_map<typename RWGuest::LabelType, Node>;


    RWHost host;
    RWGuest guest;
    LabelMatching HG_label_match = {host, guest};

    // NOTE: we want to apply cherry reduction before initializing comp_infos, so this looks a little weird...
    ComponentInfos comp_infos = simple_cherry_reduction() ? host : host;
    CompGraph tree_comp_DAG = comp_infos.component_graph_edges;
    bool failed = false;

    // initialization and early cherry reduction
    void init()
    {
      // if host has only 1 tree component, then its root has index 0 while the host's root is not necessarily 0... so we fix this.
      if(tree_comp_DAG.edgeless()) {
        const Node tc_root = tree_comp_DAG.root();
        tree_comp_DAG.add_child(tc_root, host.root());
        tree_comp_DAG.remove_node(tc_root);
      }
      // now, the component DAG's root should also be the host's root
      assert(tree_comp_DAG.root() == host.root());

      std::cout << "applying visible-component reduction...\n";      
      vis_comp_reduction();
    }

    bool simple_cherry_reduction()
    {
      std::cout << "applying cherry reduction...\n";
      bool result = false;
      for(auto label_iter = HG_label_match.begin(); label_iter != HG_label_match.end();){
        auto& uv = label_iter->second;
        // if the label only exists in the guest, but not in the host, then the host can never display the guest!
        if(uv.first.empty()) return failed = true; // this label occurs only in the guest, so the host can never display it
        if(uv.second.empty()) {
          // this label occurs only in the host, but not in the guest, so we can simply remove it, along with the entry in the label matching
          std::cout << "removing label "<<label_iter->first<<" from the host since it's not in the guest\n";
          host.remove_upwards(uv.first);
          label_iter = HG_label_match.erase(label_iter);
        } else if(!simple_cherry_reduction_from(uv)) {
          result = false;
          ++label_iter;
        } else result = true;
      }
      std::cout << "after cherry reduction:\nhost:\n"<<host<<"\nguest:\n"<<guest<<"\n\n";
      return result;
    }


  public:

    // initialization allows moving Host and Guest
    //NOTE: to allow deriving Host and Guest template parameters, we're not using forwarding references here, but 4 different constructors
    TreeInNetContainment(const Host& _host, const Guest& _guest): host(_host), guest(_guest)  { init(); }
    TreeInNetContainment(const Host& _host, Guest&& _guest): host(_host), guest(std::move(_guest)) { init(); }
    TreeInNetContainment(Host&& _host, const Guest& _guest): host(std::move(_host)), guest(_guest) { init(); }
    TreeInNetContainment(Host&& _host, Guest&& _guest): host(std::move(_host)), guest(std::move(_guest)) { init(); }

    bool simple_cherry_reduction_from(typename LabelMatching::mapped_type& HG_label_pair)
    {
      std::cout << "checking label matching of pair "<<HG_label_pair<<"\n";
      auto& u = HG_label_pair.first;
      if(host.in_degree(u) != 1) return false;
      const Node pu = host.parent(u);
      if(host.in_degree(pu) > 1) return false;
      
      auto& v = HG_label_pair.second;
      assert(guest.in_degree(v) == 1);
      const Node pv = guest.parent(v);
      if(guest.in_degree(pv) > 1) return false;
      if(host.out_degree(pu) != guest.out_degree(pv)) return false;

      // degrees match, so see if the labels of the children match
      HashSet<typename RWGuest::LabelType> seen;
      for(const Node x: guest.children(pv)) {
        if(guest.out_degree(x) != 0) return false;
        append(seen, guest.label(x));
      }
      for(const Node x: host.children(pu)){
        if(host.out_degree(x) != 0) return false;
        if(!seen.erase(host.label(x))) return false;
      }
      if(!seen.empty()) return false;

      std::cout << "found cherry at "<<pu<<" (host) and "<<pv<<" (guest)\n";

      // the childrens labels match, so remove all children and suppress both pu (in host) and pv (in guest)
      for(const Node x: guest.children(pv))
        if(x != v) HG_label_match.erase(guest.label(x));
      std::cout << "contracting cherry in host\n";
      host.remove_subtree_except(pu, u, true);
      host.suppress_node(pu);

      std::cout << "contracting cherry in guest\n";
      guest.remove_subtree_except(pv, v, true);
      guest.suppress_node(pv);
      return true;
    }

    bool simple_cherry_reduction_for_label(const typename RWHost::LabelType& label)
    {
      return label.empty() ? false : simple_cherry_reduction_from(HG_label_match.at(label));
    }

#warning: TODO: implement general cherry reduction (1. uv is cherry in guest and 2. exists lowest ancestor x of u & v s.t. x visible on v (tree-path f.ex.) and the x-->u path is unqiue) before resorting to branching

    // unzip the reticulations below x to create a MuL-tree
    //NOTE: this assumes that the cherry rule has been applied exhaustively
    MulSubtree unzip_retis(const Node x, typename TreeChecker::NodeInfos& node_infos, typename TreeChecker::LabelMatching& SG_label_match)
    {
      std::cout << "unzipping reticulations under tree component below "<<x<<"...\n";
      EdgeVec edges;
      typename MulSubtree::LabelMap labels;

      // to construct the multi-labeled tree, we use a special edge-traversal of the host without a SeenSet, so reticulations are visited multiple times
      size_t node_count = 1;
      //NOTE: we cannot simply use the node indices of the host since we may see some nodes multiple times in the MulTree...
      //      thus, we'll need a translate map, a label map, and a label matching between host and subtree
      HashMap<Node, Node> host_to_subtree; // track translation
      append(host_to_subtree, x, 0); // translate the root to 0
      node_infos.try_emplace(0, 0, 0); // distance to root and the order number of x are 0

      MetaTraversal<const RWHost, void, EdgeTraversal> my_dfs(host);
      for(const auto uv: my_dfs.preorder(x)){
        const Node u = uv.tail();
        Node v = uv.head();
        if(!host.is_reti(u)){
          // skip reticulation chains
          while(host.is_reti(v)) v = front(host.children(v));
          std::cout << "got edge "<<u<<"->"<<v<<"\n";
          // translate u & v
          const Node st_u = host_to_subtree[u];
          const Node st_v = host_to_subtree[v] = node_count++;
          std::cout << "translated to "<<st_u<<"->"<<st_v<<" in the subtree\n";
          // add the edge to the subtree
          append(edges, st_u, st_v);
          // set distance to root and order number of st_v
          node_infos.try_emplace(st_v, node_infos[st_u].dist_to_root + 1, st_v); // st_v is its own order number as we're building in preorder
          // register the label if v has one
          const auto& vlabel = host.label(v);
          if((!leaf_labels_only || host.is_leaf(v)) && !vlabel.empty()) {
            // register label in the label map
            append(labels, st_v, vlabel);
            // register label and st_v in the label matching
            const auto [iter, succ] = SG_label_match.try_emplace(vlabel);
            if(succ) append(iter->second.second, HG_label_match[vlabel].second);
            append(iter->second.first, st_v);
            std::cout << "matched labels: "<<iter->second<<"\n";
          }
        }
      }
      return {edges, labels, consecutive_tag()};
    }

    // remove the component tree below x (except for the root), returning all reticulations that may now render someone visible
    void prune_host(const Node x)
    {
      std::cout << "pruning "<<x<<"\n";
      const auto& hC = host.children(x);
      while(!hC.empty()) {
        const Node y = front(hC);
        if(host.out_degree(y) != 0) {
          if(host.in_degree(y) > 1) {
            assert(host.out_degree(y) == 1);
            host.remove_edge(x, y);
            if(host.in_degree(y) == 1) {
              // if by deleting xy, we turned y into a suppressible node, then suppress y (but also copy the comp root of y's parent to y's child)
              const Node py = host.parent(y);
              const Node z = host.any_child(y);

              const auto rt_iter = comp_infos.component_root_of.find(py);
              if(rt_iter != comp_infos.component_root_of.end()){
                if(host.in_degree(z) == 1) {
                  comp_infos.component_root_of.try_emplace(z, rt_iter->second);
                  // if the child z of y is also a leaf, then register that the component root is now stable on that leaf
                  if(host.out_degree(z) == 0)
                    comp_infos.visible.try_emplace(rt_iter->second, z);
                }
              }
              std::cout << "suppressing "<<y<<" with parent "<<host.parents(y)<<" & children "<<host.children(y)<<"\n";
              host.suppress_node(y);

              std::cout << z << " is a leaf? "<<host.is_leaf(z)<<" & "<<py<<" is tree-node? "<<host.is_tree_node(py)<<"\n";
              // if the child of y is a leaf and the parent of y is a tree node, then try to apply cherry redution
              if(host.is_leaf(z) && host.is_tree_node(py)){
                const auto z_label_iter = auto_find(host.labels(), z);
                if(z_label_iter)
                  simple_cherry_reduction_for_label(z_label_iter->second);
              }
            }
          } else {
            prune_host(y);
            host.remove_node(y);
          }
        } else host.remove_node(y);
      }
    }

    // prune the guest below Node x, 
    // ATTENTION: also remove leaves of the host that have a label below x in the guest!!!
    // ATTENTION: also remove the corresponding entries in HG_label_match
    void prune_guest(const Node x, NodeVec& prune_host_leaves)
    {
      const auto& gC = guest.children(x);
      while(!gC.empty()) {
        const Node y = front(gC);
        if(guest.is_leaf(y)){
          // if y is a leaf, then remove y from guest, remove the corresponding leaf from host, and remove the label-matching entry
          const auto HG_match_iter = HG_label_match.find(guest.label(y));
          assert(HG_match_iter->second.second == y);
          
          append(prune_host_leaves, HG_match_iter->second.first);
          HG_label_match.erase(HG_match_iter);
        }
        prune_guest(y, prune_host_leaves);
        guest.remove_node(y);
      }
    }


    // get the highest ancestor of v in the guest that is still displayed by the network below u, given that u is visible from 'visible_leaf'
    Node get_highest_displayed_ancestor(const Node u, Node v, const Node visible_leaf)
    {
      // step 1: unzip the lowest reticulations
      auto node_infos = std::make_shared<typename TreeChecker::NodeInfos>();
      auto SG_label_match = std::make_shared<typename TreeChecker::LabelMatching>();
      const MulSubtree subtree = unzip_retis(u, *node_infos, *SG_label_match);
      std::cout << "\nunzipped to:\n"<<subtree<<"[visible leaf: "<<visible_leaf<<" ("<<host.label(visible_leaf)<<")]\nguest:\n"<<guest<<"\nnode-infos (order#, root-dist): "<<*node_infos<<"\nlabel-match: "<<*SG_label_match<<"\n";

      TreeChecker subtree_contain(subtree, guest, node_infos, SG_label_match, true); // note: the checker may move out of the label matching
      std::cout << "constructed Tree-in-Tree checker\n";

      Node pv = guest.parent(v);
      std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
      while(1) {
        const auto& pv_disp = subtree_contain.who_displays(pv);
        if(pv_disp.empty()) return v;
        v = pv;
        if(pv == guest.root()) return v;
        if((pv_disp.size() == 1) && (front(pv_disp) == subtree.root())) return v;
        pv = guest.parent(pv);
        std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
      }
    }

    bool treat_comp_root(const Node u, const Node visible_leaf)
    {
      if(visible_leaf != NoNode){
        const auto& vlabel = host.label(visible_leaf);
        const auto uv_label_iter = HG_label_match.find(vlabel);
        assert(uv_label_iter != HG_label_match.end());

        // step 2: get the highest ancestor v of vis_leaf in T s.t. T_v is still displayed by N_u
        const Node v = get_highest_displayed_ancestor(u, uv_label_iter->second.second, visible_leaf);
        std::cout << v << " is the highest displayed ancestor (or it's the root) - label: "<<vlabel<<"\n";
        // step 3: replace both N_u and T_v by a leaf with the label of vis_leaf

        // step 3a: prune guest and remove nodes with label below v (in guest) from both host and guest - also remove their label entries in HG_label_match
        NodeVec prune_host_leaves;
        if(guest.is_leaf(v)) {
          // the highest thing that we could display is a leaf: mark the corresponding leaf in host to be deleted and remove the label_map entry
          prune_host_leaves.push_back(uv_label_iter->second.first);
          HG_label_match.erase(uv_label_iter);
        } else prune_guest(v, prune_host_leaves);
        std::cout << "pruned guest:\n"<<guest<<"\n";

        // step 3b: remove the leaves in host that have been marked for removal and take their reticulations with them
        std::cout << "removing leaves from host (with their reticulations): "<<prune_host_leaves<<"\n";
        for(const Node x: prune_host_leaves)
          host.remove_upwards_except(x, std::DynamicEqualPredicate<Node>(u)); // avoid removing u when going upwards from the leaves

        // step 3c: prune host
        //NOTE: keep track of nodes in the host who have one of their incoming edges removed as component roots may now see them
        prune_host(u);
        std::cout << "pruned host:\n"<<host<<"\n";

        // step 4: update labels & label maps
        std::cout << "marking "<<v<<" (guest) & "<<u<<" (host) with label "<<vlabel<<"\n";
        guest.set_label(v, vlabel);
        host.set_label(u, vlabel);
        HG_label_match.try_emplace(vlabel, u, v);

        return true;
      } else throw std::logic_error("host is not tree-component visible");
    }


    // visible tree-component reduction: find a lowest visible tree component C and reduce it in O(|C|) time; return if network/tree changed
    bool vis_comp_reduction()
    {
      std::cout << "using tree-component graph:\n"<<tree_comp_DAG<<"\n";
      bool result = false;
      // check all leaves of the component-tree to find one that is visible
      NodeVec leaf_component_roots = tree_comp_DAG.leaves().to_container<NodeVec>();
      while(!leaf_component_roots.empty()){
        const Node x = back(leaf_component_roots);  leaf_component_roots.pop_back(); 
        std::cout << "treating component-root "<<x<<" - remaining: "<<leaf_component_roots<<"\n";
        const auto vis_it = comp_infos.visible.find(x);
        if(vis_it != comp_infos.visible.end()) {
          std::cout << "visible on leaf "<<*vis_it<<"\n";
          if(treat_comp_root(x, vis_it->second)){
            std::cout << "successfully treated comp root "<<x<<"\n";
            // update the leaf-components of the tree-component-DAG and delete x from it
            for(const Node y: tree_comp_DAG.parents(x))
              if(tree_comp_DAG.out_degree(y) == 1){
                std::cout << "registering new leaf component "<<y<<"\n";
                leaf_component_roots.push_back(y);
              }
            std::cout << "removing node "<<x<<" from component-root-DAG\n"<<tree_comp_DAG<<"\n";
            tree_comp_DAG.remove_node(x);
            result = true;
          }
        }
      }
      return result;
    }

    bool displayed()
    {
      std::cout << "number of edges: "<<host.num_edges() << " (host) "<<guest.num_edges()<<" (guest)\n";
      if(failed) return false;
      if(host.edgeless()){
        if(guest.edgeless())
          return host.label(host.root()) == guest.label(guest.root());
        else
          return false;
      } else {
        // step 1: 
        assert(false); // not implemented yet
        exit(-1);
      }
    }

  };


}
