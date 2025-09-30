
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
  template<class T> requires mstd::is_any_of<T, bool, float, double, long double>
  struct FeatureAccu_<T> { using type = T; /* percentage of the feature */ };
  template<class T> using FeatureAccu = typename FeatureAccu_<std::remove_cvref_t<T>>::type;

  // a feature counter is a Features-class, but with feature-accumulators instead of features
  template<class... Feats>
  struct FeatureCount:
    public Features<FeatureList, FeatureAccu<Feats>...>
  {
    using Parent = Features<FeatureList, FeatureAccu<Feats>...>;

    template<class First, class... Others> requires (mstd::is_any_of<First, Feats...>)
    auto _count_features() const {
      using Result = std::conditional_t<std::is_arithmetic_v<First> and not std::is_integral_v<First>, First, size_t>;
      static constexpr size_t count_index = mstd::var_type_index<First, Feats...>();
      
      const auto& feats = Parent::template get<count_index>();
      Result result = 0;
      
      // if the feature is a rational, then the score is the sum of maximal values that have been gathered
      if constexpr (std::is_arithmetic_v<First> and not std::is_integral_v<First>) {
        result += std::ranges::fold_left(feats, Result{0}, std::plus<>{});
      } else if constexpr (std::is_same_v<First, bool>) { // for bools, the score is the number of different features that we saw
        result += feats.size();
      } else { // for everything else, the score is the number of different versions we saw of each feature on the list
        static_assert(mstd::VectorType<First>);
        result += std::ranges::fold_left(feats, Result{0}, [](const Result x, const auto& feat_vec){ return x + feat_vec.size(); });
      }

      if constexpr (sizeof...(Others) > 0) {
        return result + _count_features<Others...>(); // NOTE: size_t to float promotion will happen here if any result is floating-point
      } else return result;
    }

    // return the amount of features present in the accumulators (may be floating-point if there are floating-point features)
    auto count_features() const { return _count_features<Feats...>(); }

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
        if constexpr (std::is_same_v<T, bool>) {
          feat_counts |= feat;
        } else {
          assert(mstd::VectorType<decltype(feat_counts)>);
          if(feat_counts.size() < feat.size())
            feat_counts.resize(feat.size());
          
          // fractional features are assumed to be percentages --> get the maximum
          // for all other features, we just count the variety
          if constexpr (std::is_arithmetic_v<T> and not std::is_integral_v<T>) {
            for(size_t i = 0; i < feat.size(); ++i) {
              auto& stored_val = Parent::template get<merge_index>()[i];
              if(feat[i] > stored_val) stored_val = feat[i];
            }
          } else { // get the counts of all features of the current type (this is a vector of HashMaps)
            for(size_t i = 0; i < feat.size(); ++i)
              mstd::append(Parent::template get<merge_index>()[i], feat[i]);
          }
        }
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
