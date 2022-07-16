
#pragma once

#include "edge_emplacement.hpp"
#include "tree_tree_containment.hpp"

namespace PT {
  // a class that checks for subtrees of the guest to be displayed in a lowest tree component of the host
  // it is basically a TreeInTreeContainment checker
  template<StrictPhylogenyType MulSubtree,
           StrictPhylogenyType Guest,
           bool leaf_labels_only = true,
           StorageEnum MSTreeLabelStorage = vecS>
    requires std::is_same_v<typename MulSubtree::LabelType, typename Guest::LabelType>
  class TreeInComponent {
    static constexpr StorageEnum GuestLabelStorage = singleS; // so far we can only support single-labelled guests
    using TreeChecker = TreeInTreeContainment<MulSubtree, Guest, leaf_labels_only, MSTreeLabelStorage>;
    using LabelType = typename TreeChecker::LabelType;
    using NodeInfos = typename TreeChecker::NodeInfos;
    using LabelMatching = typename TreeChecker::LabelMatching;
    using MSTreeLabelNodeStorage = NodeStorage<MSTreeLabelStorage>;

    const Guest& guest;
    
    LabelMatching  SG_label_match;
    
    MulSubtree  subtree;
    TreeChecker subtree_display;

    // unzip the reticulations below u to create a MuL-tree
    //NOTE: this assumes that the cherry rule has been applied exhaustively
    template<StrictPhylogenyType Host>
    MulSubtree unzip_retis(const Host& host, const NodeDesc u, auto&& HG_label_match) {
      DEBUG2(std::cout << "unzipping reticulations under tree component below "<<u<<"...\n");
      DEBUG2(std::cout << "with label matching "<<HG_label_match<<"\n");
      MulSubtree T;
      NodeTranslation host_to_subtree; // track translation
      
      auto emplacer = EdgeEmplacers<false, Host>::make_emplacer(T, host_to_subtree);

      // to construct the multi-labeled tree, we use a special edge-traversal of the host without a SeenSet, so reticulations are visited multiple times
      EdgeTraversal<preorder, Host, void, void, void> my_dfs(u);
     
      const NodeDesc MULroot = emplacer.create_copy_of(u);
      emplacer.mark_root_directly(MULroot);

      for(const auto xy: my_dfs){
        auto [x, y] = xy.as_pair();
        if(host.out_degree(x) != 1){
          DEBUG2(std::cout << "got edge "<<x<<"->"<<y<<"\n");
          // skip reticulation chains
          while(host.out_degree(y) == 1) y = host.any_child(y);
          // add the edge to the subtree
          // NOTE: we'll make the emplacer forget whether y already has a copy in the MUL-tree, so that a new copy is added
          host_to_subtree.erase(y);
          const NodeDesc y_copy = emplacer.emplace_edge(x, y);
          // register the label if y has one
          const auto& ylabel = host.label(y);
          if((!leaf_labels_only || host.is_leaf(y)) && !ylabel.empty()) {
            std::cout << "mark - label("<<y<<") = "<<ylabel<<"\n";
            std::cout << "matching: "<<HG_label_match.at(ylabel)<<"\n";
            // register label and st_v in the label matching
            auto& subtree_nodes_with_label = append(SG_label_match, ylabel).first->second;
            append(subtree_nodes_with_label.first, y_copy);
            std::cout << "matched labels: "<< subtree_nodes_with_label <<"\n";
          }
        }
      }
      DEBUG2(std::cout << "got MUL-tree:\n"<<T<<"\n");
      return T;
    }

  public:
    // construct from network _host and node u, also pass the guest and a label matching from _host to _guest (or don't and we'll compute one)
    template<StrictPhylogenyType Host, class HG_Label_Matching>
    TreeInComponent(const Host& _host, const NodeDesc u, const Guest& _guest, HG_Label_Matching&& HG_label_match):
      guest(_guest),
      // use HG_label_match to fill the "guest"-side of SG_label_match
      SG_label_match(HG_label_match, [](auto&& node_sets){ return std::make_pair(MSTreeLabelNodeStorage(), std::move(node_sets.second)); } ),
      subtree(unzip_retis(_host, u, std::forward<HG_Label_Matching>(HG_label_match))),
      subtree_display(subtree, _guest, SG_label_match) // note: the checker may move out of the label matching
    {
      DEBUG2(std::cout << "\tconstructed TreeInComponent checker\n subtree is:\n"<< ExtendedDisplay(subtree)<<"\nguest is at "<<&guest<<":\n"<<ExtendedDisplay(guest)<<"\n");
    }

    // get the highest ancestor of v in the guest that is still displayed by the tree-component
    NodeDesc highest_displayed_ancestor(NodeDesc v) {
      DEBUG2(std::cout << "guest:\n"<<guest<<"\n");
      // step 1: unzip the lowest reticulations
      NodeDesc pv = guest.parent(v);
      DEBUG2(std::cout << "testing parent "<<pv<<" of "<<v<<"\n");
      while(1) {
        const auto& pv_disp = subtree_display.who_displays(pv);
        if(pv_disp.empty()) return v;
        v = pv;
        if(pv == guest.root()) return v;
        if((pv_disp.size() == 1) && (front(pv_disp) == subtree.root())) return v;
        pv = guest.parent(pv);
        DEBUG2(std::cout << "testing parent "<<pv<<" of "<<v<<"\n");
      }
    }

  };


}
