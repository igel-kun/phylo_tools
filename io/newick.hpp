
#pragma once

#include <vector>
#include <string_view>
#include <charconv>

#include "utils/iter_bitset.hpp"
#include "utils/set_interface.hpp"
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"
#include "utils/except.hpp"

#include "utils/types.hpp"
#include "utils/PTconfig.hpp"
#include "io/common.hpp"


/* Specification
 * 1. extended newick always ends with ';'
 * 2. each subtree is (...)[label][hybrid][data], where
 *    [label] is an optional label that cannot contain any of "#:,();" (f.ex.: arabidopsis from the neighbor's garden)
 *    [hybrid] is an optional hybrid specifier that must start with '#' and be a single number fitting in 64bit (f.ex.: #H34)
 *    [data] = [:edge-data][|node-data] where
 *        each of [:edge-data] and [|node-data] is a string from which edge/node-data can be constructed
 *            the string must start with ':' (edge-data) or '|' (node-data)
 *            the strings cannot contain any of ",()" (and "|" for edge-data)
 *            if you have multiple node/edge data items, I suggest separating the items with ':' and split the string in the constructor of your data class
 */


namespace PT{

  // ------ WRITE OUTPUT --------
  // compute the extended newick string for a subnetwork rooted at sub_root of a network N with retis_seen reticulations considered as treated
  template<StrictPhylogenyType Network_,
           DataExtracterType Extracter_ = DefaultDataExtracter<Network_>,
           NodeMapType HybNum = NodeMap<uint32_t>>
  void write_extended_newick_below(std::ostream& os,
                             const Network_& N,
                             const auto sub_root,
                             Extracter_&& extracter = Extracter_(),
                             HybNum&& hybrid_number = HybNum())
  {
    using Extracter = std::remove_cvref_t<Extracter_>;
    ssize_t hn = -1;
    bool register_node = true;
    if(not N.is_leaf(sub_root)) {
      if(N.in_degree(sub_root) > 1) {
        const auto [iter, success] = hybrid_number.try_emplace(sub_root, hybrid_number.size());
        register_node = success;
        hn = iter->second;
      }
      if(register_node) {
        os << '(';
        bool not_first = false;
        for(const auto& w: N.children(sub_root)) {
          if(not not_first) not_first = true; else os << ',';
          write_extended_newick_below(os, N, static_cast<const NodeDesc>(w), extracter, hybrid_number);
        
          // first, write the edge-data
          std::ostringstream data_oss;
          if constexpr (not Extracter::ignoring_edge_data) {
            const std::string out = std::to_string(extracter(Ex_edge_data{}, w));
            if(not out.empty() || config::write_empty_edge_data)
              data_oss << config::NW_delimeters.start_of_edge_data << out;
          }
          // then write the node data
          if constexpr (not Extracter::ignoring_node_data) {
            const std::string out = std::to_string(extracter(Ex_node_data{}, sub_root));
            if(not out.empty() || config::write_empty_node_data)
              data_oss << config::NW_delimeters.start_of_node_data << out;
          }
          os << data_oss.str();
        }
        if(not N.is_leaf(sub_root)) os << ')';
      }
    }
    if constexpr (not Extracter::ignoring_node_labels)
      if(register_node) // if we already printed the label in the past, there is no reason to reprint it
        os << extracter(Ex_node_label{}, sub_root);
    if(hn != -1) os << config::NW_start_of_hybrid_spec << 'H' << hn;
  }

  // write the extended newick string for a network N onto a stream
  template<StrictPhylogenyType Network_, DataExtracterType Extracter = DefaultDataExtracter<Network_>>
  void write_extended_newick(std::ostream& os, const Network_& N, Extracter&& extracter = Extracter()) {
    write_extended_newick_below(os, N, N.root(), std::forward<Extracter>(extracter));

    // finally, write the node-data of the root
    if constexpr (not Extracter::ignoring_node_data) {
      auto node_data = extracter(Ex_node_data{}, N.root());
      if(node_data) {
        os << config::NW_delimeters.start_of_node_data << node_data;
      }
    }
    os << ';';
  }

  // compute the extended newick string for a network N (only the part below sub_root)
  template<StrictPhylogenyType Network_, class... Args>
  std::string get_extended_newick(const Network_& N, const NodeDesc sub_root, Args&&... args) {
    std::stringstream os;
    write_extended_newick_below(os, N, sub_root, std::forward<Args>(args)...);
    os << ';';
    return os.str();
  }

  // compute the extended newick string for a network N
  template<StrictPhylogenyType Network_>
  std::string get_extended_newick(const Network_& N) { return get_extended_newick(N, N.root()); }


  // ------ READ INPUT --------
  //! a newick parser
  //NOTE: we parse newick from the back to the front since the node names are _appended_ to the node instead of _prepended_
  //NOTE: node numbers will be consecutive (0 = root) and follow a pre-order numbering of a spanning-tree
  //      this allows you to use RONetworks and anything needing pre-order numbers
  //NOTE: output is done via a functor with 'void operator()(NodeDesc, NodeDesc, std::string&&)' - edge data has to be parsed from the 3rd argument
  template<StrictEdgeEmplacerType Emplacer>
  class NewickParser {
    // a HybridInfo is a name of a hybrid together with it's hybrid-index
    using HybridIndex = uint32_t;
    using HybridInfo = std::pair<std::string_view, HybridIndex>;

    std::string newick_string;

    // map a hybrid-index to a node index (and in-degree) so that we can find the corresponding hybrid when reading a hybrid number
    std::unordered_map<HybridIndex, NodeDesc> hybrids;

    Emplacer emplacer;

    static constexpr int32_t parsing_done = -1;
    // pointer to the current read-position in the newick string
    intptr_t back = parsing_done;

  public:

    template<class... Args>
    NewickParser(std::istream& _newick_stream, Args&&... args):
      emplacer(std::forward<Args>(args)...)
    {
      std::getline(_newick_stream, newick_string);
      back = newick_string.length() - 1;
    }
    
    bool is_tree() const { return hybrids.empty(); }
    NodeSingleton parse() { return read_tree(); }
    
    // a tree is a branch followed by a semicolon
    NodeDesc read_tree() {
      NodeDesc root = NoNode;
      skip_whitespaces();
      if(back >= 0) {
        if(newick_string.at(back) == ';') {
          --back;
        } else throw mstd::MalformedInput(newick_string, back, "expected ';' but got \"" + newick_string.substr(back) + "\"\n");
        DEBUG5(std::cout << "parsing \"" << newick_string << "\""<<std::endl);
        root = read_subtree().first;
      }
      back = parsing_done;
      DEBUG3(std::cout << "done parsing, root is "<<root<<"\n");
      return root;
    }

  private:

    void skip_whitespaces() { while((back >= 0) and std::isspace(newick_string.at(back))) --back; }

    // check if this is a hybrid and return name and hybrid number
    uint32_t get_hybrid_num(std::string_view s) {
      if(not s.empty()) {
        size_t first_unconverted;
        if(s[0] == 'H') s.remove_prefix(1);
        uint32_t hn = std::stoX<uint32_t>(s, first_unconverted);
        DEBUG5(std::cout << "converted '"<<s<<"' into "<<hn<< " with first unconverted char at "<<first_unconverted<<'\n');
        if(first_unconverted != s.size())
          throw mstd::MalformedInput(newick_string, back, "found '#' but no hybrid number: \"" + s + "\"\n");
        return hn;
      } else return UINT_MAX;
    }

    using DataArrays = std::array<std::optional<std::string_view>, 4>; //  label, hybrid_num, edge_data, node_data
    enum { DA_Label = 0, DA_hyb_num = 1, DA_edge_data = 2, DA_node_data = 2 };

    // a subtree is a leaf or an internal vertex
    // return the created node as well as the data-strings for it
    auto  read_subtree() {
      std::pair<NodeDesc, DataArrays> result;
      auto& [root, data] = result;
      std::string_view root_data = read_annotation();

      DEBUG5(std::cout << "splitting root_data '"<<root_data<<"'\n");
      // split node-label from hybrid-specifier
      int i = 0;
      if(mstd::split_prefix_at_next(root_data, data[i], config::NW_start_of_hybrid_spec)) i = 1;
      if(mstd::split_prefix_at_next(root_data, data[i], config::NW_delimeters.start_of_edge_data)) i = 2;
      if(mstd::split_prefix_at_next(root_data, data[i], config::NW_delimeters.start_of_node_data)) {
        data[3] = root_data;
      } else data[i] = root_data;

      DEBUG4(std::cout << "split data into label:'"<<data[0].value_or("")<<"' hybrid_num:'"<<data[1].value_or("")<<"' edge_data:'"<<data[2].value_or("")<<"' node_data:'"<<data[3].value_or("")<<"'\n");
      if(data[1].has_value()) {
        assert(!data[1]->empty());
        // if root is a hybrid, register it
        const auto [iter, success] = hybrids.try_emplace(get_hybrid_num(data[1].value()), NoNode); // ,0
        auto& stored = iter->second;
        if(not success) {
          // we've already seen a hybrid with this index - so replace 'root' by the other node's index
          // increase the registered in-degree of 'root'
//          if (not options.allow_non_binary)
//            if(++stored.get_degree() == 3)
//              throw mstd::MalformedInput(newick_string, back, "found non-binary node, which has been explicitly disallowed");
          root = stored;
        } else if(data[3].has_value()) { // if root was an unknown hybrid, then register it
          root = stored = emplacer.create_node(data[3].value());
        } else root = stored = emplacer.create_node();
        
        // allow giving the hybrid a label at any time it is referenced
        if(data[0].has_value())
          emplacer.set_label(root, data[0].value());
       
        // if the subtree dangling from root is non-empty, then recurse
        if((back > 0) && newick_string.at(back) == ')') read_internal<true>(root);
      } else {
        // if root is not a hybrid, then just register it
        DEBUG5(std::cout << " it's not a hybrid, so create it with data '"<<data[3].value_or("")<<"'\n");
        if(data[3].has_value()) {
          root = emplacer.create_node(data[3].value());
        } else root = emplacer.create_node();
        if(data[0].has_value()) emplacer.set_label(root, data[0].value());
        if((back > 0) && newick_string.at(back) == ')') read_internal<false>(root);
      }
      return result;
    }

    // an internal vertex is ( + branchlist + )
    template<bool root_is_hybrid>
    void read_internal(const NodeDesc& root) {
      assert(back >= 2);
      
      if(newick_string.at(back) == ')') --back;
      else throw mstd::MalformedInput(newick_string, back, std::string_view("expected ')' but got '") + newick_string.at(back) + "'");

      read_branchset(root, root_is_hybrid);
      
      if(newick_string.at(back) == '(') --back;
      else throw mstd::MalformedInput(newick_string, back, std::string_view("expected '(' but got '") + newick_string.at(back) + "'");
    }

    // a branchset is a comma-separated list of branches
    // NOTE: alot of sanity checks have gone into the emplacer, since they really make more sense in there...
    void read_branchset(const NodeDesc root, const bool root_is_hybrid = false) {
//      std::unordered_multiset<NodeDesc> children_seen;
//      children_seen.insert(read_branch(root).first);
      read_branch(root);
      while(newick_string.at(back) == ',') {
//        if (root_is_hybrid){
//          if (not options.allow_non_binary)
//            throw mstd::MalformedInput(newick_string, back, "found non-binary node, which is disallowed (can be allowed via options)");
//          if (not options.allow_junctions)
//            throw mstd::MalformedInput(newick_string, back, "found reticulation with multiple children ('junction') which is disallowed (can be allowed via options)");
//        }
        --back;
//        const auto [new_child, nc_data] = 
        read_branch(root);
        if(back < 0) throw mstd::MalformedInput(newick_string, back, "unmatched ')'");
        
//        children_seen.insert(new_child);
//        if((not options.allow_parallel_edges) && (children_seen.count(new_child) > 1))
//          throw mstd::MalformedInput(newick_string, back, "read double edge "+ std::to_string(root) + " --> " + std::to_string(new_child) +
//                  " (hybrid number: " + std::string(nc_data[DA_hyb_num].value()) + ")");
      }
//      if (not options.allow_non_binary)
//        if(children_seen.size() >= 3)
//          throw mstd::MalformedInput(newick_string, back, "found non-binary node, which is disallowed (can be allowed via options)");
    }

    // a branch is a subtree + a length
    // return the head of the read branch
    void read_branch(const NodeDesc root) {
      const auto result = read_subtree();
      const auto& [child, data_arrs] = result;

      if(data_arrs[2].has_value()) {
        emplacer.emplace_edge_raw(root, child, data_arrs[2].value());
      } else emplacer.emplace_edge_raw(root, child);
      
      //return result;
    }

    // read all annotations (label, hybrid, edge-data, node-data) as string_view
    auto read_annotation() {
      std::string_view result;
      const size_t sep = newick_string.find_last_of(",()", back);
      if(sep != std::string::npos) {
        result = std::string_view{newick_string}.substr(sep + 1, back - sep);
        back = sep;
      }
      return result;
    }

  };

  template<EdgeEmplacerType Emplacer>
  NewickParser(std::istream&, Emplacer&&) -> NewickParser<std::remove_cvref_t<Emplacer>>;

  template<class Network, class... Args>
  Network parse_newick(Args&&... args) {
    return parse_network<Network, NewickParser>(std::forward<Args>(args)...);
  }

}


