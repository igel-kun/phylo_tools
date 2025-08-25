

#pragma once

#include "optional_tuple.hpp"

namespace PT {
  // ========== FeatureCollection ==========
  // A FeatureCollection contains, for each given type a list of instances of this type
  // representing the features that a species may have.
  // The number and order of the instances in each list is determined at runtime by,
  // for example, reading a feature matrix or inserting features into their lists manually.

  // ------- FeatureCollection: helpers ---------

  // a feature-list is an ordered(!) list of features (each leaf gets one?)
  template<class T> struct FeatureList_ { using type = std::vector<T>; };
  template<> struct FeatureList_<bool> { using type = mstd::ordered_bitset; };
  template<class T> using FeatureList = typename FeatureList_<std::remove_cvref_t<T>>::type;


  template<template<class> class FeatClass, class... Feats>
  struct Features: 
    public mstd::optional_tuple<FeatClass<Feats>...>
  {
    using Tuple = mstd::optional_tuple<FeatClass<Feats>...>;
    using Tuple::Tuple;

    template<class T> static constexpr auto index = mstd::var_type_index<T, Feats...>();
    template<class T> static constexpr bool occurs = mstd::is_any_of<T, Feats...>;
    template<class T> auto& get_by_type() { return this->template get<index<T>>(); };
    template<class T> const auto& get_by_type() const { return this->template get<index<T>>(); };

    friend std::ostream& operator<<(std::ostream& os, const Features<FeatClass, Feats...>& x) {
      return os << static_cast<const Tuple&>(x);
    }
  };


  // ------- FeatureCollection: main class ---------
  // 'FeatureCollection' contains a FeatureList for each type in Feats...
  template<class... Feats>
  using FeatureCollection = Features<FeatureList, Feats...>;


  // ------- FeatureCollection: factories ---------
  
  // ------- FeatureCollection: concepts ---------
   template<class T> static constexpr bool is_features_collection = false;
  template<class... Feats> static constexpr bool is_features_collection<FeatureCollection<Feats...>> = true;
  template<class T> concept StrictFeatureCollectionType = is_features_collection<T>;
  template<class T> concept FeatureCollectionType = StrictFeatureCollectionType<std::remove_cvref_t<T>>;
 
  // ------- FeatureCollection: deduction guides ---------
  
  // ------- FeatureCollection: defaults ---------
  using DefaultFeatureCollection = FeatureCollection<bool, int32_t, double, std::string>;

}


