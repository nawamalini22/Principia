﻿
#pragma once

#include <array>
#include <initializer_list>
#include <vector>

#include "quantities/named_quantities.hpp"

namespace principia {
namespace numerics {
namespace internal_fixed_arrays {

using quantities::Product;

template<typename Scalar, int rows, int columns>
class FixedMatrix;

template<typename Scalar, int size_>
class FixedVector final {
 public:
  constexpr FixedVector();  // Zero-initialized.
  constexpr explicit FixedVector(std::array<Scalar, size_> const& data);
  FixedVector(
      std::initializer_list<Scalar> const& data);  // NOLINT(runtime/explicit)

  bool operator==(FixedVector const& right) const;
  FixedVector& operator=(std::initializer_list<Scalar> const& right);

  Scalar& operator[](int index);
  constexpr Scalar const& operator[](int index) const;

  operator std::vector<Scalar>() const;

  static constexpr int size = size_;

 private:
  std::array<Scalar, size> data_;

  template<typename L, typename R, int r, int c>
  friend FixedVector<Product<L, R>, r> operator*(
      FixedMatrix<L, r, c> const& left,
      FixedVector<R, c> const& right);
};

template<typename Scalar, int rows, int columns>
class FixedMatrix final {
 public:
  // The |data| must be in row-major format.
  constexpr explicit FixedMatrix(
      std::array<Scalar, rows * columns> const& data);
  FixedMatrix(
      std::initializer_list<Scalar> const& data);  // NOLINT(runtime/explicit)

  bool operator==(FixedMatrix const& right) const;
  FixedMatrix& operator=(std::initializer_list<Scalar> const& right);

 private:
  std::array<Scalar, rows * columns> data_;

  template<typename L, typename R, int r, int c>
  friend FixedVector<Product<L, R>, r> operator*(
      FixedMatrix<L, r, c> const& left,
      FixedVector<R, c> const& right);
};

template<typename ScalarLeft, typename ScalarRight, int rows, int columns>
FixedVector<Product<ScalarLeft, ScalarRight>, rows> operator*(
    FixedMatrix<ScalarLeft, rows, columns> const& left,
    FixedVector<ScalarRight, columns> const& right);

template<typename Scalar, int rows>
class FixedStrictlyLowerTriangularMatrix final {
 public:
  static int constexpr dimension = rows * (rows - 1) / 2;

  // The |data| must be in row-major format.
  constexpr explicit FixedStrictlyLowerTriangularMatrix(
      std::array<Scalar, dimension> const& data);
  FixedStrictlyLowerTriangularMatrix(
      std::initializer_list<Scalar> const& data);  // NOLINT(runtime/explicit)

  bool operator==(FixedStrictlyLowerTriangularMatrix const& right) const;
  FixedStrictlyLowerTriangularMatrix& operator=(
      std::initializer_list<Scalar> const& right);

  // For  0 < j < i < rows, the entry a_ij is accessed as |a[i][j]|.
  // if i and j do not satisfy these conditions, the expression |a[i][j]| is
  // erroneous.
  Scalar* operator[](int index);
  constexpr Scalar const* operator[](int index) const;

 private:
  std::array<Scalar, dimension> data_;
};

}  // namespace internal_fixed_arrays

using internal_fixed_arrays::FixedMatrix;
using internal_fixed_arrays::FixedStrictlyLowerTriangularMatrix;
using internal_fixed_arrays::FixedVector;

}  // namespace numerics
}  // namespace principia

#include "numerics/fixed_arrays_body.hpp"
