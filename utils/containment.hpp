
#pragma once

#include <vector>
#include "utils.hpp"
#include "config.hpp"
#include "label_matching.hpp"
#include "auto_iter.hpp"
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
      std::cout << "label matching: "<<*HG_label_match<<"\ntemporary? "<<HG_label_match_temporary<<"\n";
      make_node_infos(_host, node_infos);
      std::cout << node_infos->size() << " node infos: " << *node_infos<<"\n";
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
        std::cout << "marking that "<<child<<" displays "<<u_child<<" poss' of "<<u_child<<" currently: "<<it->second<<"\n";
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
        if(guest.out_degree(u) > 1){
          std::cout << "building tree induced by "<<child_poss<<"\n";
          CompatibleRWTree<Host, induced_subtree_infos> induced_subhost(policy_noop, host, child_poss, node_infos);
          std::cout << "induced tree:\n"<<induced_subhost<<"\n";
          if(!induced_subhost.edgeless()) {
            // step 2: find all nodes v such that each child of u has a possibility that is seen by a distinct leaf of v
            // register the possibilities for all but one child of u
            for(const Node u_child: guest.children(u)){
              for(Node v_child: who_displays(u_child)){
                // move upwards from v_child until we reach a node that's already seen a possibility for u_child
                while(v_child != induced_subhost.root()){
                  const Node v_parent = induced_subhost.parent(v_child);
                  if(!induced_subhost[v_parent].register_child_poss(v_child, u_child)) break;
                  v_child = v_parent;
                }
              }
            }
          
            // step 3: go through the induced_subhost in postorder(!) and check containment for eligible nodes
            for(Node v: induced_subhost.nodes_postorder()){
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
          } else poss.clear(); // if the induced tree is edgeless but there are at least 2 children of u in guest, then u is not displayed
        } else append(poss, child_poss); // if u has a single child, then u maps where this child maps
      } else poss.clear(); // if no child of u can be mapped, then u cannot be mapped either

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
  template<class MulSubtree, class Guest, bool leaf_labels_only = true>  
  class TreeInComponent
  {
    using SG_Label_Matching = LabelMatchingFromNets<MulSubtree, Guest, std::vector>;

    using TreeChecker = TreeInTreeContainment<MulSubtree, Guest, leaf_labels_only>;
    using NodeInfos = typename TreeChecker::NodeInfos;


    const Guest& guest;
    
    std::shared_ptr<NodeInfos>          node_infos;
    std::shared_ptr<SG_Label_Matching>  SG_label_match;
    
    MulSubtree  subtree;
    TreeChecker subtree_display;


    // unzip the reticulations below u to create a MuL-tree
    //NOTE: this assumes that the cherry rule has been applied exhaustively
    template<class Host, class HG_Label_Matching>
    MulSubtree unzip_retis(const Host& host, const HG_Label_Matching& HG_label_match, const Node u)
    {
      std::cout << "unzipping reticulations under tree component below "<<u<<"...\n";
      std::cout << "with label matching "<<HG_label_match<<"\n";
      EdgeVec edges;
      typename MulSubtree::LabelMap labels;

      // to construct the multi-labeled tree, we use a special edge-traversal of the host without a SeenSet, so reticulations are visited multiple times
      size_t node_count = 1;
      //NOTE: we cannot simply use the node indices of the host since we may see some nodes multiple times in the MulTree...
      //      thus, we'll need a translate map, a label map, and a label matching between host and subtree
      HashMap<Node, Node> host_to_subtree; // track translation
      append(host_to_subtree, u, 0); // translate the root to 0
      node_infos->try_emplace(0, 0, 0); // distance to root and the order number of u are 0

      EdgeTraversal<preorder, Host, void, void> my_dfs(u);
      for(const auto xy: my_dfs){
        const Node x = xy.tail();
        Node y = xy.head();
        if(host.out_degree(x) != 1){
          std::cout << "got edge "<<x<<"->"<<y<<"\n";
          // skip reticulation chains
          while(host.out_degree(y) == 1) y = host.any_child(y);
          // translate u & v
          const Node st_x = host_to_subtree[x];
          const Node st_y = host_to_subtree[y] = node_count++;
          std::cout << "translated to "<<st_x<<"->"<<st_y<<" in the subtree\n";
          // add the edge to the subtree
          append(edges, st_x, st_y);
          // set distance to root and order number of st_y
          node_infos->try_emplace(st_y, node_infos->at(st_x).dist_to_root + 1, st_y); // st_v is its own order number as we're building in preorder
          std::cout << "added node-info for "<<st_y<<": "<<node_infos->at(st_y)<<"\n";
          // register the label if y has one
          const auto& ylabel = host.label(y);
          if((!leaf_labels_only || host.is_leaf(y)) && !ylabel.empty()) {
            std::cout << "mark - label("<<y<<") = "<<ylabel<<"\n";
            std::cout << "matching: "<<HG_label_match.at(ylabel)<<"\n";
            // register label in the label map
            append(labels, st_y, ylabel);
            // register label and st_v in the label matching
            const auto [iter, succ] = SG_label_match->try_emplace(ylabel);
            if(succ) iter->second.second = HG_label_match.at(ylabel).second;
            append(iter->second.first, st_y);
            std::cout << "matched labels: "<<*iter<<"\n";
          }
        }
      }
      std::cout << "got edges: "<<edges<<"\n";
      std::cout << "computed " << node_infos->size() << " node infos: "<< *node_infos<<"\n";
      return {edges, labels, consecutive_tag()};
    }

  public:
    // construct from
    template<class Host, class HG_Label_Matching>
    TreeInComponent(const Host& _host, const Guest& _guest, const HG_Label_Matching& HG_label_match, const Node u):
      guest(_guest),
      node_infos(std::make_shared<NodeInfos>()),
      SG_label_match(std::make_shared<SG_Label_Matching>()),
      subtree(unzip_retis(_host, HG_label_match, u)),
      subtree_display(subtree, _guest, node_infos, SG_label_match, true) // note: the checker may move out of the label matching
    {
      std::cout << "\tconstructed TreeInComponent checker\n subtree is:\n"<<subtree<<"\nguest is at "<<&guest<<":\n"<<guest<<"\n";
    }

    // get the highest ancestor of v in the guest that is still displayed by the tree-component
    Node highest_displayed_ancestor(Node v)
    {
      std::cout << "mark, node: "<<v<<"\n";
      std::cout << "guest:\n"<<guest<<"\n";
      // step 1: unzip the lowest reticulations
      Node pv = guest.parent(v);
      std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
      while(1) {
        const auto& pv_disp = subtree_display.who_displays(pv);
        if(pv_disp.empty()) return v;
        v = pv;
        if(pv == guest.root()) return v;
        if((pv_disp.size() == 1) && (front(pv_disp) == subtree.root())) return v;
        pv = guest.parent(pv);
        std::cout << "testing parent "<<pv<<" of "<<v<<"\n";
      }
    }


  };



  template<class Containment>
  struct ContainmentReduction
  {
    using Host = typename Containment::RWHost;
    using Guest = typename Containment::RWGuest;
    using LabelMatching = typename Containment::LabelMatching;
    using LabelType = typename Host::LabelType;
    
    Containment& contain;

    ContainmentReduction(Containment& c): contain(c) {}

    virtual bool apply() = 0;
    virtual ~ContainmentReduction() = default;
  };

  template<class Containment>
  struct ContainmentReductionWithNodeQueue: public ContainmentReduction<Containment>
  {
    using Parent = ContainmentReduction<Containment>;
    using Parent::Parent;

    HashSet<Node> node_queue;
    void add(const Node x) { append(node_queue, x); std::cout << "adding node "<<x<<" to queue of "<<reduction_name()<<", now: "<<node_queue<<"\n"; }
    
    virtual std::string reduction_name() const { return "reduction with queue"; }
  };

  // a class to merge adjacent reticulations 
  template<class Containment>
  struct ReticulationMerger: public ContainmentReductionWithNodeQueue<Containment>
  {
    using Parent = ContainmentReductionWithNodeQueue<Containment>;
    using typename Parent::Host;
    using Parent::contain;
    using Parent::node_queue;
  
    Host& host;
    ReticulationMerger(Containment& c): Parent(c), host(c.host) {}

    // if the child y of x is a reti, then contract x down to y
    bool contract_reti(const Node x)
    {
      assert(host.out_degree(x) == 1);
      const Node y = host.any_child(x);
      if(host.out_degree(y) == 1) {
        if(host.in_degree(y) > 1) {
          std::cout << "contracting reti "<<x<<" down to reti "<<y<<"\n";
          for(const Node z: host.common_parents(x, y)) contain.suppress.add(z);
          // next, we contract x onto y (we have to contract retis downwards in order to preserve their comp_info)
          if(host[x].comp_root == x) contain.comp_info.comp_DAG.suppress_node(x);
          host.contract_downwards(x, y);
          contain.comp_info.inherit_root(y);
        }
        contain.suppress.clean_up_node(y);
        return true;
      }
      return false;
    }
    // simplify host by contracting adjacent retis
    bool apply()
    {
      bool result = false;
      while(!node_queue.empty()){
        const Node x = value_pop_back(node_queue);
        if(host.has_node(x) && (host.out_degree(x) == 1) && contract_reti(x))
          result = true;
      }
      return result;
    }

    // prepare first round of reticulations to merge
    void init_queue()
    {
      for(const Node x: host.nodes())
        if((host.out_degree(x) == 1) && (host.out_degree(host.any_child(x)) == 1))
          Parent::add(x);
    }

    virtual std::string reduction_name() const { return "reticulation merger"; }
  };

  // a class to apply triangle reduction
  template<class Containment>
  struct TriangleReducer: public ContainmentReduction<Containment>
  {
    using Parent = ContainmentReduction<Containment>;
    using typename Parent::Host;
    using Parent::contain;

    Host& host;

    TriangleReducer(Containment& c): Parent(c), host(c.host) {}

    bool apply() { return triangle_rule(); }

    // triangle rule:
    // if z has 2 parents x and y s.t. xy is an arc and x & y have out-deg 2, then return the Node x in order to remove the arc xz
    Node triangle_rule(const Node z, const Node y)
    {
      assert(host.out_degree(z) == 1);
      if(host.out_degree(y) <= 2){
        for(const Node x: host.parents(y)){
          std::cout << "checking parent "<<x<<" of "<<z<<" for triangle with "<<y<<"\n";
          if((host.out_degree(x) == 2) && test(host.parents(z), x)) {
            host.remove_edge(x, z);
            contain.comp_info.inherit_root(z); // update the component info of y
            contain.suppress.clean_up_node(z, false);
            contain.suppress.clean_up_node(x, false);
            return true;
          }
        }
      } 
      return false;
    }

    bool triangle_rule(const Node z)
    {
      assert(host.has_node(z));
      bool result = false;
      if(host.out_degree(z) == 1){
        bool local_result;
        do {
          local_result = false;
          for(const Node y: host.parents(z))
            if((host.out_degree(y) <= 2) && triangle_rule(z, y)) {
              result = local_result = true;
              break;
            }
        } while(local_result && host.has_node(z));
      } 
      return result;
    }

    bool triangle_rule()
    {
      bool result = false;
      NodeVec retis = host.reticulations().to_container();
      while(!retis.empty()){
        const Node z = value_pop_back(retis);
        if(host.has_node(z) && triangle_rule(z)) result = true;
      }
      return result;
    }

    virtual std::string reduction_name() const { return "triangle rule"; }
  };

  // a class for containment cherry reduction
  template<class Containment>
  struct CherryPicker: public ContainmentReductionWithNodeQueue<Containment>
  {
    using Parent = ContainmentReductionWithNodeQueue<Containment>;
    using typename Parent::Host;
    using typename Parent::Guest;
    using typename Parent::LabelMatching;
    using typename Parent::LabelType;
    using Parent::contain;
    using Parent::node_queue;

    Host& host;
    Guest& guest;

    CherryPicker(Containment& c): Parent(c), host(c.host), guest(c.guest) {}

    bool apply() { return simple_cherry_reduction(); }

    void init_queue()
    {
      for(const auto uv: contain.HG_label_match)
        Parent::add(uv.second.first);
    }

    bool simple_cherry_reduction()
    {
      std::cout << "\tCHERRY: applying reduction...\n";
      bool result = false;
      while(!node_queue.empty()){
        const Node u = value_pop_back(node_queue);
        if(host.has_node(u) && host.has_label(u))
          if(simple_cherry_reduction_from(contain.HG_label_match.find(host.label(u))))
            result = true;
      }
      std::cout << "done with cherry reduction, result is "<<result<<"\n";
      return result;
    }

    bool simple_cherry_reduction_from(const typename LabelMatching::iterator& uv_label_iter)
    {
      std::cout << "\tCHERRY: checking label matching "<<*uv_label_iter<<"\n";
      const Node u = uv_label_iter->second.first;
      if(host.in_degree(u) != 1) return false;
      const Node pu = host.parent(u);
      if(host.in_degree(pu) > 1) return false;
      
      const Node v = uv_label_iter->second.second;
      if(guest.in_degree(v) != 1) return false;
      const Node pv = guest.parent(v);

      // degrees match, so see if the labels of the children match
      HashSet<LabelType> seen;
      for(const Node x: guest.children(pv)) {
        if(guest.out_degree(x) != 0) return false;
        append(seen, guest.label(x));
      }
      //NOTE: detect reticulated cherries by passing through reticulations
      const auto& puC = host.children(pu);
      std::cout << "\tCHERRY: considering children "<<puC<<" of "<<pu<<"\n";
      if(puC.size() < seen.size()){
        // pu cannot display pv, so pu has to display v
        contain.HG_match.match_nodes(pu, v, uv_label_iter);
        return true;
      } else {
        for(auto iter = puC.begin(); iter != puC.end();){
          Node x = *iter; ++iter;
          Node y = x;
          Node z = pu;
          // get the first reti on the pu-->x path into y and it's parent into z
          while((host.out_degree(x) == 1) && (host.in_degree(x) == 1)) y = x = host.any_child(z = x);
          // skip all remaining outdeg-1 nodes
          while(host.out_degree(x) == 1) x = host.any_child(x);
          // we've arrived at a label below pu that is not below pv, so pu-->x will never be in an embedding of guest in host!
          if(host.has_label(y) && !seen.erase(host.label(y))) {
            host.remove_edge(z, y);
            contain.comp_info.inherit_root(y); // update the component info of y
            contain.suppress.add(z);
            contain.suppress.add(y);
          }
        }
      }
      if(!seen.empty()) return false;

      std::cout << "\tCHERRY: found (reticulated) cherry at "<<pu<<" (host) and "<<pv<<" (guest)\n";
      // step 1: fix visibility labeling to u, because the leaf that pu's root is visible from may not survive the cherry reduction (but u will)
      host[host[pu].comp_root].visible_leaf = u;
      // step 2: prune host and guest
      contain.HG_match.match_nodes(pu, pv, uv_label_iter);
      std::cout << "\tCHERRY: after reduction:\nhost:\n"<<host<<"guest:\n"<<guest<<"\n";
      return true;
    }

    bool simple_cherry_reduction_for_label(const LabelType& label)
    {
      return label.empty() ? false : simple_cherry_reduction_from(contain.HG_label_match.find(label));
    }

#warning TODO: apply general cherry reduction before branching: 1. uv is cherry in guest and 2. lca(uv) is unique in host OR, slightly cheaper: 2. exists lowest ancestor x of u & v s.t. x visible on v (tree-path f.ex.) and the x-->u path is unqiue

    virtual std::string reduction_name() const { return "cherry reduction"; }
  };

  // a class to suppress suppressible nodes in host
  template<class Containment, bool leaf_labels_only = true>
  struct VisibleComponentRule: public ContainmentReduction<Containment>
  {
    using Parent = ContainmentReduction<Containment>;
    using typename Parent::Host;
    using typename Parent::Guest;
    using typename Parent::LabelMatching;
    using typename Parent::LabelType;
    using Parent::contain;

    using MulSubtree = ROMulTree<>;

    Host& host;
    Guest& guest;
    LabelMatching& HG_label_match;
    RWNetwork<>& cDAG;

    VisibleComponentRule(Containment& c):
      Parent(c), host(c.host), guest(c.guest), HG_label_match(contain.HG_label_match), cDAG(c.comp_info.comp_DAG) {}

    void treat_comp_root(const Node u, const Node visible_leaf)
    {
      TreeInComponent<MulSubtree, Guest, leaf_labels_only> tree_comp_display(host, guest, HG_label_match, u);
      
      assert(visible_leaf != NoNode);
      const auto& vlabel = host.label(visible_leaf);

      std::cout << "using visible leaf "<<visible_leaf<<" with label "<<vlabel<<"\n";
      std::cout << "label matching: "<<HG_label_match<<"\n";

      const auto uv_label_iter = HG_label_match.find(vlabel);
      assert(uv_label_iter != HG_label_match.end());

      // step 2: get the highest ancestor v of vis_leaf in T s.t. T_v is still displayed by N_u
      const Node visible_leaf_in_guest = uv_label_iter->second.second;
      std::cout << "finding highest node displaying "<<visible_leaf_in_guest<<"\n";
      const Node v = tree_comp_display.highest_displayed_ancestor(visible_leaf_in_guest);
      std::cout << v << " is the highest displayed ancestor (or it's the root) - label: "<<vlabel<<"\n";

      // step 3: replace both N_u and T_v by a leaf with the label of vis_leaf
      contain.HG_match.match_nodes(u, v, uv_label_iter);
    }


#warning TODO: whenever we remove edges from host, update component-DAG!
    // a component root x is called "1/2-eligible" if, for each each child y of x in the component-DAG, the x-y-path is unique in host and y is 1/2-eligible
    // a component root x is called "eligible" if it is 1/2-eligible and it is visible from a leaf

    // a pair (v, 10) in PathProfile[u] means that there are 10 different u-->v paths in host
    using PathProfile = HashMap<Node, HashMap<Node, size_t>>;

    // return whether u is half-eligible and, update u's paths
    //NOTE: the update to num_paths[u] are incorrect if u is not half-elighible!
    bool is_half_eligible(const Node u, PathProfile& num_paths, const HashSet<Node>& half_eligible)
    {
      auto& u_paths = num_paths[u];
      std::cout << "checking node "<<u<<" ("<<host[u]<<") with current paths "<<u_paths<<"\n";
      for(const Node v: cDAG.children(u)) if(test(half_eligible, v)) {
        std::cout << "next child: "<<v<<"\n";
        // to test if the u-v-path in host is unique, we'll look at all reticulations above v in host and check if we see u more than once
        assert(host.in_degree(v) == 1);
        Node r = host.parent(v);
        if(r != u) { // if v is also the child of u in host (!) then the u-v-path is unqiue
          NodeVec retis_above = {r};
          while(!retis_above.empty()){
            r = value_pop_back(retis_above);
            std::cout << "exploring reti "<<r<<" ("<<host[r]<<") above "<<v<<"\n";
            if(host.in_degree(r) <= 1) {
              assert(host[r].comp_root != NoNode);
              num_paths[host[r].comp_root][v]++;
              std::cout << "registered "<<host[r].comp_root<<"--"<<v<<" path. Now, "<<u<<" has "<<num_paths[host[r].comp_root][v]<<" of them\n";
            } else append(retis_above, host.parents(r));
          }
          if(u_paths[v] > 1) return false; // if we ended up counting more than one path, return non-eligibility
        } else u_paths[v] = 1;
        // for each root that v has paths to, u also has paths to via v
        for(const auto& node_and_paths: num_paths[v])
          if((u_paths[node_and_paths.first] += node_and_paths.second) > 1)
            return false;
        std::cout << "added paths to "<<u<<"'s paths, now: "<<u_paths<<"\n";
      } else return false;
      std::cout << u<< " eligible with paths: "<<u_paths<<'\n';
      // if we reached here, we know that u is half-eligible... now, if u is also visible from a leaf, its fully eligible and we'll return it
      return true;
    }

    Node get_eligible_component_root()
    {
      if(!cDAG.edgeless()) {
        // cache how many paths there are between a parent (first parameter) and a child (second parameter)
        PathProfile num_paths;
        HashSet<Node> half_eligible;
        for(const Node u: cDAG.nodes_postorder()) {
          if(is_half_eligible(u, num_paths, half_eligible)){
            if(host[u].visible_leaf != NoNode) return u;
            half_eligible.emplace(u);
          } else std::cout << u<< " not eligible\n";
        }
        // if we never found an eligible node, then return failure
        return NoNode;
      } else return cDAG.root();
    }

    // visible tree-component reduction: find a lowest visible tree component C and reduce it in O(|C|) time; return if network/tree changed
    bool apply()
    {
      const Node rt = get_eligible_component_root();
      if(rt != NoNode){
        std::cout << "tree-component rule with eligible node "<< rt <<" ("<<host[rt]<<") on tree-component DAG:\n"<<cDAG<<"\n";
        const Node vis_leaf = host[rt].visible_leaf;

        assert(cDAG.has_node(rt));
        std::cout << "host:\n"<<host<<"\n";
        std::cout << "guest:\n"<<guest<<"\n";
        std::cout << "visible on leaf "<<vis_leaf<<"\n";
        treat_comp_root(rt, vis_leaf);
        std::cout << "successfully treated comp root "<<rt<<"\n";
        return true;
      } else return false;
    } 

    virtual std::string reduction_name() const { return "visible component rule"; }
  };

  // a class to suppress suppressible nodes in host
  template<class Containment>
  struct NodeSuppresser: public ContainmentReductionWithNodeQueue<Containment>
  {
    using Parent = ContainmentReductionWithNodeQueue<Containment>;
    using ComponentInfos = typename Containment::ComponentInfos;
    using typename Parent::Host;
    using Parent::contain;
    using Parent::node_queue;
    using Parent::add;

    Host& host;
    ComponentInfos& comp_info;

    NodeSuppresser(Containment& c): Parent(c), host(c.host), comp_info(c.comp_info) {}
    

    bool apply() { return process_queue(); }

    bool process_queue(const bool recursive = true, const bool apply_reti_reduction = true)
    {
      if(node_queue.empty()) return false;
      while(!node_queue.empty())
        clean_up_node(value_pop_back(node_queue), recursive, apply_reti_reduction);
      return true;
    }

    // clean up dangling leaves/reticulations and suppressible nodes in the host
    //NOTE: this will also update comp_info and comp_info.comp_DAG
    void clean_up_node(const Node y, const bool recursive = true, const bool apply_reti_reduction = true)
    {
      std::cout << "\tCLEAN: called for "<< y << " (reduction flag: "<<apply_reti_reduction<<") in\n"<<host<<"\n";
      if(host.has_node(y) && !host.has_label(y)){
        switch(host.out_degree(y)){
          case 0: // host.out_degree(y) == 0
            {
              std::cout << "\tCLEAN: "<< y << " is a dangling leaf, so we'll remove it from host\n";
              // remove y and treat y's former parents
              for(const Node z: host.parents(y)) add(z);
              host.remove_node(y);
              if(comp_info.comp_DAG.has_node(y)) comp_info.comp_DAG.suppress_node(y); // y may have been a comp-root
            }
            break;
          case 1: // host.out_degree(y) == 1
            if(host.in_degree(y) < 2){
              const Node x = host.parent_safe(y);
              const Node z = host.any_child(y);
              std::cout << "\tCLEAN: "<< y <<" ("<<host[y]<<") is suppressible with child: "<<z<<" ("<<host[z]<<"))\n";
              // if y is the root of a tree-component, then contract the other outgoing arc yz from y,
              // unless z is a reti, in which case we kill the tree comp
              std::cout << "\tCLEAN: comp-info of "<<y<<": "<<host[y]<<"\n";

              // if there is a leaf below y, then contract that onto y
              if(host.out_degree(z) == 0){
                std::cout << "\tCLEAN: "<< y <<"'s only remaining child "<<z<<" is a leaf\n";
                host.contract_downwards(y, z);
                comp_info.comp_DAG.remove_node(y);
                comp_info.inherit_root(z);
                if(recursive) clean_up_node(z, true, apply_reti_reduction); else add(z);
                if(host.has_node(z)) contain.cherry_rule.add(z);
              } else {
                if(host[z].comp_root != z){ // if z is not a component root, then contract z onto y
                  std::cout << "\tCLEAN: "<< y <<"'s only remaining child "<<z<<" is not a component-root\n";
                  host.contract_upwards(z, y);
                  comp_info.inherit_root(y);
                  if(recursive) clean_up_node(y, true, apply_reti_reduction); else add(z);
                } else { // if z is a component root, then contract y onto z
                  std::cout << "\tCLEAN: "<< y <<"'s only remaining child "<<z<<" is a component-root\n";
                  host.contract_downwards(y, z);
                  comp_info.inherit_root(z);
                  if(comp_info.comp_DAG.has_node(y)) comp_info.comp_DAG.suppress_node(y); // y may have been a comp-root
                  if(recursive) clean_up_node(z, true, apply_reti_reduction); else add(z);
                }
                // if the parent of y still exists in host, then also recurse for him
                if(recursive) clean_up_node(x, true, apply_reti_reduction); else add(x);
              }
            } else {
              std::cout << "\tCLEAN: "<< y <<" is a reticulation\n";
              if(!apply_reti_reduction || !contain.triangle_rule.triangle_rule(y)){
                const Node z = host.any_child(y);
                switch(host.out_degree(z)){
                  case 0:
                    comp_info.inherit_root(z);
                    break;
                  case 1:
                    if(apply_reti_reduction) contain.reti_merge.contract_reti(y);
                    break;
                  default: break;
                } // end switch out-degree(z)
              } else if(recursive) clean_up_node(y, true, apply_reti_reduction);
            }// end if(in-degree(y)...)
            break;
          default: // y is a non-leaf tree node
            break;
        }
      }
      std::cout << "\tCLEAN: done.\n";
    }

    virtual std::string reduction_name() const { return "node suppression"; }
  };



  // whenever a node in the host is identified that optimally displays a node in the guest, this class updates host and guest accordingly
  template<class Containment>
  struct HostGuestMatch
  {
    using Host = typename Containment::RWHost;
    using Guest = typename Containment::RWGuest;
    using LabelMatching = typename Containment::LabelMatching;

    Containment& contain;
    Host& host;
    Guest& guest;
    LabelMatching& HG_label_match;
   
    HostGuestMatch(Containment& c): contain(c), host(c.host), guest(c.guest), HG_label_match(c.HG_label_match) {}

    // prune the guest below Node x, 
    // ATTENTION: also remove leaves of the host that have a label below x in the guest!!!
    // ATTENTION: also remove the corresponding entries in HG_label_match
    void prune_guest_except(const Node x, const Node except) 
    {
      std::cout << "pruning "<<x<<" in guest\n";
      const auto& gC = guest.children(x);
      if(!gC.empty()){
        for(auto child_iter = gC.begin(); child_iter != gC.end();) {
          const auto next_iter = std::next(child_iter);
          prune_guest_except(*child_iter, except);
          child_iter = next_iter;
        }
        if(x != except) {
          std::cout << "SUPPRESSING "<<x<<" in guest...\n";
          guest.suppress_node(x);
        }
      } else if(x != except) {
        // if x is a leaf, then remove x from guest, remove the corresponding leaf from host, and remove the label-matching entry
        const auto HG_match_iter = HG_label_match.find(guest.label(x));
        assert((HG_match_iter != HG_label_match.end()) && (HG_match_iter->second.second == x));
        const Node host_x = HG_match_iter->second.first;

        std::cout << "removing "<<x<<" (guest) and its correspondant "<<host_x<<" (host)...\n";
        for(const Node w: host.parents(host_x)) contain.suppress.add(w);
        host.suppress_node(host_x);
        guest.suppress_node(x);
        HG_label_match.erase(HG_match_iter);
      }
    }

    // this prunes the tree-remains left after recursively removing dangling leaves after pruning guest
    void prune_host_except(const Node rt, const Node except)
    {
      const auto& hC = host.children(rt);
      std::cout << "pruning "<<rt<<" with children "<<hC<<" (except "<<except<<")\n";
      if(!hC.empty()){
        auto child_iter = hC.begin();
        while(child_iter != hC.end()){
          std::cout << "looking at child "<<*child_iter<<" of "<<rt<<" in "<<hC<<"\n";
          const Node y = *child_iter;
          if(y != except) {
            if(host.in_degree(y) > 1) {
              host.remove_edge(rt, y);
              contain.comp_info.inherit_root(y); // update the component info of y
              contain.suppress.add(y);
            } else prune_host_except(y, except);
            child_iter = hC.begin();
          } else ++child_iter;
        }
        assert(host.out_degree(rt) <= 1);
        contain.suppress.clean_up_node(rt, false, false);
      } else if(rt != except) {
        if(host.has_label(rt)) {
          // if the node v in guest with the same label as x has not been removed before, it means that v can never be displayed!
          const auto HG_iter = HG_label_match.find(host.label(rt));
          if(guest.has_node(HG_iter->second.second)) contain.failed = true;
          HG_label_match.erase(HG_iter);
        }
        if(host[rt].comp_root == rt) {
          assert(contain.comp_info.comp_DAG.has_node(rt));
          contain.comp_info.comp_DAG.suppress_node(rt);
        }
        host.remove_node(rt);
      }
    }

    // after having identified a node host_u in the host that will be used to display the subtree below guest_v, remove the subtrees below them
    //NOTE: xy_label_iter points to the label-pair that will be spared (u and v will end up receiving this label)
    void match_nodes(const Node host_u, const Node guest_v, const typename LabelMatching::iterator& xy_label_iter)
    {
      // step 3a: prune guest and remove nodes with label below v (in guest) from both host and guest - also remove their label entries in HG_label_match
      const auto& vlabel = xy_label_iter->first;
      const Node host_x = xy_label_iter->second.first;
      const Node guest_y = xy_label_iter->second.second;

      std::cout << "\tMATCH: marking "<<guest_v<<" (guest) & "<<host_u<<" (host) with label "<<vlabel<<"\n";
      
      std::cout << "\tMATCH: pruning guest at "<<guest_v<<" (except "<<guest_y<<"):\n"<<guest<<"\n";
      prune_guest_except(guest_v, guest_y); // note: prune_guest automatically removes everything it encounters from HG_label_match
      std::cout << "\tMATCH: pruned guest:\n"<<guest<<"\n";

      //NOTE: prune_guest may have already suppressed host_u, so check if it's still there...
      assert(host.has_node(host_u));
      //NOTE: keep track of nodes in the host who have one of their incoming edges removed as component roots may now see them
      std::cout << "\tMATCH: pruning host at "<<host_u<<" (except "<<host_x<<"):\n"<<host<<" with comp-DAG\n"<<contain.comp_info.comp_DAG<<"\n";
      //prune_host_except(host_u, host_x);
      contain.suppress.process_queue(true, false);
      if(host.has_node(host_u)){
        prune_host_except(host_u, host_x);
        std::cout << "\tMATCH: done pruning host; now processing suppress-queue "<<contain.suppress.node_queue<<"\n";
        assert(host.in_degree(host_x) <= 1);
        contain.comp_info.inherit_root(host_x);
        contain.suppress.apply();
      }
      std::cout << "\tMATCH: pruned host:\n"<<host<<"\n";

      std::cout << "\tMATCH: node-info:\n";
      for(const auto x: host.get_node_data()) std::cout << x<<"\n";

      if(contain.comp_info.comp_DAG.has_node(host_u)){
        std::cout << "\tMATCH: removing "<<host_u<<" from cDAG\n";
        contain.comp_info.comp_DAG.remove_node(host_u);
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

    friend class ContainmentReduction<TreeInNetContainment>;
    friend class ContainmentReductionWithNodeQueue<TreeInNetContainment>;
    friend class ReticulationMerger<TreeInNetContainment>;
    friend class TriangleReducer<TreeInNetContainment>;
    friend class CherryPicker<TreeInNetContainment>;
    friend class VisibleComponentRule<TreeInNetContainment>;
    friend class NodeSuppresser<TreeInNetContainment>;
    friend class HostGuestMatch<TreeInNetContainment>;

    // we'll work with mutable copies of the network & tree, which can be given by move
    using RWHost = CompatibleRWNetwork<Host, ComponentData>;
    using RWGuest = CompatibleRWTree<Guest>;

    using ComponentInfos = TreeComponentInfos<RWHost>;

    using HostEdge = typename RWHost::Edge;
    using NodeList = _NodeList<RWHost>;
    using LabelMatching = LabelMatchingFromNets<RWHost, RWGuest, std::vector>;
    using LM_Iter = typename LabelMatching::iterator;

    RWHost host;
    RWGuest guest;
    LabelMatching HG_label_match;
    ComponentInfos comp_info;

    bool failed;

    // reduction rules
    ReticulationMerger<TreeInNetContainment> reti_merge;
    TriangleReducer<TreeInNetContainment> triangle_rule;
    CherryPicker<TreeInNetContainment> cherry_rule;
    VisibleComponentRule<TreeInNetContainment> visible_comp;
    NodeSuppresser<TreeInNetContainment> suppress;
    HostGuestMatch<TreeInNetContainment> HG_match;
  

    // ***************************************
    //         private Construction
    // ***************************************

    // general constructor with universal references and a bool sentinel
    //NOTE: the public constructors below just call this one, but they are useful for the compiler to deduce the template arguments of the class
    template<class _Host, class _Guest>
    TreeInNetContainment(_Host&& _host, _Guest&& _guest, const bool _failed):
      host(std::forward<_Host>(_host)),
      guest(std::forward<_Guest>(_guest)),
      HG_label_match(host, guest),
      comp_info(host),
      failed(_failed),
      reti_merge(*this),
      triangle_rule(*this),
      cherry_rule(*this),
      visible_comp(*this),
      suppress(*this),
      HG_match(*this)
    { init(); }

    TreeInNetContainment(const TreeInNetContainment& tc):
      host(tc.host),
      guest(tc.guest),
      HG_label_match(tc.HG_label_match),
      comp_info(tc.comp_info, host),
      failed(tc.failed),
      reti_merge(*this),
      triangle_rule(*this),
      cherry_rule(*this),
      visible_comp(*this),
      suppress(*this),
      HG_match(*this)
    {}

    TreeInNetContainment(TreeInNetContainment&& tc):
      host(std::move(tc.host)),
      guest(std::move(tc.guest)),
      HG_label_match(std::move(tc.HG_label_match)),
      comp_info(std::move(tc.comp_info), host),
      failed(std::move(tc.failed)),
      reti_merge(*this),
      triangle_rule(*this),
      cherry_rule(*this),
      visible_comp(*this),
      suppress(*this),
      HG_match(*this)
    {}

    // ***************************************
    //              Maintenance
    // ***************************************

    void apply_rules()
    {
      std::cout << "\n ===== REDUCTION RULES ======\n\n";
      reti_merge.init_queue();
      reti_merge.apply();

      // if, at some point, there are only 2 leaves left, then simply say 'yes'
      while(!failed && (HG_label_match.size() > 2)) {
        std::cout << "\nrestart rule-application...\n";
        std::cout << "host:\n" << host << "guest:\n" << guest << "comp-DAG:\n"<<comp_info.comp_DAG<<"\n";
        std::cout << "label matching: "<<HG_label_match<<"\n";
        std::cout << "comp-info: "; for(const auto& x: host.get_node_data()) std::cout << x <<"\n";

        if(suppress.apply()) continue;
        // not necessary to apply each time, clean_up_node will call it when appropriate
        //if(triangle_rule.apply()) continue;
      
        cherry_rule.init_queue();
        if(cherry_rule.apply()) continue;
        
        if(visible_comp.apply()) continue;

        reti_merge.init_queue();
        if(reti_merge.apply()) continue;
        
        break;
      }
      std::cout << "\n ===== REDUCTION done ======\n\n";
      std::cout << "host:\n" << host << "guest:\n" << guest << "comp-DAG:\n"<<comp_info.comp_DAG<<"\n";
    }

    // initialization and early reductions
    void init()
    {
      if(!clean_up_labels()){
        std::cout << "initial comp-root DAG:\n"<<comp_info.comp_DAG<<"\n";
        std::cout << "initial comp-info: "; for(const auto& x: host.get_node_data()) std::cout << x <<"\n";
        // if host has only 1 tree component, then its root is visible by all leaves, so visible-component rule solves the instance
        //NOTE: in this case, make sure the root of the component DAG has the same index as the host
        if(comp_info.comp_DAG.edgeless() && !host.edgeless()) {
          // NOTE: if comp_info.comp_DAG is edgeless, then its root may not correspond to the root of host
          const Node tc_root = comp_info.comp_DAG.root();
          comp_info.comp_DAG.add_child(tc_root, host.root());
          comp_info.comp_DAG.remove_node(tc_root);
        }

        // apply all reduction rules
        apply_rules();

        // if the comp_DAG is edgeless, then visible-component reduction must have applied (unless the loop broke for the 2-label-case)
        assert(!comp_info.comp_DAG.edgeless() || host.edgeless() || (HG_label_match.size() <= 2));
      }
      std::cout << "done initializing Tree-in-Net containment checker; failed? "<<failed<<"\n";
    }

    // remove labels present in only one of N and T, remove whether we already failed
    bool clean_up_labels()
    {
      std::cout << "cleaning-up labels...\n";
      for(auto label_iter = HG_label_match.begin(); label_iter != HG_label_match.end();){
        auto& uv = label_iter->second;
        // if the label only exists in the guest, but not in the host, then the host can never display the guest!
        if(uv.first.empty()) return failed = true; // this label occurs only in the guest, so the host can never display it
        if(uv.second.empty()) {
          // this label occurs only in the host, but not in the guest, so we can simply remove it, along with the entry in the label matching
          std::cout << "removing label "<<label_iter->first<<" from the host since it's not in the guest\n";
          host.remove_upwards(uv.first);
          label_iter = HG_label_match.erase(label_iter);
        } else ++label_iter;
      }
      return false;
    }



    // ***************************************
    //               branching
    // ***************************************

    // in our data structure, we map each target reticulation to 3 numbers: (1) #parents not seeing a comp root,
    // (2) #parents seeing a non-leaf comp-root, (3) #parents seeing a leaf-comp-root
    struct BranchInfo
    {
      Node node;
      size_t parents_seeing_noone = 0;
      size_t parents_seeing_non_leaf_comp = 0;
      size_t parents_seeing_leaf_comp = 0;

      BranchInfo(const Node& u): node(u) {}
      // a branchinfo is "better" than another if it has fewer parents seeing noone or,
      // when tied, if it has fewer parents not seeing non-leaf-components or,
      // when still tied, if it has fewer parents seeing leaf-components
      int cmp(const BranchInfo& other) const
      {
        if(parents_seeing_noone < other.parents_seeing_noone) return -1;
        if(parents_seeing_noone > other.parents_seeing_noone) return 1;

        if(parents_seeing_non_leaf_comp < other.parents_seeing_non_leaf_comp) return -1;
        if(parents_seeing_non_leaf_comp > other.parents_seeing_non_leaf_comp) return 1;

        if(parents_seeing_leaf_comp < other.parents_seeing_leaf_comp) return -1;
        if(parents_seeing_leaf_comp > other.parents_seeing_leaf_comp) return 1;
        return 0;
      }
      bool operator<(const BranchInfo& other) const { return cmp(other) < 0; }
      bool operator>(const BranchInfo& other) const { return cmp(other) > 0; }
      bool operator<=(const BranchInfo& other) const { return cmp(other) <= 0; }
      bool operator>=(const BranchInfo& other) const { return cmp(other) >= 0; }
      bool operator==(const BranchInfo& other) const { return cmp(other) == 0; }

      friend std::ostream& operator<<(std::ostream& os, const BranchInfo& bi)
      {
        return os << "["<<bi.node<<": "<<bi.parents_seeing_noone<<" see noone, "
                                       <<bi.parents_seeing_non_leaf_comp<<" see non-leaf-comp, "
                                       <<bi.parents_seeing_leaf_comp<<" see leaf-comp]";
      }
    };




  public:

    // initialization allows moving Host and Guest
    //NOTE: to allow deriving Host and Guest template parameters, we're not using forwarding references here, but 4 different constructors
    TreeInNetContainment(const Host& _host, const Guest& _guest): TreeInNetContainment(_host, _guest, false) {}
    TreeInNetContainment(const Host& _host, Guest&& _guest):      TreeInNetContainment(_host, std::move(_guest), false) {}
    TreeInNetContainment(Host&& _host, const Guest& _guest):      TreeInNetContainment(std::move(_host), _guest, false) {}
    TreeInNetContainment(Host&& _host, Guest&& _guest):           TreeInNetContainment(std::move(_host), std::move(_guest), false) {}

    // force a node u to have a node v as its parent in the embedding of the tree (as part of a branching)
    //NOTE: no check is made whether v really is a parent of u
    void force_parent(const Node u, const Node v)
    {
      // step 1: remove the "false" parents and clean-up retis and suppressible nodes
      auto& u_parents = host.parents(u);
      while(u_parents.size() > 1) {
        Node pu = front(u_parents);
        if(pu == v) pu = *(std::next(u_parents.begin()));
        host.remove_edge(pu, u);
        comp_info.inherit_root(u); // update the component info of y
        suppress.add(pu);
      }
      // step 2: copy visibility and suppress u
      suppress.add(u);
    }


    bool displayed()
    {
      if(failed) return false;
      apply_rules();
      if(failed) return false;
      std::cout << "number of edges: "<<host.num_edges() << " (host) "<<guest.num_edges()<<" (guest)\n";
      if(HG_label_match.size() <= 2) return true;
      if(host.edgeless()){
        if(guest.edgeless())
          return host.label(host.root()) == guest.label(guest.root());
        else
          return false;
      } else {
        // We'll have to branch at this point. We would like to make visible as many leaf-components as possible on each branch.
        // Branching will be done on invisible reticulations r above leaves s.t. r has few parents that see no component root and,
        // among them, those that see few non-leaf-component roots.

        // step 1: for each leaf z that does not see a non-leaf-component root, get the reticulation above z (if z does not have
        // a reticulation parent, then cherry reduction must be applicable to it)
        std::cout << "visibility: ";
        for(const auto& x: host.get_node_data()) std::cout << x <<"\n";
        std::cout << "label-matching: "<<HG_label_match<<"\n";

        std::priority_queue<BranchInfo, std::vector<BranchInfo>, std::greater<BranchInfo>> branching_candidates;
        for(const auto& HG_leaf_pair: seconds(HG_label_match)){
          const Node u = HG_leaf_pair.first;
          const Node rt_u = host[u].comp_root;
          if(rt_u == NoNode) {
            std::cout << u << " sees noone\n" << u <<"'s parents are "<<host.parents(u)<<"\n";
            // if u does not see any component-root, then its parent is necessarily a reticulation (since cherry-reduction is applied continuously)
            assert(host.in_degree(u) == 1);
            const Node pu = host.parent(u);
            assert(host.out_degree(pu) == 1);
            assert(host.in_degree(pu) > 1);
            // the parent reticulation cannot see a tree-component since u would see it too
            assert(host[pu].comp_root == NoNode);
            
            BranchInfo pu_info(pu);
            for(const Node x: host.parents(pu)) {
              const Node rt_x = host[x].comp_root;
              std::cout << "considering comp-root "<<rt_x <<" ("<<host[x]<<") of "<<pu<<"\n";
              if(rt_x != NoNode){
                if(comp_info.comp_DAG.is_leaf(rt_x))
                  pu_info.parents_seeing_leaf_comp++;
                else
                  pu_info.parents_seeing_non_leaf_comp++;
              } else pu_info.parents_seeing_noone++;
            }
            std::cout << "branch-info for "<<pu<<": "<<pu_info<<"\n";
            branching_candidates.emplace(std::move(pu_info));
          } else std::cout << rt_u << " is visible from "<<u<<" so one of the implied branches does not make progress. Ignoring "<<u<<"...\n";
        }
        std::cout << "found "<<branching_candidates.size() <<" branching opportunities\n";
        assert(!branching_candidates.empty());
        std::cout << "best branching: "<< branching_candidates.top() << "\n";

        // for each parent u of the best-branching node v, make a copy of *this and make u the only parent of v, then run the machine again
        const Node u = branching_candidates.top().node;
        for(const Node v: host.parents(u)){
          std::cout << "\n================ new branch: keep "<<v<<" --> "<<u<<" ====================\n\n";
          TreeInNetContainment sub_checker(std::as_const(*this));
          sub_checker.force_parent(u, v);
          if(sub_checker.displayed()) return true;
          std::cout << "unsuccessful branch :/\n";
        }
        // if no branch returned true, then the tree is not displayed
        return false;
      }
    }

  };


}
