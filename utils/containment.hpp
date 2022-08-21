
#pragma once

#include "cuts.hpp"
#include "tree_components.hpp"
#include "tree_tree_containment.hpp"
#include "tree_comp_containment.hpp"
#include "containment_reductions.hpp"

namespace PT {

  // a containment checker, testing if a single-labelled host-network contains a single-labeled guest tree
  //NOTE: if an invisible tree component is encountered, we will branch on which subtree to display in the component
  //NOTE: this can be used to solve multi-labeled host networks: just add a reticulation for each multiply occuring label
  template<StrictPhylogenyType _Host,
           StrictPhylogenyType _Guest,
           StorageEnum HostLabelStorage = singleS,
           bool leaf_labels_only = true>
  struct TreeInNetContainment {
    using Host = _Host;
    using Guest = _Guest;

    using LabelMatching = PT::LabelMatching<Host, Guest, HostLabelStorage, singleS>;
    using LabelMatchingPair = typename LabelMatching::StoragePair;
    using ComponentInfos = TreeComponentInfos<Host, 2>;
    static constexpr bool host_is_tree = Host::is_declared_tree;
    static constexpr bool host_is_multi_labeled = (HostLabelStorage != singleS);

    using HostEdge = typename Host::Edge;
    using NodeList = _NodeList<host_is_multi_labeled>;
    using LM_Iter = mstd::iterator_of_t<LabelMatching>;

    Host host;
    Guest guest;
    LabelMatching HG_label_match;
    ComponentInfos comp_info;
    ReductionManager<TreeInNetContainment> reduction_man;

    bool failed = false;

    // ***************************************
    //         private Construction
    // ***************************************
  protected:


    // this constructor is for constructing sub-containments when splitting at bridges
    template<PhylogenyType OtherHost, PhylogenyType OtherGuest, class LMatch, class CInfo>
    TreeInNetContainment(OtherHost&& _host, OtherGuest&& _guest, const bool _failed, LMatch&& lmatch, CInfo&& cinfo):
      host{std::forward<OtherHost>(_host)},
      guest{std::forward<OtherGuest>(_guest)},
      HG_label_match{std::forward<LMatch>(lmatch)},
      comp_info{std::forward<CInfo>(cinfo), host},
      reduction_man{*this},
      failed{_failed}
    { init(); }

    // this is used for branching - it forces u as parent of v
    template<class Translation = NodeTranslation>
    TreeInNetContainment(const TreeInNetContainment& tc, const NodeDesc u, const NodeDesc v, Translation&& trans = Translation()):
      host{tc.host, trans},
      guest{tc.guest, trans},
      HG_label_match{tc.HG_label_match, [&](const auto& other_pair){
        LabelMatchingPair result;
        for(const NodeDesc& x: other_pair.first) mstd::append(result.first, trans.at(x));
        for(const NodeDesc& y: other_pair.second) mstd::append(result.second, trans.at(y));
        return result;
      }},
      comp_info{host}, // rebuilding the component info is not much more expensive than translating the existing one
      reduction_man{*this},
      failed{tc.failed}
    {
      assert(mstd::test(trans, u));
      assert(mstd::test(trans, v));
     
      std::cout << "host:\n" << host << "\nguest:\n" << guest << "\ncomp-DAG:\n";
      comp_info.comp_DAG.print_subtree(std::cout, [](const NodeDesc x){ return std::to_string(x); });
      std::cout << "comp-roots:\n"; for(const NodeDesc x: host.nodes()) std::cout << x <<": " << comp_info.comp_root_of(x) <<'\n';
      
      force_parent(trans.at(u), trans.at(v));
      reduction_man.apply();
    }


    // general constructor with universal references and a bool sentinel
    template<PhylogenyType OtherHost, PhylogenyType OtherGuest>
    TreeInNetContainment(OtherHost&& _host, OtherGuest&& _guest, const bool _failed):
      host{std::forward<OtherHost>(_host)},
      guest{std::forward<OtherGuest>(_guest)},
      HG_label_match{host, guest},
      comp_info{host},
      reduction_man{*this},
      failed{_failed}
    { init(); }


    TreeInNetContainment(const TreeInNetContainment& tc):
      host{tc.host},
      guest{tc.guest},
      HG_label_match{tc.HG_label_match},
      comp_info{tc.comp_info, host},
      reduction_man{*this},
      failed{tc.failed}
    {}


    TreeInNetContainment(TreeInNetContainment&& tc):
      host{std::move(tc.host)},
      guest{std::move(tc.guest)},
      HG_label_match{std::move(tc.HG_label_match)},
      comp_info{std::move(tc.comp_info), host},
      reduction_man{*this},
      failed{tc.failed}
    {}

    // ***************************************
    //              Maintenance
    // ***************************************

    // initialization and early reductions
    void init() {
      if(!clean_up_labels()){
        const auto& cDAG = comp_info.comp_DAG;
        std::cout << "initial comp-root DAG ("<<cDAG.num_nodes()<<" nodes, "<<cDAG.roots().size()<<" roots):\n";
        cDAG.print_subtree_with_data(std::cout);
        // apply all reduction rules
        reduction_man.apply();

        // if the comp_DAG is edgeless, then visible-component reduction must have applied (unless the loop broke for the 2-label-case)
        assert(failed || !comp_info.comp_DAG.edgeless() || host.edgeless() || (HG_label_match.size() <= 2));
        assert(!failed && "remove me after testing");
      }
      std::cout << "done initializing Tree-in-Net containment checker; failed? "<<failed<<"\n";
    }

    // remove labels present in only one of N and T, return whether we already failed
    bool clean_up_labels() {
      std::cout << "cleaning-up labels...\n";
      for(auto label_iter = HG_label_match.begin(); label_iter != HG_label_match.end();){
        auto& uv = label_iter->second;
        // if the label only exists in the guest, but not in the host, then the host can never display the guest!
        if(uv.first.empty()) return failed = true; // this label occurs only in the guest, so the host can never display it
        const NodeDesc u = uv.first.front();
        if(uv.second.empty()) {
          // this label occurs only in the host, but not in the guest, so we can simply remove it, along with the entry in the label-matching
          std::cout << "removing label "<<label_iter->first<<" from the host since it's not in the guest\n";
          host.remove_upwards(u);
          label_iter = HG_label_match.erase(label_iter);
        } else ++label_iter;
      }
      return false;
    }



    // ***************************************
    //               branching
    // ***************************************

    // in our data structure, we map each target reticulation to 3 numbers:
    // (1) #parents not seeing a comp root,
    // (2) #parents seeing a non-leaf comp-root,
    // (3) #parents seeing a leaf-comp-root
    struct BranchInfo {
      NodeDesc node;
      size_t parents_seeing_noone = 0;
      size_t parents_seeing_non_leaf_comp = 0;
      size_t parents_seeing_leaf_comp = 0;

      BranchInfo(const NodeDesc& u): node(u) {}
      // a branchinfo is "better" than another if it has fewer parents seeing noone or,
      // when tied, if it has fewer parents not seeing non-leaf-components or,
      // when still tied, if it has fewer parents seeing leaf-components
      int cmp(const BranchInfo& other) const {
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

      friend std::ostream& operator<<(std::ostream& os, const BranchInfo& bi) {
        return os << "["<<bi.node<<": "<<bi.parents_seeing_noone<<" see noone, "
                                       <<bi.parents_seeing_non_leaf_comp<<" see non-leaf-comp, "
                                       <<bi.parents_seeing_leaf_comp<<" see leaf-comp]";
      }
    };


    // ***************************************
    //            bridge splitting
    // ***************************************
    
    // split the network at a bridge and call the containment for both components; return whether we managed to find a suitable bridge
    // set 'failed' according to the results of the 2 recursive calls
#warning "TODO: actually, this has to be done with cut-nodes instead of bridges! A cut node c may have children x,y,z such that cx & cy are in the same biconnected component distinct from that of cz. This situation should be reduced but there is no bridge"
    bool split_by_bridge() {
      for(const auto bridge: get_bridges(host)) {
        const NodeDesc sub_root = bridge.head();
        if(!Host::is_leaf(sub_root)) {
          std::cout << "\n\n-------------splitting at " << bridge << " --------------\n";
          // step 1: split off the host part below 'bridge'
          assert(Host::out_degree(bridge.tail()) > 1);
          // NOTE: keep sub_root in the host (the root of subhost will be a copy of sub_root)
          std::cout << "creating host network\n";
          Host subhost(policy_move_children_tag{}, std::move(host), sub_root);
          std::cout << "subhost is:\n" << subhost<<"\n";

          // step 1.5: move all label-matches of the subhost into sub_match
          std::cout << "splitting off label-sub-matching\n";
          LabelMatching sub_match;
          NodeDesc guestLCA = NoNode;
          auto LCA = guest.LCA();
          for(const NodeDesc x: subhost.leaves()) {
            auto node_handle = HG_label_match.extract(Host::label(x));
            assert(sub_match.get_allocator() == node_handle.get_allocator());
            auto [iter, success, new_node_handle] = sub_match.insert(std::move(node_handle));
            std::cout << "new sub-match: "<<sub_match<<"\n";
            assert(success);
            const NodeDesc iter_node = static_cast<const NodeDesc&>(iter->second.second);
            guestLCA = (guestLCA == NoNode) ? iter_node : LCA(guestLCA, iter_node);
          }
          assert(guestLCA != guest.root());
          assert(guestLCA != NoNode);

          // step 1.75: compute new ComponentInfos
          std::cout << "computing new component infos\n";
          ComponentInfos sub_cinfo{subhost};

          // step 2: split off the guest part induced by the labels in the subhost
          // NOTE: keep guestLCA in the host (the root of subguest will be a copy of guestLCA)
          std::cout << "creating guest tree by moving everything below "<<guestLCA<<"\n";
          Guest subguest(policy_move_children_tag{}, std::move(guest), guestLCA);
          std::cout << "subguest is:\n" << subguest<<"\n";

          // step 3: check if subhost contains subguest
          TreeInNetContainment sub_containment(std::move(subhost), std::move(subguest), false, std::move(sub_match), std::move(sub_cinfo));
          if(sub_containment.displayed()) {
            assert(!sub_containment.HG_label_match.empty());
            // step 4: replace the subhost and subguest by a new leaf in HG_label_match and continue checking containment
            // step 4a: find a new label
            const auto& new_label = mstd::front(sub_containment.HG_label_match).first;
            assert(!HG_label_match.contains(new_label));
            std::cout << sub_root << " gets the label "<<new_label<<"\n";
            // step 4b: give the label to both guestLCA and sub_host 
            Host::label(sub_root) = new_label;
            Guest::label(guestLCA) = new_label;
            // step 4c: register the triple in HG_label_match
            mstd::append(HG_label_match, new_label, sub_root, guestLCA);
          } else failed = true;
          return true;
        }
      }
      return false;
    }


    // find a good branchin opportunity and solve
    bool do_branch() {
      // We'll have to branch at this point. We would like to make visible as many leaf-components as possible on each branch.
      // Branching will be done on invisible reticulations r above leaves s.t. r has few parents that see no component root and,
      // among them, those that see few non-leaf-component roots.

      // step 1: for each leaf z that does not see a non-leaf-component root, get the reticulation above z (if z does not have
      // a reticulation parent, then cherry reduction must be applicable to it)
      std::priority_queue<BranchInfo, std::vector<BranchInfo>, std::greater<BranchInfo>> branching_candidates;
      for(const auto& HG_leaf_pair: mstd::seconds(HG_label_match)){
        assert(HG_leaf_pair.first.size() == 1);
        const NodeDesc u = mstd::front(HG_leaf_pair.first);
        const NodeDesc rt_u = comp_info.comp_root_of(u);
        if((rt_u == NoNode) || (rt_u == u)) {
          std::cout << u << " sees noone\n" << u <<"'s parents are "<<parents_of<Host>(u)<<"\n";
          // if u does not see any component-root, then its parent is necessarily a reticulation (since cherry-reduction is applied continuously)
          assert(host.in_degree(u) == 1);
          const NodeDesc pu = host.parent(u);
          assert(host.out_degree(pu) == 1);
          assert(host.in_degree(pu) > 1);
          // the parent reticulation should not see a tree-component since u would see it too
          const NodeDesc rt_pu = comp_info.comp_root_of(pu);
          if(rt_pu == NoNode) {
            BranchInfo pu_info(pu);
            for(const NodeDesc x: host.parents(pu)) {
              const NodeDesc rt_x = comp_info.comp_root_of(x);
              std::cout << "considering comp-root "<<rt_x <<" ("<< comp_info.comp_root_of(x) <<") of "<<pu<<"\n";
              if(rt_x != NoNode){
                if(comp_info.comp_DAG.is_leaf(rt_x))
                  pu_info.parents_seeing_leaf_comp++;
                else
                  pu_info.parents_seeing_non_leaf_comp++;
              } else pu_info.parents_seeing_noone++;
            }
            std::cout << "branch-info for "<<pu<<": "<<pu_info<<"\n";
            branching_candidates.emplace(std::move(pu_info));
          } else std::cout << u << "'s parent "<<pu<<" sees "<< rt_pu << " so one of the implied branches does not make progress. Ignoring "<<u<<"...\n"; 
        } else std::cout << rt_u << " is visible from "<<u<<" so one of the implied branches does not make progress. Ignoring "<<u<<"...\n";
      }
      std::cout << "found "<<branching_candidates.size() <<" branching opportunities\n";
      assert(!branching_candidates.empty());
      std::cout << "best branching: "<< branching_candidates.top() << "\n";

      // for each parent u of the best-branching node v, make a copy of *this and make u the only parent of v, then run the machine again
      const NodeDesc u = branching_candidates.top().node;
      for(const NodeDesc v: host.parents(u)){
        std::cout << "\n================ new branch: keep "<<v<<" --> "<<u<<" ====================\n";
        TreeInNetContainment sub_checker(std::as_const(*this), u, v);
        if(sub_checker.displayed()) return true;
        std::cout << "unsuccessful branch\n";
      }
      // if no branch returned true, then the tree is not displayed
      return false;
    }



  public:
    
    // initialization allows moving Host and Guest
    //NOTE: to allow deriving Host and Guest template parameters, we're not using forwarding references here, but 4 different constructors
    TreeInNetContainment(const Host& _host, const Guest& _guest): TreeInNetContainment(_host, _guest, false) {}
    TreeInNetContainment(const Host& _host, Guest&& _guest):      TreeInNetContainment(_host, std::move(_guest), false) {}
    TreeInNetContainment(Host&& _host, const Guest& _guest):      TreeInNetContainment(std::move(_host), _guest, false) {}
    TreeInNetContainment(Host&& _host, Guest&& _guest):           TreeInNetContainment(std::move(_host), std::move(_guest), false) {}

    // force a node u to have a node v as its parent in the embedding of the tree (as part of a branching)
    //NOTE: no check is made whether v really is a parent of u
    void force_parent(const NodeDesc u, const NodeDesc v) {
      // step 1: remove the "false" parents and clean-up retis and suppressible nodes
      auto& u_parents = host.parents(u);
      while(u_parents.size() > 1) {
        NodeDesc pu = mstd::front(u_parents);
        if(pu == v) pu = *(std::next(u_parents.begin()));
        reduction_man.remove_edge_in_host(pu, u);
      }
    }

    bool displayed() {
      if(failed) return false;
      std::cout << "display-test for host ("<<host.num_edges()<<" edges)\n"<<host<<"\n guest ("<<guest.num_edges()<<" edges)\n"<<guest<<"\n";
      if(HG_label_match.size() <= 2) return true;
      if(host.edgeless()){
        if(guest.edgeless())
          return host.label(host.root()) == guest.label(guest.root());
        else
          return false;
      } else {
        std::cout << "visibility: ";
        for(const NodeDesc x: host.nodes()) std::cout << x << ": " << comp_info.visible_leaf_of(x) <<"\n";
        std::cout << "label-matching: "<<HG_label_match<<"\n";

        return (split_by_bridge()) ? displayed() : do_branch();
      }
    }
    

    // translate a node to its corresponding node in the compoent DAG
    NodeDesc host_to_compDAG(const NodeDesc u) const {
      return_map_lookup(comp_info.N_to_compDAG, u, NoNode);
    }
    
  };


}
