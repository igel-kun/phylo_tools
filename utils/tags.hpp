
#pragma once

namespace PT {
  // indicate that only leaves will have labels, instead of every node having a label
  struct leaf_labels_only_tag {};

  // data transfer policies
  struct data_policy_t {};
  struct policy_move_t: public data_policy_t {};
  struct policy_copy_t: public data_policy_t {};
  struct policy_inplace_t: public data_policy_t {};
  struct policy_noop_t: public data_policy_t {};

  template<class T>
  concept DataPolicyTag = std::derived_from<T, data_policy_t>;



  // tags for the data extracter
  struct Ex_node_label {};
  struct Ex_node_data {};
  struct Ex_edge_data {};

  template<class T> constexpr bool is_node_label_tag = std::is_same_v<T, Ex_node_label>;
  template<class T> constexpr bool is_node_data_tag = std::is_same_v<T, Ex_node_data>;
  template<class T> constexpr bool is_edge_data_tag = std::is_same_v<T, Ex_edge_data>;
  template<class T> concept DataTag = (is_node_label_tag<T> || is_node_data_tag<T> || is_edge_data_tag<T>);

}
