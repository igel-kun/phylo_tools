
#pragma once

#include "splay_tree.hpp"

namespace mstd {

  // ========== Link-Cut Tree ==========
  // this is a data structure proposed by Sleator & Tarjan in 1985 to represent a dynamic forest;
  // it supports the following operations:
  // O(1): add_tree() - add a new tree to the forest consisting of an isolated node (which is the root and a leaf)
  // O(log n): link(T, x) - hang the tree T below x (the root of T becomes a new child of x)
  // O(log n): cut(x) - cut the edge above x, making x the root of a new tree in the forest
  // O(log n): root(x) - return the root of the tree containing x
  // O(log n): LCA(x, y) - return the LCA of x and y (if any)
  //
  // The data structure is based on representing 'preferred paths' as splay_trees internally
  // To be useful, we add a key to all nodes of the splay trees and keep a hashmap of keys to nodes around

  // ------- Link-Cut Tree: helpers ---------
  
  // ------- Link-Cut Tree: main class ---------
  template<class Key, class Payload_ = void>
  struct LinkCutTree {
    // ------- static stuff --------
    static constexpr bool has_payload = not std::is_void_v<Payload_>;
    
    // the tree is partitioned into preferred paths, each of which is represented by a splay tree in the background

    // NOTE: the topmost node of the path always has an empty 'left' pointer, so we use the 'left' pointer of the topmost node
    //        of the preferred path represented by the splay tree to point to the parent node in the parent preferred path
    //        this *should* not mess with the splay-tree data structure...
    // NOTE: just to reiterate: tag == 1 means the node is the topmost in its preferred path
    using Node = STNode<Key, Payload_, 1>;

    // the LCT owns all the splay-tree nodes and they'll be destroyed when the LCT is
    using KeyToNode = std::unordered_map<Key, std::unique_ptr<Node>>;

    // ------- members --------
    KeyToNode key_to_node;

    // ------- construction & desctruction ---------
    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: modification --------
    
    // add an isolated node to the tree; construct the node using the arguments
    template<class... Args>
    auto emplace_tree(const Key& key, Args&&... args) {
      auto node_ptr = std::make_unique<Node>(key, std::forward<Args>(args)...);
      const auto [iter, success] = key_to_node.try_emplace(key, std::move(node_ptr));
      Node* const new_x = iter->second.get();
      new_x->left.set_tag(1);
      return std::pair{new_x, success};
    }
    
    // emplace a new leaf below a node x
    template<class... Args>
    auto emplace_leaf(Node* const x_parent, const Key& x, Args&&... args) {
      auto result = emplace_tree(x, std::forward<Args>(args)...);
      link(x_parent, result.first);
      return result;
    }
    template<class... Args>
    auto emplace_leaf(const Key& x_parent, const Key& x, Args&&... args) {
      return emplace_leaf(key_to_node.at(x_parent).get(), x, std::forward<Args>(args)...);
    }
   
    // add old_root as a child to x in the represented tree
    // NOTE: this will not add old_root to the preferred path of x
    static void link(Node* const x, Node* const old_root) {
      assert(is_tree_root(old_root));
      // link the lower to the upper
      old_root->left = x;
      assert(get_parent_path_parent(old_root) == x);
    }
    void link(const Key& x, Node* const old_root) const { link(key_to_node.at(x).get(), old_root); }
    void link(const Key& x, const Key& old_root) const { link(key_to_node.at(x).get(), key_to_node.at(old_root).get()); }

    // cut the edge above x in the represented tree; x will be the root of the newly created tree
    static void cut(Node* const x) {
      assert(not is_tree_root(x));
      split_preferred_path_at(x, true);
    }
    void cut(const Key& x) const { cut(key_to_node.at(x)); }

    // a central task for LinkCutTrees is to expose a node, that is,
    // extend its preferred path to the root of the represented tree
    // return the last top-parent or x if the path of x already contains the root
    // NOTE: if you expose x and y in a row, then the returned node is the LCA of x and y
    static Node* expose(Node* const x) {
      DEBUG5(std::cout << "exposing "<<x<<'\n');
      Node* const top = get_topmost(x);
      DEBUG5(std::cout << "got topmost node of the path: "<<top<<'\n');
      Node* const top_parent = get_parent_path_parent(top);
      DEBUG5(std::cout << "top's parent is "<<top_parent<<'\n');
      if(top_parent != nullptr) {
        top_parent->splay();
        DEBUG5(std::cout << "after splaying "<<top_parent<<":\n"; print_splay_tree_of(std::cout, top_parent) << '\n');
        // after splaying the top_parent, it's at the root of its splay tree, so we can cut-off the right subtree
        // (which represents the part of the preferred path that is below top-parent)
        Node* z = top_parent->right;
        if(z != nullptr) {
          // sever z from top_parent
          top_parent->right = nullptr;
          z->parent = nullptr;
          assert(z->left.get_tag() == 0);
          // find the successor of top_parent on its preferred path; that one will be the topmost node of the new path
          while(z->left != nullptr) z = z->left;
          assert(z->left.get_tag() == 0);
          z->left.set(top_parent, 1);
          DEBUG5(std::cout << "after severing "<<z<<":\n"; print_splay_tree_of(std::cout, z) << '\n');
        }
        // now that top_parent is the end of its path, we can merge it with x's path (with topmost node 'top')
        top->splay();
        assert(top->parent == nullptr);
        assert(top_parent->right == nullptr);
        top->left.set(nullptr, 0);
        top->parent = top_parent;
        top_parent->right = top;
        return expose(top_parent);
      } else return x;
    }

  protected:
    // split the preferred path of x above x, creating a new preferred path whose topmost node is x
    // if cut_edge is set, then also cut the edge to the parent in the represented tree
    static void split_preferred_path_at(Node* const x, const bool cut_edge = false) {
      if(not is_topmost(x)) {
        Node* real_parent = get_represented_parent(x);
        // since we splayed x to get the parent, splaying the parent now results in it being the root of the splay tree and x its right child
        real_parent.splay();
        assert(real_parent->right == x);
        assert(x->left == nullptr);
        assert(x->left.get_tag() == 0);
        // now, sever x from the splay tree to create its own splay tree
        real_parent->right = nullptr;
        x->parent = nullptr;
        // if we're not cutting the edge, then we need to link back to the parent via x->left
        x->left.set(cut_edge ? nullptr : real_parent, 1);
      } else if(cut_edge) x->left = nullptr; // x is already topmost in its preferred path
    }

    // ------- methods: query --------
  public:
    // return whether x is the highest node in its preferred path
    // NOTE: since we're using 'left' of the highest node to store a pointer to the parent in the parent path,
    //        we'll need to check the bool tagged onto 'left'
    static bool is_topmost(Node* const x) { assert(x != nullptr); return x->left.get_tag(); }

    // return the highest node in the preferred path of x
    // NOTE: to do this, first splay(x), then look all the way to the left
    static Node* get_topmost(Node* x) {
      assert(x != nullptr);
      x->splay();
      // walk to the left until we find the topmost node
      while(not is_topmost(x)) x = x->left;
      return x;
    }

    // return whether x is the root of a tree in the forest
    static bool is_tree_root(Node* const x) { assert(x != nullptr); return get_parent_path_parent(x) == nullptr; }

    // return the root of the tree of x
    static Node* get_tree_root(Node* const x) { expose(x); return get_topmost(x); }

    // return the child of x in its preferred path
    static Node* get_successor_in_path(Node* x) {
      assert(x != nullptr);
      x->splay();
      x = x->right;
      if(x == nullptr) return nullptr;
      while(x->left != nullptr) x = x->left;
      return x;
    }
    // return the parent of x in its preferred path
    static Node* get_predecessor_in_path(Node* x) {
      assert(x != nullptr);
      x->splay();
      x = x->left;
      if(x == nullptr) return nullptr;
      while(x->left != nullptr) x = x->right;
      return x;
    }

    // return the parent of x in its represented tree
    static Node* get_represented_parent(Node* x) {
      if(not is_topmost(x)) {
        return get_predecessor_in_path(x);
      } else return get_parent_path_parent(x);
    }

    // return the parent in the parent path of a topmost node in a splay tree
    static Node* get_parent_path_parent(Node* const x) {
      assert(is_topmost(x));
      return x->left;
    }

    static Node* LCA(Node* const x, Node* const y) {
      DEBUG4(std::cout << "computing LCA of "<<x->key<<" and "<<y->key<<" by double expose\n");
      expose(x);
      DEBUG5(std::cout << "splay tree of "<<x<<" after expose:\n"; print_splay_tree_of(std::cout, x););
      Node* const result = expose(y);
      DEBUG5(std::cout << "splay tree of "<<y<<" after 2nd expose:\n"; print_splay_tree_of(std::cout, y););
      return result;
    }
    /*
    const Key& LCA(const Key& x, const Key& y) const {
      expose(key_to_node.at(x));
      return expose(key_to_node.at(y))->key;
    }
    */
  };

  template<class Key, class Payload_>
  std::ostream& print_link_cut_tree(std::ostream& os, const LinkCutTree<Key, Payload_>& tree) {
    os << "preferred paths in the splay tree:\n";
    for(const auto& [key, node]: tree.key_to_node) {
      assert(node != nullptr);
      if(node->is_root()) {
        print_splay_tree_rooted_at(os, node.get());
        os << '\n';
      }
    }
    return os;
  }

  // ------- Link-Cut Tree: factories ---------
  
  // ------- Link-Cut Tree: concepts ---------
  
  // ------- Link-Cut Tree: deduction guides ---------
  
  // ------- Link-Cut Tree: defaults ---------

}
