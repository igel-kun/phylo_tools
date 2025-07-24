
#pragma once

namespace PT {

  // ========== ReticulationVisible ==========
  // describe what ReticulationVisible does...

  // ------- ReticulationVisible: helpers ---------
  
  // ------- ReticulationVisible: main class ---------

  // return a list of tree-component roots r that have a "private" leaf, that is, each r has a tree-path to some leaf
  template<StrictPhylogenyType Net>
  NodeSet get_comp_roots_with_leaf_path(const Net& N) {
    NodeSet result;
    NodeSet has_leaf_path;
    for(const auto uv: N.edges_postorder()) {
      const auto [u,v] = uv.as_pair();
      switch(Net::type_of(v)) {
        default:
          assert(false);
        case NODE_TYPE_LEAF:
          append(has_leaf_path, {u, v});
          break;
        case NODE_TYPE_INTERNAL_TREE:
          if(test(has_leaf_path, v))
            append(has_leaf_path, u);
          break;
        case NODE_TYPE_INTERNAL_RETI:
          if(test(has_leaf_path, v))
            append(result, v);
      }
    }
    return result;
  }
 
  // ------- ReticulationVisible: factories ---------
  
  // ------- ReticulationVisible: concepts ---------
  
  // ------- ReticulationVisible: deduction guides ---------
  
  // ------- ReticulationVisible: defaults ---------

}
