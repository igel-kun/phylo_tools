
#pragma once

#include "tagged_pointer.hpp"

namespace mstd {

  // ========== SplayTree ==========
  // a binary tree that supports the 'splay' operation
  // splay(v) brings a node v to the root of the tree, keeping the inorder-numbering in tact
  // keeping the inorder-numbering in tact means that the tree has the heap-property after splaying iff it had the heap-property before splaying
  // the heap-property is that the payloads enumerated in in-order are sorted
  //
  // SplayTrees have been invented by Sleator & Tarjan, 1985

  // ------- SplayTree: helpers ---------
  // NOTE: we'll allow tagging the left pointer, to work well with LCTrees (that need 1 bit per node)
  template<class Key, class _Payload = void, uint8_t _tag_size = 0>
  struct STNode {
    // ------- static stuff --------
    static constexpr uint8_t tag_size = _tag_size;
    static constexpr bool has_payload = not std::is_void_v<_Payload>;
    using Payload = std::conditional_t<has_payload, _Payload, mstd::monostate>;
    // NOTE: we'll have to compute a lower bound on the alignment of STNode since the compiler cannot figure out the alignment of incomplete types
    using STPointer = std::conditional_t<tag_size == 0, STNode*, mstd::TaggedPtr<STNode*, tag_size, 3*sizeof(uintptr_t)+sizeof(Key)>>;
    
    // ------- members --------
    [[ no_unique_address ]] Payload payload;
    Key key;
    STPointer left = nullptr;
    STNode* right = nullptr;
    STNode* parent = nullptr;

    // ------- construction & desctruction ---------
    STNode() = default;
    
    template<class First, class... Args> requires (not has_payload and not mstd::is_any_of<First, STNode> and (sizeof...(Args) != 0))
    STNode(First&& first, Args&&... args):
      key(std::forward<First>(first), std::forward<Args>(args)...)
    {}
 
    template<class KeyInit, class... Args> requires (mstd::is_same_v<KeyInit, Key>)
    STNode(KeyInit&& _key, Args&&... args):
      payload(std::forward<Args>(args)...),
      key(std::forward<KeyInit>(_key))
    {}
   
    template<mstd::TupleType KeyTuple, mstd::TupleType PayloadTuple>
    STNode(std::piecewise_construct_t, KeyTuple&& _key, PayloadTuple&& _payload):
      payload(std::make_from_tuple<Payload>(std::forward<PayloadTuple>(_payload))),
      key(std::make_from_tuple<Key>(std::forward<KeyTuple>(_key)))
    {}


    // ------- operators --------
    bool operator==(const STNode& y) const { return key == y.key; }

    // ------- methods: initialization --------
    // ------- methods: modification --------
  protected:
    void replace_left(STNode* const new_child) {
      if(new_child != nullptr) new_child->parent = this;
      left = new_child;
    }
    void replace_right(STNode* const new_child) {
      if(new_child != nullptr) new_child->parent = this;
      right = new_child;
    }
    
    void replace_child(STNode* const old_node, STNode* const new_node) {
      assert((left == old_node) or (right == old_node));
      if(left == old_node) {
        left = new_node;
      } else right = new_node;
      if(new_node != nullptr) new_node->parent = this;
    }

  public:
    // rotate this to the left, so that right becomes our parent
    STNode* left_rotate() {
      assert(right != nullptr);
      STNode* const y = right;

      if(parent != nullptr) {
        parent->replace_child(this, y);
      } else y->parent = nullptr;
      replace_right(y->left);
      y->replace_left(this);
      return y;
    }

    // rotate this to the right, so that left becomes our parent
    STNode* right_rotate() {
      assert(left != nullptr);
      STNode* const y = left;

      if(parent != nullptr) {
        parent->replace_child(this, y);
      } else y->parent = nullptr;
      replace_left(y->right);
      y->replace_right(this);
      return y;
    }

    // rotate the parent of this, so that this becomes the new parent
    STNode* rotate_upwards() {
      assert(parent != nullptr);
      if(is_left_child()) {
        return parent->right_rotate();
      } else return parent->left_rotate();
    }

    // splay brings the node to the root of its splay-tree via rotations
    // NOTE: wiki says: if both x and its parent are the same type of children (left/right), then rotate the parent first, then x
    //                  if they are different types of children, then rotate x twice
    void splay() {
      while(parent != nullptr) {
        if(parent->parent != nullptr) {
          if(parent->is_left_child() == is_left_child()) {
            parent->rotate_upwards();
          } else rotate_upwards();
        }
        rotate_upwards();
      }
    }

    // ------- methods: query --------
  public:
    const Key& get_key() const { return key; }
    const Key& get_payload() const { return payload; }
    bool is_root() const { return parent == nullptr; }
    bool is_left_child() const {
      assert(parent != nullptr);
      return (parent->left == this);
    }
  };

  template<class Key, class _Payload, uint8_t _tag_size>
  std::ostream& operator<<(std::ostream& os, STNode<Key, _Payload, _tag_size>* const x) {
    if(x != nullptr) {
      os << static_cast<void*>(x) << " ("<<x->key<<") {P: ";
      if(x->parent != nullptr)
        os << static_cast<void*>(x->parent) << " ("<<x->parent->key<<"),";
      else os << "NULL, ";
      os << "L: ";
      if(x->left != nullptr)
        os << static_cast<void*>(x->left.get_pointer()) << "[tag "<<int{x->left.get_tag()}<<"] ("<<x->left->key<<"), ";
      else os << "NULL, ";
      os << "R: ";
      if(x->right != nullptr)
        os << static_cast<void*>(x->right) << " ("<<x->right->key<<")}";
      else os << "NULL}";
    } else os << "{NULL}";
    return os;
  }

  template<class Key, class _Payload, uint8_t _tag_size>
  std::ostream& print_splay_tree_rooted_at(std::ostream& os, STNode<Key, _Payload, _tag_size>* const root, const size_t indent = 0) {
    os << std::string(indent, ' ');
    if(root != nullptr) {
      os << root << '\n';
      if(root->left.get_tag() == 1) {
        os << std::string(indent + 2, ' ') << "(upwards to " << root->left.get_pointer() << ")\n";
      } else print_splay_tree_rooted_at(os, root->left.get_pointer(), indent + 2);
      print_splay_tree_rooted_at(os, root->right, indent + 2);
      return os;
    } else return os << "(NULL)\n";
  }
 
  template<class Key, class _Payload, uint8_t _tag_size>
  std::ostream& print_splay_tree_of(std::ostream& os, STNode<Key, _Payload, _tag_size>* root) {
    if(root != nullptr) {
      while(not root->is_root()) root = root->parent;
      return print_splay_tree_rooted_at(os, root);
    } else return os << "(NULL)\n";
  }
 
/*
  // ------- SplayTree: main class ---------
  template<class Key
  struct splaytree {
    // ------- static stuff --------
    // ------- members --------
    // ------- construction & desctruction ---------
    splaytree() {}
    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: modification --------
    // ------- methods: query --------
  };
  */
  // ------- SplayTree: factories ---------
  
  // ------- SplayTree: concepts ---------
  
  // ------- SplayTree: deduction guides ---------
  
  // ------- SplayTree: defaults ---------

}
