
#pragma once

/*
 * this class can handle generic node-/edge- data (also see examples/gen.cpp for how to use it)
 * use this if you're reading a network with unknown node-/edge- data **that you want to keep**
 * (if you're not interested in keeping the data, just ignore it using mstd::ConstFunction as a property reader)
 * To use the generic data handler, just declare your network accordingly, for example:
 * using MyNetwork = DefaultLabeledNetwork<DefaultDataVec>; // a network with node labels and generic node-data
 * using MyNetwork = DefaultNetwork<void, DefaultDataVec>; // a network without node labels but with generic edge-data
 * using MyNetwork = DefaultDAG<DefaulDataVec, DefaultDataVec>;
 *    // a DAG (vector of roots) without node labels but with generic node-data and generic edge-data
 *
 * By default, the generic data is a std::vector of std::variant<intptr_t, floatptr_t, MyString> so use vector/variant API to access it
 * (where, by default, MyString is an 8-byte owning (aka memory-managed) char* called mstd::StringNoLength).
 * For example, you can access the generic data as follows:
 *    MyNetwork N = ...;
 *    NodeDesc u = ....;
 *    N[u].data().at(10).get<intptr_t>() = ...; // interpret the 10'th field of node u's data as integer and set that integer
 *                                              // NOTE: this will throw std::bad_variant_access if the 10'th helt something other than an integer before
 *    N[u].data().at(7).emplace<StringNoLength>("blubb"); // replace the value of the 7th field by the string "blubb"
 *                                                        // NOTE: this will not throw, but destroy the object helt before
 *
 * Of course, you can customize the generic data by passing your own types to DataVec :)
 */

#include <optional>

#include "charp.hpp"
#include "token.hpp"

#include "config.hpp"

namespace mstd {

  template<class T>
  struct generic_reader {
    static_assert(not Variant<T>); // for Variant<T>, the class should be specialized later on

    using Result = std::optional<T>;

    static Result parse(const std::string_view s) {
      if constexpr (std::is_arithmetic_v<T>) {
        Result result;
        if(!s.empty()) {
          size_t first_unconverted;
          result = stoX<T>(s, first_unconverted);
          if(first_unconverted != s.size()) result.reset();
        }
        return result;
      } else { // if it's any other type, construct it from string_view
        static_assert(std::is_constructible_v<T, const std::string_view&>);
        return Result{s};
      }
    }
    
    Result operator()(const std::string_view s) const { return parse(s); }
  };

  template<class... Ts> requires (sizeof...(Ts) > 0)
  struct generic_reader<std::variant<Ts...>> {
    using Result = std::optional<std::variant<Ts...>>;
    
  protected:
    template<class First, class... Rest>
    static Result parse_(const std::string_view s) {
      auto opt = generic_reader<First>::parse(s);
      if(not opt.has_value()) {
        if constexpr (sizeof...(Rest) > 0) {
          return parse_<Rest...>(s);
        } else return Result{};
      } else return Result{std::move(*opt)};
    }

  public:
    static Result parse(const std::string_view s) { return parse_<Ts...>(s); }
    
    Result operator()(const std::string_view s) const { return parse(s); }
  };



  template<class... Items>
  struct DataVec: public std::vector<std::variant<Items...>> {
    using Data = std::variant<Items...>;
    
  protected:
    using Parent = std::vector<Data>;

  public:
    template<class T>
    decltype(auto) emplace_item(std::piecewise_construct_t, T&& t) { return Parent::emplace_back(std::forward<T>(t)); }

    void emplace_items(std::piecewise_construct_t) {}
    
    template<class Last, class... Args>
    void emplace_items(std::piecewise_construct_t, Args&&... args, Last&& last) {
      DEBUG5(std::cout << "adding to data of length "<<Parent::size()<<'\n');
      emplace_items(std::piecewise_construct_t{}, std::forward<Args>(args)...);
      emplace_item(std::piecewise_construct_t{}, std::forward<Last>(last));
    }

    decltype(auto) emplace_item(const std::string_view s) {
      DEBUG5(std::cout << "adding to data of length "<<Parent::size()<<'\n');
      auto opt = generic_reader<Data>::parse(s);
      if(opt)
        return Parent::emplace_back(*opt);
      else throw std::logic_error{std::string{"Could not read "} + mstd::type_name<Data>() + " from " + s};
    }

    void emplace_items(const std::string_view s) {
      DEBUG4(std::cout << "making generic data by splitting the string '"<<s<<"'\n");
      for(const auto x: tokenize(s, config::data_delimeters)) {
        emplace_item(x);
        DEBUG5(std::cout << "\tread generic data '"<<Parent::back()<<"' from '"<<x<<"'\n");
      }
    }

    DataVec() = default;

    // construct from a bunch of Stringlikes
    template<class Last> requires (Stringlike<Last>)
    DataVec(Last&& last, bool shrink = true) {
      emplace_items(last);
      if(shrink) Parent::shrink_to_fit();
    }
    template<class FirstString, class LastString, class... Strings> requires (Stringlike<LastString> && Stringlike<FirstString>)
    DataVec(FirstString&& first, Strings&&... strings, LastString&& last, bool shrink = true):
      DataVec(std::forward<FirstString>(first), std::forward<Strings>(strings)..., false)
    {
      emplace_items(last);
      if(shrink) Parent::shrink_to_fit();
    }

    // construct items directly by their type
    template<class... Items_>
    DataVec(std::piecewise_construct_t x, Items_&&... items, bool shrink = true) {
      emplace_items(std::piecewise_construct_t{}, std::forward<Items_>(items)...);
      if(shrink) Parent::shrink_to_fit();
    }


    friend std::ostream& operator<<(std::ostream& os, const DataVec& dv) {
      bool first_item = true;
      for(auto& x: dv) {
        if(not first_item) {
          os << config::data_delimeters[0];
        } else first_item = false;
        os << x;
      }
      return os;
    }
  };

  using DefaultDataVec = DataVec<intptr_t, floatptr_t, StringNoLength>;


}
