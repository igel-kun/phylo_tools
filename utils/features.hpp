

#pragma once

#include "brute_force.hpp"

#include "io/features.hpp"

namespace PT {

  // a feature-list is an ordered(!) list of features (each leaf gets one?)
  template<class T> struct _FeatureList { using type = std::vector<T>; };
  template<> struct _FeatureList<bool> { using type = mstd::ordered_bitset; };
  template<class T> using FeatureList = typename _FeatureList<std::remove_cvref_t<T>>::type;


  template<template<class> class FeatClass, class... Feats>
  struct _Features: public mstd::optional_tuple<FeatClass<Feats>...> {
    using Tuple = mstd::optional_tuple<FeatClass<Feats>...>;
    using Tuple::Tuple;

    template<class T> static constexpr auto index = mstd::var_type_index<T, Feats...>();
    template<class T> static constexpr bool occurs = mstd::is_in<T, Feats...>;
    template<class T> auto& get_by_type() { return this->template get<index<T>>(); };
    template<class T> const auto& get_by_type() const { return this->template get<index<T>>(); };

    friend std::ostream& operator<<(std::ostream& os, const _Features<FeatClass, Feats...>& x) {
      return os << static_cast<const Tuple&>(x);
    }
  };

  // 'FeatureCollection' contains a FeatureList for each type in Feats...
  template<class... Feats>
  using FeatureCollection = _Features<FeatureList, Feats...>;

  using DefaultFeatureCollection = FeatureCollection<bool, int32_t, double, std::string>;

  template<class T> static constexpr bool is_features_collection = false;
  template<class... Feats> static constexpr bool is_features_collection<FeatureCollection<Feats...>> = true;
  template<class T> concept StrictFeatureCollectionType = is_features_collection<T>;
  template<class T> concept FeatureCollectionType = StrictFeatureCollectionType<std::remove_cvref_t<T>>;


  // in a feature accumulator, we can collect all features seen in a set of nodes
  template<class T> struct _FeatureAccu { using type = HashSet<T>; };
  template<> struct _FeatureAccu<bool> { using type = bool; /* did we see the features?*/ };
  template<class T> using FeatureAccu = typename _FeatureAccu<std::remove_cvref_t<T>>::type;

  // a feature counter has is a Features-class, but has feature-accumulators instead of features
  template<class... Feats>
  struct FeatureCount: public _Features<FeatureList, FeatureAccu<Feats>...>
  {
  protected:
    using Parent = _Features<FeatureList, FeatureAccu<Feats>...>;

    template<class First, class... Others> requires (mstd::is_in<First, Feats...>)
    size_t _count_features() const {
      static constexpr size_t count_index = mstd::var_type_index<First, Feats...>();
      if constexpr (sizeof...(Others) > 0)
        return _count_features<Others...>() + Parent::template get<count_index>().size();
      else return Parent::template get<count_index>().size();
    }

  public:
    // return the amount of features present in the accumulators
    size_t count_features() const { return _count_features<Feats...>(); }

  protected:
    template<class T> requires (mstd::is_in<T, Feats...>)
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

  public:

    auto& operator+=(const FeatureCollection<Feats...>& feats) {
      _merge<Feats...>(feats);
      return *this;
    }
  };

  template<class T> struct _FeatureCountFor {};
  template<class... Feats> struct _FeatureCountFor<FeatureCollection<Feats...>> { using type = FeatureCount<Feats...>; };
  template<class T> using FeatureCountFor = typename _FeatureCountFor<std::remove_cvref_t<T>>::type;


  struct _feature_diversity {
    // compute the feature diversity of a given set of nodes
    template<mstd::IterableType Container>
      requires FeatureCollectionType<mstd::value_type_of_t<Container>>
    size_t operator()(Container&& container) {
      // TODO: use std::accumulate here
      FeatureCountFor<mstd::value_type_of_t<Container>> accu;
      for(const auto& feat: container) {
        DEBUG5(std::cout << "adding "<<feat<<" to accu " << accu <<'\n');
        accu += feat;
      }
      return accu.count_features();
    }

    template<mstd::IterableType Map>
      requires FeatureCollectionType<typename mstd::value_type_of_t<Map>::second_type>
    size_t operator()(Map&& feat_map) {
      DEBUG4(std::cout << "computing score for "<<feat_map<<'\n');
      //return operator()(feat_map | std::ranges::views::transform([](const auto& p){return p.second; }));
      //return operator()(feat_map | std::ranges::views::transform(mstd::selector<1>{}));
      return operator()(mstd::seconds(feat_map));
    }

  };

  template<mstd::IterableType Container> requires FeatureCollectionType<mstd::value_type_of_t<Container>>
  size_t feature_diversity(Container&& container) { return _feature_diversity{}(std::forward<Container>(container)); }


  template<mstd::IterableType Container> // container should contain FeatureCollections or indirections to FeatureCollections
    requires (FeatureCollectionType<mstd::value_type_of_t<Container>> or
              FeatureCollectionType<mstd::value_type_of_t<mstd::value_type_of_t<Container>>>)
  auto optimize_feature_diversity(const auto k, const Container& container, size_t num_solutions = 1) {
    return mstd::brute_force(k, container, num_solutions, _feature_diversity{});
  }
  template<mstd::MapType Map> // Map should map to FeatureCollections or indirections to FeatureCollections
    requires (FeatureCollectionType<mstd::mapped_type_of_t<Map>> or
              FeatureCollectionType<mstd::value_type_of_t<mstd::mapped_type_of_t<Map>>>)
  auto optimize_feature_diversity(const auto k, const Map& _map, size_t num_solutions = 1) {
    return mstd::brute_force(k, _map, num_solutions, _feature_diversity{});
  }

}


