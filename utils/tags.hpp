
#pragma once

namespace PT {

  struct owning_tag {};
  struct non_owning_tag {};

  // this tag allows creating an edge u-->v from an existing adjacency v-->u
  struct reverse_edge_tag {};

  // indicate that only leaves will have labels, instead of every node having a label
  struct leaf_labels_only_tag {};

  // general no-cleanup tag, indicating that we will do a cleanup later
  struct no_cleanup_tag {};

  // in order to pass only the type of something to a constructor, we will use a type-carrying-tag
  template<class T> struct type_carrier { using type = T; };

  // data transfer policies
  struct data_policy_tag {};
  struct policy_move_tag: public data_policy_tag {};
  struct policy_copy_tag: public data_policy_tag {};
  struct policy_inplace_tag: public data_policy_tag {};
  struct policy_noop_tag: public data_policy_tag {};
  struct policy_move_children_tag: public data_policy_tag {};

  template<class T>
  concept DataPolicyTag = std::derived_from<T, data_policy_tag>;

  // tags for the data extracter
  struct Ex_node_label {};
  struct Ex_node_data {};
  struct Ex_edge_data {};

  template<class T> constexpr bool is_node_label_tag = std::is_same_v<T, Ex_node_label>;
  template<class T> constexpr bool is_node_data_tag = std::is_same_v<T, Ex_node_data>;
  template<class T> constexpr bool is_edge_data_tag = std::is_same_v<T, Ex_edge_data>;
  template<class T> concept DataTag = (is_node_label_tag<T> || is_node_data_tag<T> || is_edge_data_tag<T>);

}
