
#pragma once


namespace PT{

  // the fake mul tree keeps a network and a root (any vertex inside the network) around and minicks a MUL tree
  class FakeMULTree{
    typedef Network::Node Node;

    const Network& N;
    const uint32_t root;
    const uint32_t N_vertices;
    
    // new virtual leaves with their virtual label (a virtual child has an index >= N_vertices)
    LNodeVec fake_leaves;
    // the parent of each virtual leaf
    IndexVec fake_parents;


    // get the unique label below a reticulation
    const std::string& get_label_below(const Node& reti) const
    {
      assert(reti.succ.size() == 1);
      cont uint32_t child_idx = reti.succ[0];
      const Node& child = N.get_vertex(child_idx);
      
      if(child.is_leaf())
        return N.get_name(child_idx);
      else
        return get_label_below(child);
    }

    // fill the vector of virtual leaves and their virtual parents
    void construct_fake_leaves(const Node& sub_root)
    {
      for(uint32_t i = 0; i < sub_root.pred.size(); ++i){
        const uint32_t child_idx = sub_root.pred[i];
        const Node& child = N.get_vertex(child_idx);
        
        if(child.is_reti()){
          // if this child is a reticulation, construct a new virtual child with the unique label below the reticulation
          fake_leaves.emplace_back(N_vertices + fake_leaves.size(), get_label_below(child));
          fake_parents.push_back(sub_root);
        } else if(child.is_leaf()) {
          // we found a real leaf
          fake_leaves.emplace_back(N_vertices + fake_leaves.size(), N.get_name(child_idx));
          fake_parents.push_back(sub_root);
        } else construct_fake_leaves(child);
      }
    }

  public:
    FakeMULTree(const Network& _N, const uint32_t _root):
      N(_N), root(_root) 
    {
     assert(!root.is_reti());
      construct_fake_leaves(N.get_vertex(root));
    }

    uint32_t LCA(const uint32_t x, const uint32_t y) const
    {
      if(x == y) return x;
      // repalce fake leaves by their parents
      return N.LCA(
        (x >= N_vertices) ? fake_parents[x - N_vertices] : x,
        (y >= N_vertices) ? fake_parents[y - N_vertices] : y
        );
    }

    const LNodeVec& get_leaves_labeled() const
    {
      return fake_leaves;
    }
  };
}
