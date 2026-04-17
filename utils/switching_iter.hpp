
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
  template<StrictPhylogenyType Net_, class DefaultParent_ = typename Switching<Net_>::FirstParent>
  struct SwitchingIter:
    public mstd::iter_traits_from_reference<Switching<Net_>>
  {
    // ------- static stuff --------
    using Net = Net_;
    using DefaultParent = DefaultParent_;
    using Traits = mstd::iter_traits_from_reference<Switching<Net>>;
    using AdjVec = NetAdjVec<Net>;
    using typename Traits::value_type;
    using typename Traits::pointer;

    // ------- members --------
  protected:
    [[ no_unique_address ]] DefaultParent parent_select;
    Switching<Net> cache;
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
  };
  
  // ------- SwitchingIter: factories ---------
  template<StrictPhylogenyType Net, class DefaultParent = typename Switching<Net>::FirstParent>
  using SwitchingFactory = mstd::IterFactory<SwitchingIter<Net, DefaultParent>>;
 
  // ------- SwitchingIter: concepts ---------
  
  // ------- SwitchingIter: deduction guides ---------
  template<typename Net>
  SwitchingIter(Net) -> SwitchingIter<Net>; 
 
  // ------- SwitchingIter: defaults ---------

} // namespace
