
#pragma once

#include "feature_collection.hpp"

namespace PT {

  // ========== FeatureCount ==========
  // compute the total number of features in a FeatureCollection
  // To this end, we just sum up the size() of each feature-list in the collection

  // ------- FeatureCount: helpers ---------
  
  // ------- FeatureCount: main class ---------
  // in a feature accumulator, we can collect all features seen in a set of nodes
  template<class T> struct FeatureAccu_ { using type = HashSet<T>; };
  template<> struct FeatureAccu_<bool> { using type = bool; /* did we see the features?*/ };
  template<class T> using FeatureAccu = typename FeatureAccu_<std::remove_cvref_t<T>>::type;

  // a feature counter is a Features-class, but with feature-accumulators instead of features
  template<class... Feats>
  struct FeatureCount:
    public Features<FeatureList, FeatureAccu<Feats>...>
  {
    using Parent = Features<FeatureList, FeatureAccu<Feats>...>;

    template<class First, class... Others> requires (mstd::is_any_of<First, Feats...>)
    size_t _count_features() const {
      static constexpr size_t count_index = mstd::var_type_index<First, Feats...>();
      if constexpr (sizeof...(Others) > 0)
        return _count_features<Others...>() + Parent::template get<count_index>().size();
      else return Parent::template get<count_index>().size();
    }

    // return the amount of features present in the accumulators
    size_t count_features() const { return _count_features<Feats...>(); }

    auto& operator+=(const FeatureCollection<Feats...>& feats) {
      _merge<Feats...>(feats);
      return *this;
    }

  protected:
    template<class T> requires (mstd::is_any_of<T, Feats...>)
    void merge(const FeatureList<T>& feat) {
      static constexpr size_t merge_index = mstd::var_type_index<T, Feats...>();
      if constexpr (std::is_same_v<T, bool>) {
        this->template get<merge_index>() |= feat;
      } else {
        for(size_t i = 0; i < feat.size(); ++i)
          mstd::append(this->template get<merge_index>()[i], feat[i]);
      }
    }

    template<class First, class... Others>
    void _merge(const auto& feats) {
      merge<First>(feats.template get_by_type<First>());
      if constexpr (sizeof...(Others) > 0)
        _merge<Others...>(feats);
    }
  };

  // ------- FeatureCount: factories ---------  
  // ------- FeatureCount: concepts ---------
  // ------- FeatureCount: deduction guides ---------
  // ------- FeatureCount: defaults ---------
  template<class T> struct FeatureCountFor_ {};
  template<class... Feats> struct FeatureCountFor_<FeatureCollection<Feats...>> { using type = FeatureCount<Feats...>; };
  template<class T> using FeatureCountFor = typename FeatureCountFor_<std::remove_cvref_t<T>>::type;

}
