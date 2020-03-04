#ifndef _FUTURE_EXTRAS_H_
#define _FUTURE_EXTRAS_H_

//===- llvm/ADT/BitVector.h - Bit vectors -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the BitVector class.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace llvm {

/// ForwardIterator for the bits that are set.
/// Iterators get invalidated when resize / reserve is called.
template <typename BitVectorT> class const_set_bits_iterator_impl {
  const BitVectorT &Parent;
  int Current = 0;

  void advance() {
    assert(Current != -1 && "Trying to advance past end.");
    Current = Parent.find_next(Current);
  }

public:
  const_set_bits_iterator_impl(const BitVectorT &Parent, int Current)
      : Parent(Parent), Current(Current) {}
  explicit const_set_bits_iterator_impl(const BitVectorT &Parent)
      : const_set_bits_iterator_impl(Parent, Parent.find_first()) {}
  const_set_bits_iterator_impl(const const_set_bits_iterator_impl &) = default;

  const_set_bits_iterator_impl operator++(int) {
    auto Prev = *this;
    advance();
    return Prev;
  }

  const_set_bits_iterator_impl &operator++() {
    advance();
    return *this;
  }

  unsigned operator*() const { return Current; }

  bool operator==(const const_set_bits_iterator_impl &Other) const {
    assert(&Parent == &Other.Parent &&
           "Comparing iterators from different BitVectors");
    return Current == Other.Current;
  }

  bool operator!=(const const_set_bits_iterator_impl &Other) const {
    assert(&Parent == &Other.Parent &&
           "Comparing iterators from different BitVectors");
    return Current != Other.Current;
  }
};

class SmallBitVector;

using sbv_const_set_bits_iterator = const_set_bits_iterator_impl<SmallBitVector>;
using sbv_set_iterator = sbv_const_set_bits_iterator;

sbv_const_set_bits_iterator sbv_set_bits_begin(SmallBitVector *sbv) {
  return sbv_const_set_bits_iterator(*sbv);
}

sbv_const_set_bits_iterator sbv_set_bits_end(SmallBitVector *sbv) {
  return sbv_const_set_bits_iterator(*sbv, -1);
}

iterator_range<sbv_const_set_bits_iterator> sbv_set_bits(SmallBitVector *sbv) {
  return make_range(sbv_set_bits_begin(sbv), sbv_set_bits_end(sbv));
}

} // end namespace llvm

#if __cplusplus < 201300
namespace std {
  template<class T> struct _Unique_if {
    typedef unique_ptr<T> _Single_object;
  };

  template<class T> struct _Unique_if<T[]> {
    typedef unique_ptr<T[]> _Unknown_bound;
  };

  template<class T, size_t N> struct _Unique_if<T[N]> {
    typedef void _Known_bound;
  };

  template<class T, class... Args>
    typename _Unique_if<T>::_Single_object
    make_unique(Args&&... args) {
      return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

  template<class T>
    typename _Unique_if<T>::_Unknown_bound
    make_unique(size_t n) {
      typedef typename remove_extent<T>::type U;
      return unique_ptr<T>(new U[n]());
    }

  template<class T, class... Args>
    typename _Unique_if<T>::_Known_bound
    make_unique(Args&&...) = delete;
}
#endif

#endif /* _FUTURE_EXTRAS_H_ */
