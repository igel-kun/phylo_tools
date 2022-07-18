
#pragma once

#include <vector>
#include <string_view>
#include <charconv>
#include "utils/types.hpp"
#include "utils/set_interface.hpp"
#include "utils/iter_bitset.hpp"
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"

namespace PT{
  //! an exception for problems with the input string
  struct MalformedNewick : public std::exception {
    const ssize_t pos;
    const std::string msg;

    MalformedNewick(const std::string_view newick_string, const ssize_t _pos, const std::string _msg = "unknown error"):
      pos(_pos), msg(_msg + " (position " + std::to_string(_pos) + ")" + DEBUG3(" - relevant substring: "+newick_string.substr(_pos)) + "") {}

    const char* what() const throw() {
      return msg.c_str();
    }
  };

  // compute the extended newick string for a subnetwork rooted at sub_root of a network N with retis_seen reticulations considered as treated
  template<class _Network, class Container = PT::NodeSet>
  std::string get_extended_newick(const _Network& N, const NodeDesc sub_root, Container&& retis_seen = Container()) {
    std::string accu = "";
    if((N.in_degree(sub_root) <= 1) || !mstd::test(retis_seen, sub_root)){
      accu += '(';
      for(const auto& w: N.children(sub_root))
        accu += get_extended_newick(N, w, retis_seen) + ',';
      // remove last "," (or the "(" for leaves)
      accu.pop_back();
      if(!N.is_leaf(sub_root)) accu += ')';
    }
    if constexpr (_Network::Node::has_label) accu += N.label(sub_root);
    if(N.in_degree(sub_root) > 1) {
      accu += "#H" + std::to_string(sub_root);
      mstd::append(retis_seen, sub_root);
    }
    return accu;
  }

  // compute the extended newick string for a network N
  template<class _Network>
  std::string get_extended_newick(const _Network& N) { return get_extended_newick(N, N.root()) += ';'; }

  template<bool store_degree = true>
  struct NodeDescAndDegree: public std::pair<NodeDesc, Degree> {
    using Parent = std::pair<NodeDesc, Degree>;
    using Parent::Parent;

    NodeDesc& get_node() { return this->first; }
    const NodeDesc& get_node() const { return this->first; }
    Degree& get_degree() { return this->second; }
    const Degree& get_degree() const { return this->second; }
  };
  template<>
  struct NodeDescAndDegree<false> {
    NodeDesc x;
    NodeDescAndDegree(const NodeDesc& _x, const Degree&): x(_x) {}
    NodeDesc& get_node() { return x; }
    const NodeDesc& get_node() const { return x; }
    friend std::ostream& operator<<(std::ostream& os, const NodeDescAndDegree<false>& ndd) { return os << ndd.x; }
  };

  //! a newick parser
  //NOTE: we parse newick from the back to the front since the node names are _appended_ to the node instead of _prepended_
  //NOTE: node numbers will be consecutive (0 = root) and follow a pre-order numbering of a spanning-tree
  //      this allows you to use RONetworks and anything needing pre-order numbers
  //NOTE: output is done via a functor with 'void operator()(NodeDesc, NodeDesc, std::string&&)' - edge data has to be parsed from the 3rd argument
#warning TODO: turn this into an iterator in order to avoid carrying around edgesets!
  template<class NodeCreationFunctor, class EdgeCreationFunctor, bool allow_non_binary = true, bool allow_junctions = true>
  class NewickParser {
    // a HybridInfo is a name of a hybrid together with it's hybrid-index
    using HybridIndex = uint32_t;
    using HybridInfo = std::pair<std::string_view, HybridIndex>;

    const std::string_view newick_string;

    // map a hybrid-index to a node index (and in-degree) so that we can find the corresponding hybrid when reading a hybrid number
    std::unordered_map<HybridIndex, NodeDescAndDegree<allow_non_binary>> hybrids;

    // pointer to the current read-position in the newick string
    ssize_t back;

    bool parsed = false;

    NodeCreationFunctor create_node;
    EdgeCreationFunctor create_edge;

  public:

    NewickParser(const std::string& _newick_string,
                 NodeCreationFunctor& _create_node,
                 EdgeCreationFunctor& _create_edge):
      newick_string(_newick_string),
      back(_newick_string.length() - 1),
      create_node(_create_node),
      create_edge(_create_edge)
    {}

    bool is_tree() const { return hybrids.empty(); }
    NodeDesc parse() { return read_tree(); }

    // a tree is a branch followed by a semicolon
    NodeDesc read_tree() {
      NodeDesc root = NoNode;
      skip_whitespaces();
      if(back >= 0) {
        if(newick_string.at(back) == ';')
          --back;
        else throw MalformedNewick(newick_string, back, "expected ';' but got \"" + newick_string.substr(back) + "\"\n");
        DEBUG5(std::cout << "parsing \"" << newick_string << "\""<<std::endl);
        root = read_subtree();
      }
      parsed = true;
      std::cout << "done parsing, root is "<<root<<"\n";
      return root;
    }

  private:

    void skip_whitespaces() { while((back >= 0) && std::isspace(newick_string.at(back))) --back; }

    // check if this is a hybrid and return name and hybrid number
    HybridInfo get_hybrid_info(const std::string_view name) {
      HybridInfo result = {{}, UINT32_MAX};
      const size_t sharp = name.rfind('#');
      if(sharp != std::string::npos){ // if name contains '#', then it's a hybrid
        result.first = name.substr(0, sharp); // the part before the '#' is the name of that hybrid
        const size_t hybrid_num_start = name.find_first_of("0123456789", sharp); // the part after the '#' is it's hybrid index, used to reference it later
        if(hybrid_num_start != std::string::npos) {
          std::from_chars(name.data() + hybrid_num_start, name.data() + name.size(), result.second);
          //const std::string hybrid_type = name.substr(sharp + 1, hybrid_num_start - sharp - 1);
        } else throw MalformedNewick(newick_string, back, "found '#' but no hybrid number: \"" + name + "\"\n");
      }
      return result;
    }

    // a subtree is a leaf or an internal vertex
    NodeDesc read_subtree() {
      NodeDesc root;
      skip_whitespaces();
      
      // read the name of the root, any non-trailing whitespaces are considered part of the name
      std::string_view root_name = read_name();

      // find if root is a hybrid and, if so, it's number
      HybridInfo hyb_info = get_hybrid_info(root_name);
      
      if(hyb_info.second != UINT32_MAX){
        const auto [iter, success] = hybrids.try_emplace(hyb_info.second, NoNode, 0);
        auto& stored = iter->second;
        if(!success){
          // if root is a known hybrid, then lookup its index in 'hybrids' and increase registered in-degree
          // we've already seen a hybrid with this index - so replace 'root' by the other node's index
          // increase the registered in-degree of 'root'
          if constexpr (!allow_non_binary)
            if(++stored.get_degree() == 3)
              throw MalformedNewick(newick_string, back, "found non-binary node, which has been explicitly disallowed");
        } else stored.get_node() = create_node(hyb_info.first); // if root was an unknown hybrid, then register it
        root = stored.get_node();
       
        // if the subtree dangling from root is non-empty, then recurse
        if((back > 0) && newick_string.at(back) == ')') read_internal<true>(root);
      } else {
        // if root is not a hybrid, then just register it
        root = create_node(root_name);
        if((back > 0) && newick_string.at(back) == ')') read_internal<false>(root);
      }

      skip_whitespaces();
      return root;
    }

    // an internal vertex is ( + branchlist + )
    template<bool root_is_hybrid>
    void read_internal(const NodeDesc& root) {
      assert(back >= 2);
      
      if(newick_string.at(back) == ')') --back;
      else throw MalformedNewick(newick_string, back, std::string_view("expected ')' but got '") + newick_string.at(back) + "'");

      read_branchset<root_is_hybrid>(root);
      
      if(newick_string.at(back) == '(') --back;
      else throw MalformedNewick(newick_string, back, std::string_view("expected '(' but got '") + newick_string.at(back) + "'");
    }

    // a branchset is a comma-separated list of branches
    template<bool root_is_hybrid = false>
    void read_branchset(const NodeDesc root) {
      std::unordered_set<NodeDesc> children_seen;
      children_seen.insert(read_branch(root));
      while(newick_string.at(back) == ',') {
        if constexpr (root_is_hybrid){
          if constexpr (!allow_non_binary)
            throw MalformedNewick(newick_string, back, "found non-binary node, which has been explicitly disallowed");
          if constexpr (!allow_junctions)
            throw MalformedNewick(newick_string, back, "found reticulation with multiple children ('junction') which has been explicitly disallowed");
        }
        --back;
        const NodeDesc new_child = read_branch(root);
        if(!children_seen.emplace(new_child).second)
          throw MalformedNewick(newick_string, back, "read double edge "+ std::to_string(root) + " --> "+std::to_string(new_child));
        if(back < 0) throw MalformedNewick(newick_string, back, "unmatched ')'");
      }
      if constexpr (!allow_non_binary)
        if(children_seen.size() == 3)
          throw MalformedNewick(newick_string, back, "found non-binary node, which has been explicitly disallowed");

    }

    // a branch is a subtree + a length
    // return the head of the read branch
    NodeDesc read_branch(const NodeDesc root) {
      std::string_view data = read_data();
      const NodeDesc child = read_subtree();
      create_edge(root, child, data);
      return child;
    }

    // read edge data as string
    std::string_view read_data() {
      std::string_view result;
      const size_t sep = newick_string.find_last_of(",():", back);
      if((sep != std::string::npos) && (newick_string.at(sep) == ':')){
        result = newick_string.substr(sep + 1, back - sep);
        back = sep - 1;
      }
      return result;
    }


    // read the root's name
    std::string_view read_name() {
      size_t next = newick_string.find_last_of("(),", back);
      // if we cannot find any of "()," before back, then the name stars at newick_string[0]
      if(next == std::string::npos) next = 0;
      const size_t length = back - next;
      back = next;
      return newick_string.substr(next + 1, length);
    }

  };

  using NodeFromString = std::function<NodeDesc(const std::string_view)>;
  template<PhylogenyType Phylo>
  using AdjacencyFromString = std::function<typename Phylo::Adjacency(const NodeDesc d, const std::string_view)>;

  template<class F>
  concept NodeFromStringFunction = std::invocable<F, const std::string_view>;
  template<class F>
  concept AdjacencyFromStringFunction = std::invocable<F, const NodeDesc, const std::string_view>;

  template<StrictPhylogenyType Phylo>
  struct DefaultNodeCreation {
    using Node = typename Phylo::Node;

    NodeDesc operator()(std::string_view s) const {
      // if the network's nodes have data, then s is first passed into the node-creation (which may modify s)
      const NodeDesc result = Phylo::create_node(s);
      // if the network's nodes have labels, then whatever remains of s is assigned as label
      if constexpr (Phylo::has_node_labels) Phylo::node_of(result).label() = s;
      return result;
    }
  };
  template<PhylogenyType Phylo>
  struct DefaultAdjCreation {
    using Adjacency = typename Phylo::Adjacency;

    auto operator()(const NodeDesc d, std::string_view s) const {
      if constexpr (Phylo::has_edge_data) {
        return Adjacency(d, s);
      } else return d;
    }
  };

  // define const references to these global functions
  template<PhylogenyType Phylo>
  using DefaultNodeCreationRef = const DefaultNodeCreation<Phylo>&;
  template<PhylogenyType Phylo>
  using DefaultAdjCreationRef = const DefaultAdjCreation<Phylo>&;

  // build phylogeny from a string and, optionally, a node- and edge- creation functions
  template<PhylogenyType Phylo,
           NodeFromStringFunction CreateNode = DefaultNodeCreation<Phylo>,
           AdjacencyFromStringFunction CreateAdjacency = DefaultAdjCreation<Phylo>>
  Phylo parse_newick(const std::string& in,
                     CreateNode&& _create_node = CreateNode(),
                     CreateAdjacency&& _create_adjacency = CreateAdjacency())
  {
    Phylo N; // this allows NRVO
    const auto create_node = [&](const std::string_view data){ N.count_node(); return _create_node(data); };
    const auto create_edge = [&](const NodeDesc& u, const NodeDesc& v, const std::string_view data){ N.add_edge(u, _create_adjacency(v, data)); };
    const NodeDesc root = NewickParser(in, create_node, create_edge).parse();
    N.mark_root(root);
    return N;
  }
  // build a phylogeny from a string and, possibly, an edge-creation function, but neither given phylogeny nor node-creation function
  template<PhylogenyType Phylo, AdjacencyFromStringFunction CreateAdjacency>
  Phylo parse_newick(const std::string& in, CreateAdjacency&& create_adjacency) {
    return parse_newick<Phylo>(in, DefaultNodeCreation<Phylo>(), std::forward<CreateAdjacency>(create_adjacency));
  }
 
  template<class Network, class... Args>
  Network parse_newick(std::istream& in, Args&&... args) {
    std::string in_line;
    std::getline(in, in_line);
    return parse_newick<Network>(in_line, std::forward<Args>(args)...);
  }
}

