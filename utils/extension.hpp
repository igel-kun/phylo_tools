
#pragma once



namespace PT{

  template<class _Node = uint32_t>
  class Extension: public std::vector<_Node>
  {
    using Parent = std::vector<_Node>;
  public:

    // import constructors
    using Parent::Parent;
    using Node = _Node;


    void get_inverse(std::unordered_map<Node, uint32_t>& inverse) const
    {
      for(unsigned i = 0; i < this->size(); ++i) inverse.emplace(this->at(i), i);
    }

    // return if the extension is valid for a given network
    template<class Network>
    bool is_valid_for(const Network& N)
    {
      if(this->size() == N.num_nodes()){
        // construct inverse of the extension, mapping each node to its position
        std::unordered_map<Node, uint32_t> inverse;
        get_inverse(inverse);

        // check if all arcs in the network go backwards in the extension
        for(const auto& uv: N.get_edges())
          if(inverse.at(uv.head()) > inverse.at(uv.tail())) return false;

        return true;
      } else return false;
    }

    // return the scanwidth of the extension in the network N
    template<class Network>
    uint32_t scanwidth(const Network& N) const
    {
      using SWIter = SecondIterator<std::unordered_map<Node, uint32_t>>;
      std::unordered_map<Node, uint32_t> sws = sw_map(N);
      return *(std::max_element(SWIter(sws.begin()), SWIter(sws.end())));
    }

    // get mapping of nodes to their scanwidth in the extension
    template<class Network, class _Container>
    void sw_map(const Network& N, _Container& out) const
    {
      // first, compute the extension tree
      std::unordered_map<Node, std::vector_list<Node>> ext_tree_adj_vec;
      ext_to_tree(N, *this, ext_tree_adj_vec);
      std::cout << "computed ext tree: "<< ext_tree_adj_vec<<std::endl;

      for(const Node u: *this){
        uint32_t sw_u = N.in_degree(u);
        if(!N.is_leaf(u))
          for(const Node v: ext_tree_adj_vec.at(u))
            sw_u += out.at(v) - 1; // only the arc uv contributes to sw[v] - sw[u]
        append(out, u, sw_u);
        DEBUG3(std::cout << "found sw("<<u<<") = "<<sw_u<<std::endl);
      }
    }

    template<class Network, class _Container = std::unordered_map<Node, uint32_t>>
    _Container sw_map(const Network& N) const
    {
      _Container result;
      sw_map(N, result);
      return result;
    }

  };


}// namespace

