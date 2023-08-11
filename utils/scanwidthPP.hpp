
// preprocessing routines for scanwidth computation

#pragma once

#include "slope.hpp" // slope reduction
#include "types.hpp"
#include "phylogeny.hpp"
#include "shortcuts.hpp"

namespace PT {
  // The preprocessing might decide to include an arc xy in all of the 'scanwidth bags' that contain another arc uv
  // in this case, uv represents TWO arcs instead of one. To achieve this, we will use WEIGHTED arcs instead

  // NOTE: EdgeWeightExtract can either take a Network::Adjacency or a Network::Edge and must return a REFERENCE to the edge weight
  template<StrictPhylogenyType Network, class EdgeWeightExtract = DefaultExtractData<Ex_edge_data, Network>>
  struct ScanwidthPreprocessor {
    using Adjacency = typename Network::Adjacency;
    using Edge = typename Network::Edge;
    static constexpr bool call_with_adj = (std::is_invocable_v<EdgeWeightExtract, Adjacency>);

    using DataRef = std::conditional_t<call_with_adj, std::invoke_result_t<EdgeWeightExtract, Adjacency>, std::invoke_result_t<EdgeWeightExtract, Edge>>;

    static_assert(std::is_reference_v<DataRef>);


    Network& N;
    EdgeWeightExtract edge_weight;

    ScanwidthPreprocessor(Network& _N, EdgeWeightExtract&& _edge_weight):
      N(_N), edge_weight(_edge_weight)
    {}
    ScanwidthPreprocessor(Network& _N): N(_N) {}

    DataRef get_edge_weight(const Edge& uv) {
      if constexpr (call_with_adj) {
        return edge_weight(uv.head());
      } else return edge_weight(uv);
    }
    DataRef get_edge_weight(const Adjacency& u, const NodeDesc v) {
      if constexpr (call_with_adj) {
        return edge_weight(u);
      } else return edge_weight(Edge{reverse_edge_tag{}, v, u});
    }


    template<bool reverse = false, EdgeContainerType Edges>
    auto path_start_and_end(const Edges& path) {
      if constexpr (reverse)
        return std::pair<NodeDesc, NodeDesc>(mstd::back(path).tail(), mstd::front(path).head());
      else
        return std::pair<NodeDesc, NodeDesc>(mstd::front(path).tail(), mstd::back(path).head());
    }

    // remove the shortcut over the given path and add its weight to all nodes of the path
    template<bool reverse = false, EdgeContainerType Edges>
    void remove_shortcut(const Edges& path) {
      assert(path.size() > 1);
      const auto [u,v] = path_start_and_end<reverse>(path);
      const auto uv = N.find_edge(u,v);
      if(!uv.is_invalid()) {
        const auto uv_weight = get_edge_weight(uv);
        N.remove_edge_no_cleanup(uv);
        for(const auto& xy: path)
          get_edge_weight(xy) += uv_weight;
      }
    }

    template<class T>
    bool remove_shortcuts(T&& arg) {
      DEBUG4(std::cout<<"removing shortcuts from:\n"<< ExtendedDisplay(N) <<"\n");
      
      // step 1: collect all shortcuts
      const auto shorts = detect_shortcuts<NodeMap<NodeDesc>, true, Network>(std::forward<T>(arg));
      const NodeMap<NodeDesc> shortcuts = shorts.get_all_shortcuts();

      // step 2: remove each shortcut; to this end, construct ONE of the paths this edge is cutting short by using preorder numbers
      for(const auto [u, v]: shortcuts) {
        DEBUG4(std::cout << "removing shortcut "<<u<<"->"<<v<<"\n");
        const NetEdgeVec<Network> uv_path = shorts.template get_path<NetEdgeVec<Network>>(u, v);
        DEBUG4(std::cout << "using path "<<uv_path<<"\n");
        remove_shortcut(uv_path);
      }
      return !shortcuts.empty();
    }

    void apply_to_inedge(const NodeDesc x, auto& do_something) {
      if constexpr (call_with_adj) {
        const Adjacency& p_adj = Network::parent(x);
        do_something(edge_weight(p_adj), p_adj);
      } else {
        const Edge px = Network::any_inedge(x);
        do_something(edge_weight(px), px.tail());
      }
    }

    // given a path-end and a vector of weights, contract all edges whose weight does not correspond to the weight-vector
    bool contract_edges_according_to_weights(const NodeDesc path_start,
                                             const NodeDesc last_on_path,
                                             const NodeDesc path_end,
                                             auto weight_iter,
                                             const size_t offset = 0) {
      bool result = false;
      NodeDesc x = last_on_path;
      
      auto contracter = [&](auto& weight, const NodeDesc x_parent) mutable {
        std::cout << "considering edge between "<<x<<" and its parent "<<x_parent<<" for contraction (offset: "<<offset<<")\n";
        if(*weight_iter != weight) {
          if(x_parent != path_start){
            // NOTE: we will not contract the uppermost node of a path since
            //    otherwise, completing the partial extension into a whole extension might put some other nodes before the end of the path
            std::cout << "contracting edge between "<<x<<" and its parent "<<x_parent<<" (path start is "<<path_start<<")\n";
            if(N.contract_up_abort(x, x_parent) != 0) weight += offset;
          } else weight += offset;
          result = true;
        } else {
          weight += offset;
          ++weight_iter;
        }
        x = x_parent;
      };

      while(x != path_start) {
        DEBUG4(std::cout << "climbing to "<<x<<"\n");
        assert(Network::in_degree(x) == 1);
        apply_to_inedge(x, contracter);
        DEBUG4(std::cout << "next stop: "<<x<<"\n");
      }
      DEBUG5(std::cout << "after contractions:\n"<<N<<"\n");
      return result;
    }

    // apply path reduction to the given path (using slope-reduction)
    bool treat_path_end(const Adjacency& last_on_path, const NodeDesc path_end) {
      using Weight = std::remove_cvref_t<std::invoke_result_t<EdgeWeightExtract, Edge>>;
      DEBUG4(std::cout << "treating path-end "<<last_on_path<<" --> " << path_end<<"\n");
      // step 1: write down the edge weights of the path in a vector
      std::vector<Weight> weights;
      append(weights, get_edge_weight(last_on_path, path_end));

      NodeDesc x = last_on_path;
      auto weight_appender = [&](const auto weight, const NodeDesc x_parent) mutable { append(weights, weight); x = x_parent; };
      do{
        assert(Network::in_degree(x) == 1);
        apply_to_inedge(x, weight_appender);
      } while(Network::is_suppressible(x));
      const NodeDesc path_start = x;
      // weights should contain at least 2 weights, otherwise u was not a path_end!
      assert(weights.size() > 1);
      // if xu is an edge, then the network contains a shortcut, so employ the shortcut reduction here
      size_t weight_offset = 0;
      const auto xv = Network::find_edge(x, path_end);
      if(!xv.is_invalid())
        weight_offset = get_edge_weight(xv);
        // defer removing xv such as not to disturb last_on_path
      const bool has_offset = (weight_offset != 0);
      
      // step 2: apply slopw-reduction to the weight-vector
      const size_t old_length = weights.size();
      SlopeReduction::apply(weights);
      assert(weights.size() >= 1);
      DEBUG3(std::cout << "weights after slope reduction: "<<weights<<" (offset: "<<weight_offset<<")\n");
      
      if(has_offset || (weights.size() < old_length)){
        const NodeDesc old_last = last_on_path;
        // use weight-0 as 'stop-token' in case everything has been removed from the weight-sequence
        // also remove the last weight since this is the weight of the uppermost edge, which will never be deleted anyways
        weights.back() = 0;
          
        if(has_offset){
          get_edge_weight(last_on_path, path_end) += weight_offset;
          N.remove_edge_no_cleanup(xv);
        }

        // step 3: go through the path and the reduced vector, contracting all edges whose weight has been removed
        if(weights.size() == 1) {
          // if the slope reduction reduced everything, then all weights are equal, so we'll get a single edge as result, no need to call contract_bla_..
          DEBUG4(std::cout<<"now hanging "<< path_end<<" from "<<old_last<<" to "<<path_start<<"\n");

          N.transfer_child_abort(path_end, old_last, path_start);
          N.remove_upwards_no_suppression(old_last);
          return true;
        } else return contract_edges_according_to_weights(x, old_last, path_end, std::next(weights.begin()), weight_offset) ? true : has_offset;
      } else return false;
    }


    // apply path reduction to the given path_ends (using slope-reduction)
    bool treat_path_ends(const NodeSet& path_ends) {
      bool result = false;
      DEBUG4(std::cout << N << "\ninput: "<<path_ends<<"\n");
      for(const NodeDesc path_end: path_ends) {
        DEBUG4(std::cout << "treating path end "<<path_end<<" with degrees "<<Network::degrees(path_end)<<"\n");
        if((Network::in_degree(path_end) > 1) || (Network::out_degree(path_end) > 1)) {
          for(const Adjacency& last_on_path: Network::parents(path_end)){
            DEBUG4(std::cout << "treating parent "<<last_on_path<<" of "<<path_end<<"\n");
            if(Network::is_suppressible(last_on_path))
              result |= treat_path_end(last_on_path, path_end);
          }
        }
      }
      return result;
    }


    // exhaustively remove leaves with in-degree 1
    bool remove_leaves(auto& leaves, auto& path_ends) {
      NodeDesc p = NoNode;
      while(!leaves.empty()){
        const NodeDesc v = mstd::value_pop(leaves);
        DEBUG5(std::cout << "next leaf: "<<v<<" - degrees: "<<Network::degrees(v)<<"\n");
        assert(Network::in_degree(v) == 1);
        p = Network::parent(v);
        N.remove_node(v);
        switch(Network::out_degree(p)){
          case 0:
            if(Network::in_degree(p) == 1) append(leaves, p);
            break;
          case 1: { // if v's parent p is now out-degree 1 (it has been out-deg 2 before), then p is no longer a path-end, but its unique child may be one
              erase(path_ends, p);
              const NodeDesc w = Network::child(p);
              DEBUG5(std::cout << "consider child "<<w<<" of "<<p<<" as new path-end\n");
              const auto [w_ind, w_out] = Network::degrees(w);
              if((w_ind > 1) || (w_out > 1)) append(path_ends, w);
            }
        }
      }
      return (p != NoNode); // if p changed, then we removed a node :)
    }
    // remove all leaves and return the set of remaining outdeg-0 vertices
    template<NodeContainerType Nodes>
    Nodes remove_leaves(Nodes leaves) {
      Nodes new_leaves;
      while(!leaves.empty()) {
        const NodeDesc v = mstd::value_pop(leaves);
        DEBUG5(std::cout << "next leaf: "<<v<<" - degrees: "<<Network::degrees(v)<<"\n");
        if(Network::in_degree(v) == 1) {
          const NodeDesc p = Network::parent(v);
          N.remove_node(v);
          if(Network::out_degree(p) == 0) append(leaves, p);
        } else append(new_leaves, v);
      }
      leaves = std::move(new_leaves);
      return leaves;
    }


    void get_leaves_and_path_ends(auto& leaves, auto& path_ends){
      for(const NodeDesc u: N.nodes()){
        const auto [ind,outd] = Network::degrees(u);
        if(ind == 1) {
          switch(outd){
            case 0:
              append(leaves, u);
              break;
            case 1: {
                const NodeDesc u_child = Network::child(u);
                const auto [uc_ind, uc_out] = Network::degrees(u_child);
                if((uc_ind > 1) || (uc_out > 1)) append(path_ends, u_child);
              }
            default: {}
          }
        }
      }
    }

    bool remove_trivial_nodes() {
      DEBUG4(std::cout<<"contracting trivial nodes\n");
      //NOTE: we cannot modify the network while iterating over its nodes, so we cache the trivial nodes in a vector
      // step 0: collect leaves and endpoints of paths with at least 2 edges
      NodeVec leaves;
      NodeSet path_ends;
      get_leaves_and_path_ends(leaves, path_ends);
      DEBUG3(std::cout << "preprocessing leaves "<<leaves<<" and path ends "<<path_ends<<"\n");
      // step 1: exhaustively remove all leaves from the network
      bool result = remove_leaves(leaves, path_ends);
      // step 2: apply path reduction to the paths ending in the path_ends
      result |= treat_path_ends(path_ends);
      return result;
    }

    bool apply_preprocessing() {
      const size_t pre_edges = N.num_edges();
      remove_shortcuts(remove_leaves(N.leaves().template to_container<NodeVec>()));
      while(remove_trivial_nodes() && remove_shortcuts(N)) {
        DEBUG3(std::cout << "network is now:\n"<<N<<"\n");
      }
      DEBUG4(std::cout << "edge weights:\n"; for(const auto& uv: N.edges()) std::cout << uv << "\n");
      return N.num_edges() != pre_edges;
    }
  };
  
  template<StrictPhylogenyType Network, class EdgeWeightExtract>
  auto make_sw_preprocessor(Network& N, EdgeWeightExtract&& edge_weight) {
    return ScanwidthPreprocessor<Network, EdgeWeightExtract>(N, std::forward<EdgeWeightExtract>(edge_weight));
  }
  template<StrictPhylogenyType Network>
  auto make_sw_preprocessor(Network& N) { return ScanwidthPreprocessor<Network, DefaultExtractData<Ex_edge_data, Network>>(N); }

  template<StrictPhylogenyType Network, class EdgeWeightExtract = DefaultExtractData<Ex_edge_data, Network>>
  bool apply_sw_preprocessing(Network& N, EdgeWeightExtract&& edge_weight) {
    return make_sw_preprocessor(N, std::forward<EdgeWeightExtract>(edge_weight)).apply_preprocessing();
  }
  template<StrictPhylogenyType Network>
  bool apply_sw_preprocessing(Network& N) { return make_sw_preprocessor(N).apply_preprocessing(); }

}
