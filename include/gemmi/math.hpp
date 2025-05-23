// Copyright 2018 Global Phasing Ltd.
//
// Math utilities. 3D linear algebra.

#ifndef GEMMI_MATH_HPP_
#define GEMMI_MATH_HPP_

#include <cmath>      // for fabs, cos, sqrt, round
#include <algorithm>  // for min
#include <array>
#include <stdexcept>  // for out_of_range
#include <type_traits>  // for enable_if, is_integral

namespace gemmi {

constexpr double pi() { return 3.1415926535897932384626433832795029; }

// The value used in converting between energy[eV] and wavelength[Angstrom].
// $ units -d15 'h * c / eV / angstrom'
constexpr double hc() { return 12398.4197386209; }

// The Bohr radius (a0) in Angstroms.
constexpr double bohrradius() { return 0.529177210903; }

// for Mott-Bethe factor
constexpr double mott_bethe_const() { return 1. / (2 * pi() * pi() * bohrradius()); }

// Used in conversion of ADPs (atomic displacement parameters).
constexpr double u_to_b() { return 8 * pi() * pi(); }

constexpr double deg(double angle) { return 180.0 / pi() * angle; }
constexpr double rad(double angle) { return pi() / 180.0 * angle; }

constexpr float sq(float x) { return x * x; }
constexpr double sq(double x) { return x * x; }

inline double log_cosh(double x) {
  // cosh(x) would overflow for x > 710.5, so we calculate:
  // ln(cosh(x)) = ln(e^x + e^-x) - ln(2) = ln(e^x * (1 + e^-2x)) - ln(2)
  x = std::abs(x);
  return x - std::log(2) + std::log1p(std::exp(-2 * x));
}

inline int iround(double d) { return static_cast<int>(std::round(d)); }

inline double angle_abs_diff(double a, double b, double full=360.0) {
  double d = std::fabs(a - b);
  if (d > full)
    d -= std::floor(d / full) * full;
  return std::min(d, full - d);
}

// similar to C++17 std::clamp()
template<class T> constexpr T clamp(T v, T lo, T hi) {
  return std::min(std::max(v, lo), hi);
}

template <typename Real>
struct Vec3_ {
  Real x, y, z;

  Vec3_() : x(0), y(0), z(0) {}
  Vec3_(Real x_, Real y_, Real z_) : x(x_), y(y_), z(z_) {}
  explicit Vec3_(std::array<int, 3> h) : x(h[0]), y(h[1]), z(h[2]) {}

  Real& at(int i) {
    switch (i) {
      case 0: return x;
      case 1: return y;
      case 2: return z;
      default: throw std::out_of_range("Vec3 index must be 0, 1 or 2.");
    }
  }
  Real at(int i) const { return const_cast<Vec3_*>(this)->at(i); }

  Vec3_ operator-() const { return {-x, -y, -z}; }
  Vec3_ operator-(const Vec3_& o) const { return {x-o.x, y-o.y, z-o.z}; }
  Vec3_ operator+(const Vec3_& o) const { return {x+o.x, y+o.y, z+o.z}; }
  Vec3_ operator*(Real d) const { return {x*d, y*d, z*d}; }
  Vec3_ operator/(Real d) const { return *this * (1.0/d); }
  Vec3_& operator-=(const Vec3_& o) { *this = *this - o; return *this; }
  Vec3_& operator+=(const Vec3_& o) { *this = *this + o; return *this; }
  Vec3_& operator*=(Real d) { *this = *this * d; return *this; }
  Vec3_& operator/=(Real d) { return operator*=(1.0/d); }

  Vec3_ negated() const { return {-x, -y, -z}; }
  Real dot(const Vec3_& o) const { return x*o.x + y*o.y + z*o.z; }
  Vec3_ cross(const Vec3_& o) const {
    return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
  }
  Real length_sq() const { return x * x + y * y + z * z; }
  Real length() const { return std::sqrt(length_sq()); }
  Vec3_ changed_magnitude(Real m) const { return operator*(m / length()); }
  Vec3_ normalized() const { return changed_magnitude(1.0); }
  Real dist_sq(const Vec3_& o) const { return (*this - o).length_sq(); }
  Real dist(const Vec3_& o) const { return std::sqrt(dist_sq(o)); }
  Real cos_angle(const Vec3_& o) const {
    return dot(o) / std::sqrt(length_sq() * o.length_sq());
  }
  Real angle(const Vec3_& o) const {
    return std::acos(clamp(cos_angle(o), -1., 1.));
  }
  bool approx(const Vec3_& o, Real epsilon) const {
    return std::fabs(x - o.x) <= epsilon &&
           std::fabs(y - o.y) <= epsilon &&
           std::fabs(z - o.z) <= epsilon;
  }
  bool has_nan() const {
    return std::isnan(x) || std::isnan(y) || std::isnan(z);
  }
};

using Vec3 = Vec3_<double>;
using Vec3f = Vec3_<float>;

inline Vec3 operator*(double d, const Vec3& v) { return v * d; }

/// Rodrigues' rotation formula: rotate vector v about given axis of rotation
/// (which must be a unit vector) by given angle (in radians).
inline Vec3 rotate_about_axis(const Vec3& v, const Vec3& axis, double theta) {
  double sin_theta = std::sin(theta);
  double cos_theta = std::cos(theta);
  return v * cos_theta + axis.cross(v) * sin_theta +
         axis * (axis.dot(v) * (1 - cos_theta));
}

struct Mat33 {
  double a[3][3] = { {1.,0.,0.}, {0.,1.,0.}, {0.,0.,1.} };

  // make it accessible with ".a"
  typedef double row_t[3];
  const row_t& operator[](int i) const { return a[i]; }
  row_t& operator[](int i) { return a[i]; }

  Mat33() = default;
  explicit Mat33(double d) : a{{d, d, d}, {d, d, d}, {d, d, d}} {}
  Mat33(double a1, double a2, double a3, double b1, double b2, double b3,
        double c1, double c2, double c3)
  : a{{a1, a2, a3}, {b1, b2, b3}, {c1, c2, c3}} {}

  static Mat33 from_columns(const Vec3& c1, const Vec3& c2, const Vec3& c3) {
    return Mat33(c1.x, c2.x, c3.x, c1.y, c2.y, c3.y, c1.z, c2.z, c3.z);
  }

  Vec3 row_copy(int i) const {
    if (i < 0 || i > 2)
      throw std::out_of_range("Mat33 row index must be 0, 1 or 2.");
    return Vec3(a[i][0], a[i][1], a[i][2]);
  }

  Vec3 column_copy(int i) const {
    if (i < 0 || i > 2)
      throw std::out_of_range("Mat33 column index must be 0, 1 or 2.");
    return Vec3(a[0][i], a[1][i], a[2][i]);
  }

  Mat33 operator+(const Mat33& b) const {
    return Mat33(a[0][0] + b[0][0], a[0][1] + b[0][1], a[0][2] + b[0][2],
                 a[1][0] + b[1][0], a[1][1] + b[1][1], a[1][2] + b[1][2],
                 a[2][0] + b[2][0], a[2][1] + b[2][1], a[2][2] + b[2][2]);
  }
  Mat33 operator-(const Mat33& b) const {
    return Mat33(a[0][0] - b[0][0], a[0][1] - b[0][1], a[0][2] - b[0][2],
                 a[1][0] - b[1][0], a[1][1] - b[1][1], a[1][2] - b[1][2],
                 a[2][0] - b[2][0], a[2][1] - b[2][1], a[2][2] - b[2][2]);
  }

  Vec3 multiply(const Vec3& p) const {
    return {a[0][0] * p.x + a[0][1] * p.y + a[0][2] * p.z,
            a[1][0] * p.x + a[1][1] * p.y + a[1][2] * p.z,
            a[2][0] * p.x + a[2][1] * p.y + a[2][2] * p.z};
  }
  Vec3 left_multiply(const Vec3& p) const {
    return {a[0][0] * p.x + a[1][0] * p.y + a[2][0] * p.z,
            a[0][1] * p.x + a[1][1] * p.y + a[2][1] * p.z,
            a[0][2] * p.x + a[1][2] * p.y + a[2][2] * p.z};
  }
  // p has elements from the main diagonal of a 3x3 diagonal matrix
  Mat33 multiply_by_diagonal(const Vec3& p) const {
    return Mat33(a[0][0] * p.x, a[0][1] * p.y, a[0][2] * p.z,
                 a[1][0] * p.x, a[1][1] * p.y, a[1][2] * p.z,
                 a[2][0] * p.x, a[2][1] * p.y, a[2][2] * p.z);
  }
  Mat33 multiply(const Mat33& b) const {
    Mat33 r;
    for (int i = 0; i != 3; ++i)
      for (int j = 0; j != 3; ++j)
        r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
    return r;
  }
  Mat33 transpose() const {
    return Mat33(a[0][0], a[1][0], a[2][0],
                 a[0][1], a[1][1], a[2][1],
                 a[0][2], a[1][2], a[2][2]);
  }
  double trace() const { return a[0][0] + a[1][1] + a[2][2]; }

  bool approx(const Mat33& other, double epsilon) const {
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        if (std::fabs(a[i][j] - other.a[i][j]) > epsilon)
          return false;
    return true;
  }
  bool has_nan() const {
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        if (std::isnan(a[i][j]))
            return true;
    return false;
  }

  double determinant() const {
    return a[0][0] * (a[1][1]*a[2][2] - a[2][1]*a[1][2]) +
           a[0][1] * (a[1][2]*a[2][0] - a[2][2]*a[1][0]) +
           a[0][2] * (a[1][0]*a[2][1] - a[2][0]*a[1][1]);
  }
  Mat33 inverse() const {
    Mat33 inv;
    double inv_det = 1.0 / determinant();
    inv[0][0] = inv_det * (a[1][1] * a[2][2] - a[2][1] * a[1][2]);
    inv[0][1] = inv_det * (a[0][2] * a[2][1] - a[0][1] * a[2][2]);
    inv[0][2] = inv_det * (a[0][1] * a[1][2] - a[0][2] * a[1][1]);
    inv[1][0] = inv_det * (a[1][2] * a[2][0] - a[1][0] * a[2][2]);
    inv[1][1] = inv_det * (a[0][0] * a[2][2] - a[0][2] * a[2][0]);
    inv[1][2] = inv_det * (a[1][0] * a[0][2] - a[0][0] * a[1][2]);
    inv[2][0] = inv_det * (a[1][0] * a[2][1] - a[2][0] * a[1][1]);
    inv[2][1] = inv_det * (a[2][0] * a[0][1] - a[0][0] * a[2][1]);
    inv[2][2] = inv_det * (a[0][0] * a[1][1] - a[1][0] * a[0][1]);
    return inv;
  }
  bool is_identity() const {
    return a[0][0] == 1 && a[0][1] == 0 && a[0][2] == 0 &&
           a[1][0] == 0 && a[1][1] == 1 && a[1][2] == 0 &&
           a[2][0] == 0 && a[2][1] == 0 && a[2][2] == 1;
  }

  double column_dot(int i, int j) const {
    return a[0][i] * a[0][j] + a[1][i] * a[1][j] + a[2][i] * a[2][j];
  }

  bool is_upper_triangular() const {
    return a[1][0] == 0 && a[2][0] == 0 && a[2][1] == 0;
  }
};

struct UpperTriangularMat33 {
  double a11 = 0, a12 = 0, a13 = 0;
  double          a22 = 0, a23 = 0;
  double                   a33 = 0;
  UpperTriangularMat33() = default;
  UpperTriangularMat33& operator=(const Mat33& m) {
    if (m.is_upper_triangular()) {
      a11 = m[0][0];
      a12 = m[0][1];
      a13 = m[0][2];
      a22 = m[1][1];
      a23 = m[1][2];
      a33 = m[2][2];
    } else {
      a11 = a12 = a13 = a22 = a23 = a33 = NAN;
    }
    return *this;
  }
  Vec3 multiply(const Vec3& p) const {
    return {a11 * p.x + a12 * p.y + a13 * p.z,
                        a22 * p.y + a23 * p.z,
                                    a33 * p.z};
  }
};

// Symmetric matrix 3x3. Used primarily for an ADP tensor.
template<typename T> struct SMat33 {
  T u11, u22, u33, u12, u13, u23;

  // The PDB ANISOU record has the above order, but in a different context
  // (such as metric tensor) the order of Voigt notation may be preferred.
  std::array<T, 6> elements_pdb() const   { return {{u11, u22, u33, u12, u13, u23}}; }
  std::array<T, 6> elements_voigt() const { return {{u11, u22, u33, u23, u13, u12}}; }

  Mat33 as_mat33() const {
    return Mat33(u11, u12, u13, u12, u22, u23, u13, u23, u33);
  }

  // the arguments i and j must be in [0,2], i.e. 0, 1 or 2.
  T& unchecked_ref(int i, int j) {
    T* ptrs[9] = {&u11, &u12, &u13, &u12, &u22, &u23, &u13, &u23, &u33};
    return *ptrs[3 * i + j];
  }

  T trace() const { return u11 + u22 + u33; }
  bool nonzero() const { return trace() != 0; }

  bool all_zero() const {
    return u11 == 0 && u22 == 0 && u33 == 0 && u12 == 0 && u13 == 0 && u23 == 0;
  }

  void scale(T s) const {
    u11 *= s; u22 *= s; u33 *= s; u12 *= s; u13 *= s; u23 *= s;
  }

  template<typename Real>
  SMat33<Real> scaled(Real s) const {
    return SMat33<Real>{u11*s, u22*s, u33*s, u12*s, u13*s, u23*s};
  }

  // returns U + kI
  SMat33<T> added_kI(T k) const {
    return {u11+k, u22+k, u33+k, u12, u13, u23};
  }

  // returns squared norm r^T U r where U is this matrix and vector r is arg
  template<typename VT>
  auto r_u_r(const Vec3_<VT>& r) const -> decltype(r.x+u11) {
    return r.x * r.x * u11 + r.y * r.y * u22 + r.z * r.z * u33 +
      2 * (r.x * r.y * u12 + r.x * r.z * u13 + r.y * r.z * u23);
  }
  double r_u_r(const std::array<int,3>& h) const {
    // it's faster to first convert ints to doubles (Vec3)
    return r_u_r(Vec3(h));
  }

  Vec3 multiply(const Vec3& p) const {
    return {u11 * p.x + u12 * p.y + u13 * p.z,
            u12 * p.x + u22 * p.y + u23 * p.z,
            u13 * p.x + u23 * p.y + u33 * p.z};
  }

  SMat33 operator-(const SMat33& o) const {
    return {u11-o.u11, u22-o.u22, u33-o.u33, u12-o.u12, u13-o.u13, u23-o.u23};
  }
  SMat33 operator+(const SMat33& o) const {
    return {u11+o.u11, u22+o.u22, u33+o.u33, u12+o.u12, u13+o.u13, u23+o.u23};
  }

  // return M U M^T
  template<typename Real=double>
  SMat33<Real> transformed_by(const Mat33& m) const {
    // slightly faster than m.multiply(as_mat33()).multiply(m.transpose());
    auto elem = [&](int i, int j) {
      return static_cast<Real>(
          m[i][0] * (m[j][0] * u11 + m[j][1] * u12 + m[j][2] * u13) +
          m[i][1] * (m[j][0] * u12 + m[j][1] * u22 + m[j][2] * u23) +
          m[i][2] * (m[j][0] * u13 + m[j][1] * u23 + m[j][2] * u33));
    };
    return SMat33<Real>{elem(0, 0), elem(1, 1), elem(2, 2),
                        elem(0, 1), elem(0, 2), elem(1, 2)};
  }

  T determinant() const {
    return u11 * (u22*u33 - u23*u23) +
           u12 * (u23*u13 - u33*u12) +
           u13 * (u12*u23 - u13*u22);
  }

  SMat33 inverse_(T det) const {
    SMat33 inv;
    T inv_det = 1.0f / det;
    inv.u11 = inv_det * (u22 * u33 - u23 * u23);
    inv.u22 = inv_det * (u11 * u33 - u13 * u13);
    inv.u33 = inv_det * (u11 * u22 - u12 * u12);
    inv.u12 = inv_det * (u13 * u23 - u12 * u33);
    inv.u13 = inv_det * (u12 * u23 - u13 * u22);
    inv.u23 = inv_det * (u12 * u13 - u11 * u23);
    return inv;
  }
  SMat33 inverse() const {
    return inverse_(determinant());
  }

  /// Based on https://en.wikipedia.org/wiki/Eigenvalue_algorithm
  /// To calculate both eigenvalues and eigenvectors use eig3.hpp
  std::array<double, 3> calculate_eigenvalues() const {
    double p1 = u12*u12 + u13*u13 + u23*u23;
    if (p1 == 0)
      return {{u11, u22, u33}};
    double q = (1./3.) * trace();
    SMat33<double> b{u11 - q, u22 - q, u33 - q, u12, u13, u23};
    double p2 = sq(b.u11) + sq(b.u22) + sq(b.u33) + 2 * p1;
    double p = std::sqrt((1./6.) * p2);
    double r = b.determinant() / ((1./3.) * p2 * p);
    double phi = 0;
    if (r <= -1)
      phi = (1./3.) * pi();
    else if (r < 1)
      phi = (1./3.) * std::acos(r);
    double eig1 = q + 2 * p * std::cos(phi);
    double eig3 = q + 2 * p * std::cos(phi + 2./3.*pi());
    return {{eig1, 3 * q - eig1 - eig3, eig3}};
  }
};

struct Transform {
  Mat33 mat;
  Vec3 vec;

  Transform inverse() const {
    Mat33 minv = mat.inverse();
    return {minv, minv.multiply(vec).negated()};
  }

  Vec3 apply(const Vec3& x) const { return mat.multiply(x) + vec; }

  Transform combine(const Transform& b) const {
    return {mat.multiply(b.mat), vec + mat.multiply(b.vec)};
  }

  bool is_identity() const {
    return mat.is_identity() && vec.x == 0. && vec.y == 0. && vec.z == 0.;
  }
  void set_identity() { mat = Mat33(); vec = Vec3(); }

  bool has_nan() const {
    return mat.has_nan() || vec.has_nan();
  }

  bool approx(const Transform& o, double epsilon) const {
    return mat.approx(o.mat, epsilon) && vec.approx(o.vec, epsilon);
  }
};

template<typename Pos>
struct Box {
  Pos minimum = Pos(INFINITY, INFINITY, INFINITY);
  Pos maximum = Pos(-INFINITY, -INFINITY, -INFINITY);
  void extend(const Pos& p) {
    if (p.x < minimum.x) minimum.x = p.x;
    if (p.y < minimum.y) minimum.y = p.y;
    if (p.z < minimum.z) minimum.z = p.z;
    if (p.x > maximum.x) maximum.x = p.x;
    if (p.y > maximum.y) maximum.y = p.y;
    if (p.z > maximum.z) maximum.z = p.z;
  }
  Pos get_size() const { return maximum - minimum; }
  void add_margins(const Pos& p) { minimum -= p; maximum += p; }
  void add_margin(double m) { add_margins(Pos(m, m, m)); }
};

// internally used functions
namespace impl {
// MSVC is missing isnan(IntegralType), so we define is_nan as a replacement
template<typename T>
typename std::enable_if<std::is_integral<T>::value, bool>::type
is_nan(T) { return false; }
template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_nan(T a) { return std::isnan(a); }

template<typename T>
typename std::enable_if<std::is_integral<T>::value, bool>::type
is_same(T a, T b) { return a == b; }
template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_same(T a, T b) { return std::isnan(b) ? std::isnan(a) : a == b; }
} // namespace impl

} // namespace gemmi
#endif
