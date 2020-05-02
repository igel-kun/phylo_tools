

#pragma once

namespace PT {
  // return whether a single-labeled host-tree contains a single-labeled guest tree
  template<class Host, class Guest,
    class = std::enable_if_t<Host::is_single_labeled && Guest::is_single_labeled>>
  bool tree_in_tree(const Guest& guest, const Host& host)
  {

  }
}
