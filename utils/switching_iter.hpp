
#include "switching.hpp"
#include "iter_factory.hpp"

namespace PT {

  // ========== SwitchingIter ==========
  // an iterator over all switchings of a network

  // ------- SwitchingIter: helpers ---------
  template<SwitchingType S>
  struct NetworkOf_<S> { using type = S::Net; };
 
  // ------- SwitchingIter: main class ---------
  // NOTE: we can pass a set of leaves, in which case only the partial switchings of reticulations above those leaves will be iterated
  template<StrictPhylogenyType Net_, StorageEnum active_parent_storage = hashsetS, class DefaultParent_ = typename Switching<Net_>::FirstParent>
    requires (std::is_invocable_v<DefaultParent_, const NodeDesc>)
  struct SwitchingIter:
    public mstd::iter_traits_from_reference<Switching<Net_, active_parent_storage>>
  {
    // ------- static stuff --------
    using Net = Net_;
    using DefaultParent = DefaultParent_;
    using Traits = mstd::iter_traits_from_reference<Switching<Net, active_parent_storage>>;
    using AdjVec = NetAdjVec<Net>;
    using typename Traits::value_type;
    using typename Traits::pointer;

    // ------- members --------
  protected:
    [[ no_unique_address ]] DefaultParent parent_select;
    Switching<Net, active_parent_storage> cache;
    bool valid = true;

    // ------- construction & desctruction ---------
  public:
    SwitchingIter() = default;

    template<class... Args> requires std::is_constructible_v<DefaultParent, Args&&...>
    SwitchingIter(const Net& N, Args&&... args):
      parent_select(std::forward<Args>(args)...),
      cache(roots_tag{}, N.roots(), parent_select) {}

    template<RootsOrLeavesTag Tag, NodeOrContainerType Nodes, class... Args> requires std::is_constructible_v<DefaultParent, Args&&...>
    SwitchingIter(const Tag t, const Nodes& X, Args&&... args):
      parent_select(std::forward<Args>(args)...),
      cache(t, X, parent_select) {}

    // ------- operators --------
    auto& operator*() const { return cache; }
    auto operator->() const { return &cache; }

    SwitchingIter& operator++() {
      DEBUG4(std::cout << "advancing switching iter, current active parents: ";
        for(auto& [v, vp]: cache.active_parent) std::cout << "("<<v<<", "<<*vp<<")\n");

      for(auto& [v, vp]: cache.active_parent) {
        if(++vp != Net::parents(v).end()) {
          return *this;
        } else vp = parent_select(v);
      }
      DEBUG5(std::cout << "all switchings considered, rendering iter invalid...\n");
      valid = false;
      assert(not is_valid());
      return *this;
    }

    SwitchingIter operator++(int) { SwitchingIter old{*this}; ++(*this); return old; }

    bool operator==(const SwitchingIter& other) const {
      if(is_valid()) {
        if(other.is_valid()) {
          return cache == other.cache;
        } else return false;
      } else return not other.is_valid();
    }

    // ------- methods: initialization --------
    // ------- methods: query --------
    bool is_invalid() const { return not is_valid(); }
    bool is_valid() const { return valid; }

    const auto& get_active_parents() const { return cache.active_parent; }
    const auto& get_switching() const { return cache; }

    // ------- methods: modification --------
    // advance to the next switching
    // calling this repeatedly iterates more efficiently through the switchings above a set of leaves
    // return whether the iterator is still valid
    // NOTE: if the switching is empty, then we scan the network above 'nodes'
    template<NodeOrIterableType Nodes, class... Args> requires (active_parent_storage == vecS)
    bool advance_above(const Nodes& nodes, const bool mark_invalid_if_empty, Args&&... args) {
      static_assert(mstd::VectorType<decltype(cache.active_parent)>);
      // step 1: advance the switching
      if(valid) {
        DEBUG5(std::cout << "advancing switching "<<cache.active_parent<<" over nodes "<<nodes<<'\n');
        if(not cache.active_parent.empty()) {
          while(true) {
            auto& [v, vp] = cache.active_parent.back();
            if(++vp == Net::parents(v).end()) {
              DEBUG5(std::cout << "increased active parent of "<<v<<" beyond bounds, erasing...\n");
              cache.active_parent.pop_back();
              if(cache.active_parent.empty()) {
                // if we went over all switchings, mark the iterator as invalid
                valid = false;
                return false;
              }
            } else {
              DEBUG5(std::cout << "changed active parent of "<<v<<" to "<<*vp<<'\n');
              break;
            }
          } // while true
        } // if cache is not empty
        // step 2: discover new reticulations
        cache.discover_above(nodes, std::forward<Args>(args)...);
        if(mark_invalid_if_empty and cache.active_parent.empty()) valid = false;
      }
      return valid;
    }
    template<NodeOrIterableType Nodes, class... Args> requires (active_parent_storage == vecS)
    bool advance_above(const Nodes& nodes, Args&&... args) { return advance_above(nodes, false, std::forward<Args>(args)...); }

  };
  
  // ------- SwitchingIter: factories ---------
  template<StrictPhylogenyType Net, StorageEnum active_parent_storage = hashsetS, class DefaultParent = typename Switching<Net>::FirstParent>
    requires (std::is_invocable_v<DefaultParent, const NodeDesc>)
  using SwitchingFactory = mstd::IterFactory<SwitchingIter<Net, active_parent_storage, DefaultParent>>;
 
  // ------- SwitchingIter: concepts ---------
  
  // ------- SwitchingIter: deduction guides ---------
  template<typename Net>
  SwitchingIter(Net) -> SwitchingIter<Net>; 
 
  // ------- SwitchingIter: defaults ---------

} // namespace
