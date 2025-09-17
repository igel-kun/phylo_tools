
#pragma once

#include "brute_force.hpp"
#include "feature_collection.hpp"
#include "feature_count.hpp"

namespace PT {

  // ========== feature_diversity ==========
  // compute or optimize the feature diversity of a subset of leaves,
  // given a collection of feature-lists

  // ------- feature_diversity: helpers ---------
  
  // ------- feature_diversity: main class ---------
   struct feature_diversity {
    // compute the feature diversity of a given set of nodes
    template<mstd::IterableType Container>
      requires FeatureCollectionType<mstd::value_type_of_t<Container>>
    size_t operator()(Container&& container) {
      using ContainerCounter = FeatureCountFor<mstd::value_type_of_t<Container>>;
      ContainerCounter accu;
      // TODO: use std::accumulate here
      for(const auto& feat: container) {
        DEBUG5(std::cout << "got container of "<<mstd::type_name<mstd::value_type_of_t<Container>>() << '\n');
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

  // ------- feature_diversity: factories ---------
  template<mstd::IterableType Container> requires FeatureCollectionType<mstd::value_type_of_t<Container>>
  size_t compute_feature_diversity(Container&& container) { return feature_diversity{}(std::forward<Container>(container)); }

  template<mstd::StrictIterableType Container> // container should contain FeatureCollections or indirections to FeatureCollections
    requires (FeatureCollectionType<mstd::value_type_of_t<Container>> or
              FeatureCollectionType<mstd::value_type_of_t<mstd::value_type_of_t<Container>>>)
  auto optimize_feature_diversity(const auto k, const Container& container, size_t num_solutions = 1) {
    return mstd::brute_force(k, container, num_solutions, feature_diversity{});
  }
  template<mstd::StrictMapType Map> // Map should map to FeatureCollections or indirections to FeatureCollections
    requires (FeatureCollectionType<mstd::mapped_type_of_t<Map>> or
              FeatureCollectionType<mstd::value_type_of_t<mstd::mapped_type_of_t<Map>>>)
  auto optimize_feature_diversity(const auto k, const Map& _map, size_t num_solutions = 1) {
    return mstd::brute_force(k, _map, num_solutions, feature_diversity{});
  }
 
  // ------- feature_diversity: concepts ---------  
  // ------- feature_diversity: deduction guides ---------
  // ------- feature_diversity: defaults ---------
}
