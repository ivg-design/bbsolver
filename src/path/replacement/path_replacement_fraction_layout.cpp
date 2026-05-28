#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace bbsolver {

std::vector<double> BuildMedianStableFractionLayout(
    const std::vector<ReplacementFrameFitRecord>& records,
    int target_vertices) {
  if (target_vertices <= 0 || records.empty()) {
    return {};
  }
  for (const ReplacementFrameFitRecord& record: records) {
    if (static_cast<int>(record.outline_fractions.size()) != target_vertices) {
      return {};
    }
  }

  std::vector<double> fractions(static_cast<std::size_t>(target_vertices), 0.0);
  std::vector<double> values;
  values.reserve(records.size());
  for (int k = 0; k < target_vertices; ++k) {
    values.clear();
    for (const ReplacementFrameFitRecord& record: records) {
      const double fraction = record.outline_fractions[static_cast<std::size_t>(k)];
      if (!std::isfinite(fraction)) {
        return {};
      }
      values.push_back(std::clamp(fraction, 0.0, 1.0));
    }
    const std::size_t mid = values.size() / 2;
    std::nth_element(values.begin(),
                     values.begin() + static_cast<std::ptrdiff_t>(mid),
                     values.end());
    double median = values[mid];
    if (values.size() % 2 == 0 && mid > 0) {
      const double upper = median;
      std::nth_element(values.begin(),
                       values.begin() + static_cast<std::ptrdiff_t>(mid - 1),
                       values.end());
      median = 0.5 * (values[mid - 1] + upper);
    }
    fractions[static_cast<std::size_t>(k)] = median;
  }

  // Keep the source seam pinned. Phase-2 fraction layouts preserve vertex 0 as
  // the seam; the median should not let tiny numeric drift move that anchor.
  fractions.front() = 0.0;
  constexpr double kMinGap = 1e-6;
  if (fractions.back() >= 1.0 - kMinGap) {
    fractions.back() = 1.0 - kMinGap;
  }
  for (int k = 1; k < target_vertices; ++k) {
    if (!(fractions[static_cast<std::size_t>(k)] >
          fractions[static_cast<std::size_t>(k - 1)] + kMinGap)) {
      return {};
    }
  }
  return fractions;
}

}  // namespace bbsolver
