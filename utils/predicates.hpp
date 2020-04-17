

namespace std{
  // Predicates
  // note: for an iterator it, use pred.value(it) to get the value of the predicate for the item at position it
  template<class Predicate>
  struct NotPredicate: public Predicate
  {
    template<class X> static bool value(const X& x) { return !Predicate::value(x); }
  };
  struct TruePredicate{ template<class X> static bool value(X& x) { return true; } };
  using FalsePredicate = NotPredicate<TruePredicate>;

  // predicate for maps, applying a predicate to the values
  template<class _Map, class _ItemPredicate>
  struct MapPredicate
  {
    using value_type = typename _Map::value_type;
    const _Map& X;
    const _ItemPredicate it_pred;
    
    template<class... Args>
    MapPredicate(const _Map& _X, Args&&... args): X(_X), it_pred(forward<Args>(args)...) {}
  };
  template<class _Map, class _ItemPredicate>
  struct MapValuePredicate: public MapPredicate<_Map, _ItemPredicate>{
    using Parent = MapPredicate<_Map, _ItemPredicate>;
    using Parent::Parent;
    template<class MapIter>
    bool value(const MapIter& it) const {
      return Parent::it_pred.value(&(it->second));
    }
  };
  template<class _Map, class _ItemPredicate>
  struct MapKeyPredicate: public MapPredicate<_Map, _ItemPredicate>{
    using Parent = MapPredicate<_Map, _ItemPredicate>;
    using Parent::Parent;
    template<class MapIter>
    bool value(const MapIter& it) const { return Parent::it_pred.value(&(it->first)); }
  };

  // predicate for containers of sets, returning whether the given set is empty
  template<class _Set>
  struct EmptySetPredicate {
    template<class _SetIter = _Set*>
    static bool value(const _SetIter& x) {return x->empty();}
  };
  template<class _Set>
  using NonEmptySetPredicate = NotPredicate<EmptySetPredicate<_Set>>;

  // a predicate returning true/or false depending whether the query is in a given set
  template<class Container, bool is_in = true>
  struct ContainmentPredicate{
    using Item = typename Container::value_type;
    const Container& c;
    ContainmentPredicate(const Container& _c): c(_c) {}
    template<class ContainerIter>
    bool value(const ContainerIter& it) const { return test(c, reinterpret_cast<const Item>(*it)) == is_in; }
  };


}
