// Copyright 2020 Global Phasing Ltd.
//
// Class Intensities that reads multi-record data from MTZ, mmCIF or XDS_ASCII
// and merges it into mean or anomalous intensities.
// It can also read merged data.

#ifndef GEMMI_INTENSIT_HPP_
#define GEMMI_INTENSIT_HPP_

#include <cassert>
#include <cstdint>      // for int8_t
#include "symmetry.hpp"
#include "unitcell.hpp"
#include "util.hpp"     // for vector_remove_if
#include "stats.hpp"    // for Correlation

namespace gemmi {

struct Binner;
struct Mtz;
struct XdsAscii;
struct ReflnBlock;
namespace cif { struct Block; }
using std::int8_t;

// If used to request a particular data type:
//   MergedMA = Mean if available, otherwise Anomalous,
//   MergedAM = Anomalous if available, otherwise Mean.
//   UAM = Unmerged if available, otherwise MergedAM
enum class DataType { Unknown, Unmerged, Mean, Anomalous,
                      MergedMA, MergedAM, UAM };

struct MergingR {
  int all_refl = 0;
  int unique_refl = 0;
  double r_merge_num = 0;  // numerator
  double r_meas_num = 0;
  double r_pim_num = 0;
  double intensity_sum = 0;  // denominator

  double r_merge() const { return r_merge_num / intensity_sum; }
  double r_meas() const { return r_meas_num / intensity_sum; }
  double r_pim() const { return r_pim_num / intensity_sum; }

  void add(double r_merge_num_, int nobs, double intensity_sum_) {
    all_refl += nobs;
    unique_refl += 1;
    if (nobs > 1) { // for nobs==1, r_merge_num_ must be 0
      r_merge_num += r_merge_num_;
      double t = r_merge_num_ / std::sqrt(nobs - 1);
      r_pim_num += t;
      r_meas_num += std::sqrt(nobs) * t;
    }
    intensity_sum += intensity_sum_;
  }

  void add_other(const MergingR& o) {
    all_refl += o.all_refl;
    unique_refl += o.unique_refl;
    r_merge_num += o.r_merge_num;
    r_meas_num += o.r_meas_num;
    r_pim_num += o.r_pim_num;
    intensity_sum += o.intensity_sum;
  }
};

/// Returns STARANISO version or empty string.
GEMMI_DLL std::string read_staraniso_b_from_mtz(const Mtz& mtz, SMat33<double>& output);

struct GEMMI_DLL Intensities {
  struct Refl {
    Miller hkl;
    int8_t isign;  // 1 for I(+), -1 for I(-), 0 for mean or unmerged
    int8_t isym;   // for unmerged data: encodes symmetry op like M/ISYM in MTZ
    short nobs;
    double value;
    double sigma;

    bool operator<(const Refl& o) const {
      return std::tie(hkl[0], hkl[1], hkl[2], isign) <
             std::tie(o.hkl[0], o.hkl[1], o.hkl[2], o.isign);
    }
    // for merged data
    const char* intensity_label() const {
      if (isign == 0)
        return "<I>";
      return isign > 0 ? "I(+)" : "I(-)";
    }
    std::string hkl_label() const {
      return cat(intensity_label(), " (", hkl[0], ' ', hkl[1], ' ', hkl[2], ')');
    }
  };

  struct AnisoScaling {
    SMat33<double> b = {0., 0., 0., 0., 0., 0.};

    bool ok() const { return !b.all_zero(); }
    double scale(const Miller& hkl, const UnitCell& cell) const {
      Vec3 s = cell.frac.mat.left_multiply(Vec3(hkl[0], hkl[1], hkl[2]));
      return std::exp(0.5 * b.r_u_r(s));
    }
  };

  std::vector<Refl> data;
  const SpaceGroup* spacegroup = nullptr;
  UnitCell unit_cell;
  double unit_cell_rmsd[6] = {0., 0., 0., 0., 0., 0.};
  double wavelength;
  DataType type = DataType::Unknown;
  std::vector<Op> isym_ops;
  AnisoScaling staraniso_b;

  static const char* type_str(DataType data_type) {
    switch (data_type) {
      case DataType::Unmerged: return "I";
      case DataType::Mean: return "<I>";
      case DataType::Anomalous: return "I+/I-";
      case DataType::MergedAM:
      case DataType::MergedMA:
      case DataType::UAM:
      case DataType::Unknown: return "n/a";
    }
    unreachable();
  }

  const char* type_str() const { return Intensities::type_str(type); }

  std::string spacegroup_str() const { return spacegroup ? spacegroup->xhm() : "none"; }

  // returns (d_max, d_min)
  std::array<double,2> resolution_range() const;

  // pre: both are sorted
  Correlation calculate_correlation(const Intensities& other) const;

  void add_if_valid(const Miller& hkl, int8_t isign, int8_t isym, double value, double sigma) {
    // XDS marks rejected reflections with negative sigma.
    // Sigma 0.0 rarely happens (e.g. 5tkn), but is also problematic.
    if (!std::isnan(value) && sigma > 0)
      data.push_back({hkl, isign, isym, /*nobs=*/0, value, sigma});
  }

  void remove_systematic_absences() {
    if (!spacegroup)
      return;
    GroupOps gops = spacegroup->operations();
    vector_remove_if(data, [&](Refl& x) { return gops.is_systematically_absent(x.hkl); });
  }

  void sort() { std::sort(data.begin(), data.end()); }

  void merge_in_place(DataType data_type);

  std::vector<MergingR> calculate_merging_rs(const Binner* binner) const;

  void switch_to_asu_indices();

  void read_unmerged_intensities_from_mtz(const Mtz& mtz);
  void read_mean_intensities_from_mtz(const Mtz& mtz);
  // with check_complete=true, throw if anomalous data is null where it shouldn't be
  void read_anomalous_intensities_from_mtz(const Mtz& mtz, bool check_complete=false);

  void read_mtz(const Mtz& mtz, DataType data_type);

  void read_unmerged_intensities_from_mmcif(const ReflnBlock& rb);
  void read_mean_intensities_from_mmcif(const ReflnBlock& rb);
  void read_anomalous_intensities_from_mmcif(const ReflnBlock& rb, bool check_complete=false);

  void read_f_squared_from_mmcif(const ReflnBlock& rb);

  void read_mmcif(const ReflnBlock& rb, DataType data_type);

  void read_xds(const XdsAscii& xds);

  // returns STARANISO version or empty string
  std::string take_staraniso_b_from_mtz(const Mtz& mtz);

  bool take_staraniso_b_from_mmcif(const cif::Block& block);

  Mtz prepare_merged_mtz(bool with_nobs);
};

// Minimal compatibility with MtzDataProxy and ReflnDataProxy.
struct IntensitiesDataProxy {
  const Intensities& intensities_;
  size_t stride() const { return 1; }
  size_t size() const { return intensities_.data.size(); }
  const SpaceGroup* spacegroup() const { return intensities_.spacegroup; }
  const UnitCell& unit_cell() const { return intensities_.unit_cell; }
  Miller get_hkl(size_t offset) const { return intensities_.data[offset].hkl; }
  double get_num(size_t n) const { return intensities_.data[n].value; }
};

template<typename DataProxy>
std::pair<DataType, size_t> check_data_type_under_symmetry(const DataProxy& proxy) {
  const SpaceGroup* sg = proxy.spacegroup();
  if (!sg)
    return {DataType::Unknown, 0};
  std::unordered_map<Op::Miller, int, MillerHash> seen;
  ReciprocalAsu asu(sg);
  GroupOps gops = sg->operations();
  bool centric = gops.is_centrosymmetric();
  DataType data_type = DataType::Mean;
  for (size_t i = 0; i < proxy.size(); i += proxy.stride()) {
    auto hkl_sign = asu.to_asu_sign(proxy.get_hkl(i), gops);
    int sign = hkl_sign.second ? 2 : 1;  // 2=positive, 1=negative
    auto r = seen.emplace(hkl_sign.first, sign);
    if (data_type != DataType::Unmerged && !r.second) {
      if ((r.first->second & sign) != 0 || centric) {
        data_type = DataType::Unmerged;
      } else {
        r.first->second |= sign;
        data_type = DataType::Anomalous;
      }
    }
  }
  return {data_type, seen.size()};
}

} // namespace gemmi
#endif
