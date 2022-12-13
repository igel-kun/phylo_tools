
// preprocessing routines for scanwidth computation

#pragma once

namespace PT {

  // The preprocessing might decide to include an arc xy in all of the 'scanwidth bags' that contain another arc uv
  // in this case, uv represents TWO arcs instead of one. To achieve this, we will use weighted arcs instead


  // remove the shortcut over the given path and add its weight to all nodes of the path
  template<PhylogenyType Network, EdgeContainerType Edges, class EdgeWeightExtract>
  void remove_shortcut(Network& N, const Edges& path, EdgeWeightExtract&& edge_weight) {
    assert(path.size() > 1);
    const NodeDesc v = mstd::back(path).head();
    const NodeDesc u = mstd::front(path).tail();
    const auto uv = N.find_edge(u,v);
    if(!uv.is_invalid()) {
      const auto uv_weight = edge_weight(uv);
      N.remove_edge_no_cleanup(uv);
      for(const auto& xy: path)
        edge_weight(xy) += uv_weight;
    }
  }

  template<PhylogenyType Network, class EdgeWeightExtract>
  bool remove_shortcuts(Network& N, EdgeWeightExtract&& edge_weight) {
    using EdgeData = typename Network::EdgeData;
    std::cout<<"removing shortcuts\n";
    // we will sweep the network with a preorder scan, writing down preorder numbers
    // whenever we encounter an edge to a seen vertex with a larger preorder number,
    // we construct its corresponding path in reverse
    NodeMap<size_t> preorder_num;
    EdgeSet<EdgeData> shortcuts;
    append(preorder_num, N.root(), 0);
    size_t current_num = 1;
    // step 1: collect all shortcuts via all-edge-preorder traversal
    for(const auto uv: N.edges_preorder()) {
      const NodeDesc v = uv.head();
      const auto [v_iter, success] = append(preorder_num, v, current_num);
      if(!success) {
        const NodeDesc u = uv.tail();
        const size_t u_num = preorder_num.at(u);
        if(v_iter->second > u_num)
          append(shortcuts, uv);
      } else ++current_num;
    }
    // step 2: remove each shortcut; to this end, construct ONE of the paths this edge is cutting short by using preorder numbers
    for(const auto& uv: shortcuts) {
      const NodeDesc u = uv.tail();
      NodeDesc v = uv.head();
      const size_t u_num = preorder_num.at(u);
      const size_t v_num = preorder_num.at(v);
      std::deque<Edge<EdgeData>> path;
      do {
        // to find the next node w on the path, take any parent with preorder number between u's and v's
        for(const auto wv: N.in_edges(v)){
          const NodeDesc w = wv.tail();
          const size_t w_num = preorder_num.at(w);
          // NOTE: for the first arc on the path, we disallow w_num == u_num since, otherwise, we get the length-1 path uv again
          if((u_num + path.empty() <= w_num) && (w_num < v_num)) {
            path.push_front(wv);
            v = w;
            break;
          }
        }
      } while(v != u);
      std::cout << "found shortcut for path "<<path<<"\n";
      remove_shortcut(N, path, edge_weight);
    }
    return !shortcuts.empty();
  }


  // contract a trivial node either up or down
  // return the parent/child of u if it has now become trivial (and was not trivial before)
  template<PhylogenyType Network, class EdgeWeightExtract>
  NodeDesc contract_trivial_node(Network& N, const NodeDesc u, EdgeWeightExtract&& edge_weight) {
    std::cout << "contracting trivial node: "<<u<<"\n";
    NodeDesc newly_trivial = NoNode;
    if(N.in_degree(u) == 1) {
      const NodeDesc p = N.parent(u);
      if(N.out_degree(u) == 1) {
        const NodeDesc w = N.child(u);
        const auto pw = N.find_edge(p, w);
        if(!pw.is_invalid()){
          std::cout << "found edge "<< pw <<"\n";
          // step 1: if pw already exists, then remove the shortcut before contracting pu
          const std::array<typename Network::Edge, 2> path{N.find_edge(p,u), N.find_edge_fwd(u,w)};
          remove_shortcut(N, path, edge_weight);
          // step 2: contract pu
          N.contract_up(u, p);
        } else N.contract_up(u, p);
      } else {
        assert(N.out_degree(u) == 0);
        N.remove_node(u);
      }
      // step 3: check if p is now trivial
      const auto [p_in, p_out] = N.degrees(p);
      if((p_in <= 1) && (p_out == 1)) // if p_out == 0, then p was trivial before, so it's not 'newly' trivial
        newly_trivial = p;
    } else { // u is the root
      assert(N.out_degree(u) == 1);
      N.contract_down(u);
    }
    return newly_trivial;
  }


  template<PhylogenyType Network, class EdgeWeightExtract>
  bool remove_trivial_nodes(Network& N, EdgeWeightExtract&& edge_weight) {
    std::cout<<"contracting trivial nodes\n";
    // a node is trivial if its in- and out- degree is at most 1
    //NOTE: we cannot modify the network while iterating over its nodes, so we cache the trivial nodes in a vector
    NodeVec trivial_nodes = N.nodes_with([&](const NodeDesc u){ auto [ind,outd] = N.degrees(u); return (ind < 2) && (outd < 2); }).to_container();
    bool applied = !trivial_nodes.empty();
    while(!trivial_nodes.empty() && !N.edgeless()) {
      // each trivial node is contracted onto their parent/child depending on which is unique
      const NodeDesc t = contract_trivial_node(N, mstd::value_pop(trivial_nodes), edge_weight);
      if(t != NoNode) append(trivial_nodes, t);
    }
    return applied;
  }

  template<PhylogenyType Network, class EdgeWeightExtract = DefaultExtractData<Ex_edge_data, Network>>
  bool apply_preprocessing(Network& N, EdgeWeightExtract&& edge_weight = EdgeWeightExtract()) {
    const size_t pre_edges = N.num_edges();
    do {
      std::cout << "network is now:\n"<<N<<"\n";
      remove_shortcuts(N, edge_weight);
    } while(remove_trivial_nodes(N, edge_weight));
    return N.num_edges() != pre_edges;
  }

}
