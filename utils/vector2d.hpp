

//! file vector2d.hpp
/** This is a 2 dimentional vector (aka matrix) that is slightly faster than
 * vector<vector<T>> and slightly more convenient than keeping track of the
 * index offsets using vector<T>
 **/

#pragma once

#include <cassert>
#include <vector>

namespace mstd{
  struct Symmetric {};
  struct Asymmetric {};
  
  template<typename Element, typename Something = std::vector<Element>, typename Symmetry = Asymmetric>
  class Something2d : public Something
  {
  protected:
    size_t columns = 0;
    using Parent = Something;
    using Coords = std::pair<size_t, size_t>;

    static constexpr bool is_symmetric = std::is_same_v<Symmetry, Symmetric>;


    template<class Sym = Symmetry> requires (!std::is_same_v<Sym, Symmetric>)
    size_t linearize(const Coords& coords) const
    {
      return coords.second * columns + coords.first;
    }

    template<class Sym = Symmetry> requires (std::is_same_v<Sym, Symmetric>)
    size_t linearize(const Coords& coords) const
    {
      if(coords.first <= coords.second)
        return (coords.second * (coords.second + 1)) / 2  + coords.first;
      else
        return (coords.first * (coords.first + 1)) / 2  + coords.second;
    }

  public:
    Something2d(): Something() {}

    Something2d(const size_t cols, const size_t rows, const Element& element = Element()):
      columns(cols)
    {
      // reserve size such that we can access the last index (cols-1,rows-1)
      Something::resize(linearize({cols - 1, rows - 1}) + 1, element);
    }

    template<class Sym = Symmetry> requires (!std::is_same_v<Sym, Symmetric>)
    Coords size() const {
      assert(columns > 0);
      return {columns, Something::size() / columns};
    }
    template<class Sym = Symmetry> requires (std::is_same_v<Sym, Symmetric>)
    Coords size() const {
      assert(columns > 0);
      return {columns, columns};
    }


    //! NOTE: coordinates are always (col, row) as in (x, y)
    //! NOTE: the coordinates given to operator[] SHOULD NOT BE OUT OF BOUNDS, otherwise, unexpected things might happen
    /** also: why in the hell does C++ force operator[] to take exactly one argument??? **/
    Element& operator[](const Coords& coords)
    {
      return Something::operator[](linearize(coords));
    }
    
    const Element& operator[](const Coords& coords) const
    {
      return Something::operator[](linearize(coords));
    }
    
    const Element& at(const Coords& coords) const
    {
      return Something::at(linearize(coords));
    }
    
    void resize(const size_t cols, const size_t rows, const Element& element = Element())
    {
      columns = cols;
      Something::resize(linearize({cols - 1, rows - 1}) + 1, element);
    }
    
    size_t rows() const
    {
      return size().second;
    }
    size_t cols() const
    {
      return columns;
    }
  };


  template<typename Element>
  using vector2d = Something2d<Element, std::vector<Element>, Asymmetric>;
  template<typename Element>
  using symmetric_vector2d = Something2d<Element, std::vector<Element>, Symmetric>;


}// namespace


