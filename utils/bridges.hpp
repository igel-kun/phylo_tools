
#pragma once

#include "types.hpp"
#include "set_interface.hpp"

// this is my own bridge finder
// NOTE: most bridge finders out there work only on undirected graphs, so I modified Tarjan's ideas slightly...
// NOTE: we'll find so-called "vertical cut nodes", that is, nodes u that have a child v such that all descendants of v only have neighbors below u
// NOTE: to this end, each node v communicates the smallest and highest preorder number of any of its neighbors to its parent u in the preorder spanning tree

namespace PT{

  enum CutObject { CO_vertical_cut_node = 1, CO_bridge = 2, CO_vcn_outedge = 3 };

  struct CutNodeInfo {
    uint32_t DFS_descendants; // number of non-strict (!) descendants in the DFS subtree (always >= 1)
    uint32_t disc_time;
    std::linear_interval<> neighbors; // smallest and highest preprder number of any neighbor of any non-strict descendant
    bool up_to_date = false; // TODO: find a better way to track bottom-up updates!

    CutNodeInfo(const uint32_t _disc_time):
      DFS_descendants(1), disc_time(_disc_time), neighbors(_disc_time)
    {
      std::cout << "making new info entry: " << *this << "\n";
    }
 
    // the DFS interval of v is [disc_time : disc_time + DFS_descendants]
    uint32_t first_outside_subtree() const { return disc_time + DFS_descendants; }
    void update_lowest_neighbor(const uint32_t u) { neighbors.update_lo(u); }
    void update_highest_neighbor(const uint32_t u) { neighbors.update_hi(u); }
    void update_neighbors(const uint32_t x) { neighbors.update(x); }
    void update(const CutNodeInfo& other) { neighbors.merge(other.neighbors); }

    // return true if the given CutNodeInfo has a descendant that "sees" someone outside our DFS-subtree
    bool child_has_outside_neighbor(const CutNodeInfo& child) const {
      return (child.neighbors.lo < disc_time) || (child.neighbors.hi >= first_outside_subtree());
    }
    // return true if we ourselves have a descendant that "sees" someone outside our DFS-subtree
    bool has_outside_neighbor() const {
      std::cout << "infos: "<<*this << "\n";
      return child_has_outside_neighbor(*this);
    }

    friend std::ostream& operator<<(std::ostream& os, const CutNodeInfo& b) {
      return os << "(disc: "<<b.disc_time<<" desc: "<<b.DFS_descendants<<" NH: "<<b.neighbors<<')';
    }
  };

  struct CutNodeInfoWithMarks: public CutNodeInfo {
    using CutNodeInfo::CutNodeInfo;
    bool cut_node_mark = false; // TODO: find another way to indicate marked state so as to save these 4 bytes!
  };



  // this is a base for iterators listing vertical cut-nodes, bridges or out-edges of vertical cut nodes (used to compute "biconnected" components)
  template<PhylogenyType Network, CutObject cut_object>
  class _CutNodeIter {
    using InfoStruct = std::conditional_t<cut_object == CO_vertical_cut_node, CutNodeInfoWithMarks, CutNodeInfo>;
    using traits = std::iter_traits_from_reference<NodeDesc>;
    using InfoMap = NodeMap<InfoStruct>;
    using POTraversal = EdgeTraversal<postorder, Network>;
    using DFSIter = typename POTraversal::OwningIter;

  protected:
    NodeDesc root;
    InfoMap node_infos;
    DFSIter iter;

    // the first DFS sets up preorder numbers and numbers of descendants in the node_infos
    // NOTE: we use node_infos to indicate which nodes we have already visited
    std::emplace_result<InfoMap> setup_DFS(const NodeDesc& u, uint32_t& time) {
      const auto emp_res = node_infos.emplace(u, time);
      if(emp_res.second){
        ++time;
        auto& u_info = emp_res.first->second;
        for(const NodeDesc& v: node_of<Network>(u).children()) {
          const auto [v_iter, success] = setup_DFS(v, time);
          auto& v_info = v_iter->second;
          std::cout << "first DFS for edge "<<u<<" "<<u_info<<" -----> "<<v<<" "<<v_info<<"\n";
          if(!success) {
            u_info.update_lowest_neighbor(v_info.disc_time);
            v_info.update_neighbors(u_info.disc_time);
            std::cout << "updated infos: "<<u_info<<"\t\t"<<v_info<<"\n";
          } else u_info.DFS_descendants += v_info.DFS_descendants;
        }
      }
      return emp_res;
    }

    // update infos for the current iterator and return whether the head of the edge pointed to by said iterator is a vertical cut node
    // return true if we want to yield iter->second as next cut node
    bool update_upwards() {
      if(!iter.is_valid()) {
        if constexpr (cut_object == CO_vertical_cut_node) {
          // we just arrived at the end of the DFS; in this case, yield the root if it's a cut-node
          if(!node_infos.at(root).cut_node_mark)
            root = NoNode;
        }
        return true;
      } else {
        const auto [u, v] = iter->as_pair();
        auto& u_info = node_infos.at(u);
        auto& v_info = node_infos.at(v);
        // step 1: update v_info with v's children
        if(!v_info.up_to_date){
          for(const NodeDesc& w: node_of<Network>(v).children())
            v_info.update(node_infos.at(w));
          v_info.up_to_date = true;
        }
        std::cout << "updated "<<u_info<<"\n";
        // step 2: if u is a vertical cut node due to v, then mark u
        if constexpr (cut_object == CO_vertical_cut_node) {
          if(!u_info.child_has_outside_neighbor(v_info)) std::cout << "marking "<<u<<" from "<<v<<" -- "<<u_info<<"\t&\t"<<v_info<<"\n";
          u_info.cut_node_mark |= (!u_info.child_has_outside_neighbor(v_info));
          // step 3: if v is marked as vertical cut node, yield it, otherwise continue
          if(v_info.cut_node_mark) {
            v_info.cut_node_mark = false; // NOTE: reset the cut-node flag such that v is only output once!
            return true;
          } else return false;
        } else if constexpr (cut_object == CO_bridge) {
          return !v_info.has_outside_neighbor();
        } else if constexpr (cut_object == CO_vcn_outedge) {
          return !u_info.child_has_outside_neighbor(v_info);
        }
      }
    }

    // the second DFS propagates the lowest and highest neighbors upwards in the DFS tree
    void advance() {
      std::cout << "advancing cut node...\n";
      if(iter.is_valid()) {
        while(true) {
          ++iter;
          if(update_upwards()) return;
        }
      } else root = NoNode; // if the iter was already invalid when we called advance(), then also make the root invalid now
    }

  public:
    _CutNodeIter(const NodeDesc& _root):
      root(_root), iter(POTraversal(_root).begin())
    {
      if(_root != NoNode) {
        uint32_t time = 0;
        setup_DFS(root, time);
      }
    }

    _CutNodeIter& operator++() { advance(); return *this; }
    _CutNodeIter  operator++(int) { _CutNodeIter result = *this; ++(*this); return result; }

    _CutNodeIter(const _CutNodeIter&) = default;
    _CutNodeIter(_CutNodeIter&&) = default;
    _CutNodeIter& operator=(const _CutNodeIter&) = default;
    _CutNodeIter& operator=(_CutNodeIter&&) = default;

    bool operator==(const _CutNodeIter& other) const { return iter == other.iter; }
    template<class T> bool operator!=(const T& other) const { return !operator==(other); }

    typename DFSIter::reference operator*() const { return *iter; }
    typename DFSIter::pointer operator->() const { return operator*(); }

    bool operator==(const std::GenericEndIterator&) const { return !is_valid(); }
    bool is_valid() const { return iter.is_valid(); }
  };


  template<PhylogenyType Network>
  class CutNodeIter: public _CutNodeIter<Network, CO_vertical_cut_node>, public std::iter_traits_from_reference<NodeDesc> {
  protected:
    using Parent = _CutNodeIter<Network, CO_vertical_cut_node>;
    using Traits = std::iter_traits_from_reference<NodeDesc>;
  public:
    using Parent::iter;
    using typename Traits::reference;
    using typename Traits::pointer;

    CutNodeIter(const NodeDesc& _root): Parent(_root)
    {
      if(_root != NoNode){
        // the first node of iter is a leaf (iter is postorder) which is never a cut-node, so we can savely advance() to the first cut-node
        Parent::update_upwards();
        Parent::advance();
      }
    }

    reference operator*() const { return iter.is_valid() ? iter->as_pair().second : Parent::root; }
    pointer operator->() const { return operator*(); }

    bool operator==(const std::GenericEndIterator&) const { return !is_valid(); }
    bool is_valid() const { return Parent::root != NoNode; }
  };


  template<PhylogenyType Network>
  class BridgeIter: public _CutNodeIter<Network, CO_bridge>, public std::my_iterator_traits<typename EdgeTraversal<postorder, Network>::OwningIter> {
    using Parent = _CutNodeIter<Network, CO_bridge>;
    using Parent::node_infos;
  public:
    using Parent::Parent;
    using Parent::iter;

    BridgeIter& operator++() {
      do {
        Parent::advance();
        // note that *iter is a bridge iff iter->head() is a vertical cut-node with in-degree 1
      } while(iter.is_valid() && (node_infos.at(iter->head()).has_outside_neighbor()));
      return *this;
    }
    BridgeIter  operator++(int) { BridgeIter result = *this; ++(*this); return result; }

  };


  template<PhylogenyType Network>
  class VcnOutedgeIter: public _CutNodeIter<Network, CO_vcn_outedge>, public std::my_iterator_traits<typename EdgeTraversal<postorder, Network>::OwningIter> {
    using Parent = _CutNodeIter<Network, CO_vcn_outedge>;
  public:
    using Parent::Parent;
  };









  template<PhylogenyType Network> using CutNodeIterFactory = std::IterFactory<CutNodeIter<Network>>;
  template<PhylogenyType Network> using BridgeIterFactory = std::IterFactory<BridgeIter<Network>>;
  template<PhylogenyType Network> using VcnOutedgeIterFactory = std::IterFactory<VcnOutedgeIter<Network>>;

  template<PhylogenyType Network> auto get_cut_nodes(const NodeDesc& rt) { return CutNodeIterFactory<Network>(rt); }
  template<PhylogenyType Network> auto get_bridges(const NodeDesc& rt) { return BridgeIterFactory<Network>(rt); }
  template<PhylogenyType Network> auto get_vcn_outedges(const NodeDesc& rt) { return VcnOutedgeIterFactory<Network>(rt); }
  template<PhylogenyType Network> auto get_cut_nodes(const Network& N) { return CutNodeIterFactory<Network>(N.root()); }
  template<PhylogenyType Network> auto get_bridges(const Network& N) { return BridgeIterFactory<Network>(N.root()); }
  template<PhylogenyType Network> auto get_vcn_outedges(const Network& N) { return VcnOutedgeIterFactory<Network>(N.root()); }


}// namespace
