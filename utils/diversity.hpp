
#pragma once


namespace PT {

  template<StrictPhylogenyType Net, class GammaFunctor, class InheritanceProbFunctor>
  class DiversityCalculator {
    using Edge = typename Net::Edge;

    NodeMap<Degree> updated_children;
    [[no_unique_address]] GammaFunctor gamma;
    [[no_unique_address]] InheritanceProbFunctor inheritance_prob;

    void mark_updated_along(const Edge& e) {
      const NodeDesc u = e.tail();
      Degree& u_updated_children = updated_children[u];
      if(++u_updated_children == Net::out_degree(u))
        take_node(u);  
    }

  public:

    // calling this multiple times for the same node WILL break the calculation, please don't do that!
    void take_node(const NodeDesc x, const float child_gamma = 1.0) {
      for(const auto e: Net::in_edges(x)) {
        gamma(e) = child_gamma * inheritance_prob(e);
        mark_updated_along(e);
      }
    }
    void dont_take_node(const NodeDesc x) { take_node(x, 0.0); }


    // ---------------------- construction ----------------------------

    // if the first argument is neither a phylogeny nor a nodecontainer, then initialize the gamma-functor with all arguments
    template<mstd::TupleType GammaInit, mstd::TupleType InhProbInit>
    DiversityCalculator(std::piecewise_construct_t, GammaInit&& g_init, InhProbInit&& i_init):
      gamma{make_from_tuple<GammaFunctor>(std::forward<GammaInit>(g_init))},
      inheritance_prob{make_from_tuple<InheritanceProbFunctor>(std::forward<InhProbInit>(i_init))}
    {}

    template<class GammaInit, class InhProbInit> requires (!mstd::TupleType<GammaInit> && !mstd::TupleType<InhProbInit>)
    DiversityCalculator(GammaInit&& g_init, InhProbInit&& i_init):
      gamma{std::forward<GammaInit>(g_init)}, inheritance_prob{std::forward<InhProbInit>(i_init)}
    {}
  
    // pass some leaves to be taken; make it searchable efficiently if possible
    template<NodeContainerType Nodes, class... Args>
    DiversityCalculator(Nodes&& leaves_to_take, const Net& N, Args&&... args):
      DiversityCalculator(std::forward<Args>(args)...)
    {
      // take all leaves in leaves_to_take, and don't take all other leaves (take with inh. prob. 0)
      for(const NodeDesc u: N.leaves())
        take_node(u, test(leaves_to_take, u));
    }
  };

  // deduce the Phylogeny, but provide the GammaFunctor type in the template
  template<class GammaFunctor, class InheritanceProbFunctor, NodeContainerType Nodes, StrictPhylogenyType Net, class... Args>
  auto calculate_diversity(Nodes&& nodes, const Net& net, std::piecewise_construct_t pwc, Args&&... args) {
    return DiversityCalculator<Net, GammaFunctor, InheritanceProbFunctor>(std::forward<Nodes>(nodes), net, pwc, std::forward<Args>(args)...);
  }
  // deduce everything
  template<class GammaFunctor, class InheritanceProbFunctor, NodeContainerType Nodes, StrictPhylogenyType Net>
  auto calculate_diversity(Nodes&& nodes, const Net& net, GammaFunctor&& gamma, InheritanceProbFunctor&& inh_prob) {
    return DiversityCalculator<Net, GammaFunctor, InheritanceProbFunctor> {
      std::forward<Nodes>(nodes), net, std::forward<GammaFunctor>(gamma), std::forward<InheritanceProbFunctor>(inh_prob)};
  }

}

