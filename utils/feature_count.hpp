
#pragma once

#include "feature_collection.hpp"

namespace PT {

  // ========== FeatureCount ==========
  // compute the total number of features in a FeatureCollection
  // To this end, we just sum up the size() of each feature-list in the collection

  // ------- FeatureCount: helpers ---------
  
  // ------- FeatureCount: main class ---------
  // in a feature accumulator, we can collect all features seen in a set of nodes
  template<class T> struct FeatureAccu_ { using type = HashSet<T>; /* list all feature we've seen for this position */ };
  template<> struct FeatureAccu_<bool> { using type = bool; /* did we see the feature?*/ };
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
      if(not feat.empty()) {
        auto& feat_counts = Parent::template get<merge_index>();
        DEBUG5(std::cout << "merging " << feat << " into "<<feat_counts<<'\n');
        if constexpr (not std::is_same_v<T, bool>) {
          // get the counts of all features of the current type (this is a vector of HashMaps)
          assert(mstd::VectorType<decltype(feat_counts)>);
          if(feat_counts.size() < feat.size())
            feat_counts.resize(feat.size());
          for(size_t i = 0; i < feat.size(); ++i)
            mstd::append(Parent::template get<merge_index>()[i], feat[i]).first;
        } else feat_counts |= feat;
      }
    }

    template<class First, class... Others>
    void _merge(const auto& feats) {
      DEBUG5(std::cout << "merging "<< feats.template get_by_type<First>().size() << " features of type "<<mstd::type_name<First>()<<" to feature-counter\n");

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
