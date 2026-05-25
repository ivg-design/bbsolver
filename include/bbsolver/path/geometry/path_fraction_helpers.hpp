#pragma once

// Shared outline-fraction array utilities used by the path_frame_fit family.
// Outline fractions are normalized arc-length positions in source seam order:
// [0, 1) for closed paths, [0, 1] for open paths. The helpers below understand
// that seam-wraparound semantics differ between closed and open and they all
// share the same kFractionEpsilon tolerance for deduplication and ordering.
//
// Pure leaf: no DiagnosticsWriter, no progress, no acceptance state — only
// fraction arithmetic. These helpers previously lived in
// path_frame_fit.cpp's anonymous namespace and had 4–10 call sites each
// across the file; PFF2 promotes them so canonical-layout and outline-fraction
// expansion can be extracted in subsequent slices without duplicating the
// seam-wraparound logic.
//
// Usage from a .cpp:
//   namespace bbsolver { namespace { using namespace pff_fractions; ... } }
// keeps existing unqualified call sites compiling unchanged.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace bbsolver {
namespace pff_fractions {

// Minimum gap between adjacent fractions; also the snap-to-zero tolerance for
// closed-path wraparound. 1e-6 of unit perimeter; smaller than the smallest
// AE-meaningful sub-frame increment.
constexpr double kFractionEpsilon = 1e-6;

inline bool FractionsInStrictSeamOrder(const std::vector<double>& fractions, bool closed) {
  if (fractions.empty()) {
    return false;
  }
  double previous = -std::numeric_limits<double>::infinity();
  for (double fraction : fractions) {
    if (!std::isfinite(fraction)) {
      return false;
    }
    if (closed) {
      if (fraction < 0.0 || fraction >= 1.0) {
        return false;
      }
    } else if (fraction < 0.0 || fraction > 1.0) {
      return false;
    }
    if (fraction <= previous + kFractionEpsilon * 0.001) {
      return false;
    }
    previous = fraction;
  }
  return true;
}

inline int FractionSegmentCount(const std::vector<double>& fractions, bool closed) {
  return closed ? static_cast<int>(fractions.size())
                : std::max(0, static_cast<int>(fractions.size()) - 1);
}

inline double FractionGap(const std::vector<double>& fractions, int segment_index, bool closed) {
  if (segment_index < 0 || segment_index >= FractionSegmentCount(fractions, closed)) {
    return 0.0;
  }
  const int n = static_cast<int>(fractions.size());
  const int next = segment_index + 1 < n ? segment_index + 1 : 0;
  double end = fractions[static_cast<std::size_t>(next)];
  if (closed && next == 0) {
    end += 1.0;
  }
  return std::max(0.0, end - fractions[static_cast<std::size_t>(segment_index)]);
}

inline std::vector<int> FractionSegmentsByDescendingGap(const std::vector<double>& fractions,
                                                        bool closed,
                                                        int max_segments) {
  const int segment_count = FractionSegmentCount(fractions, closed);
  std::vector<int> segments;
  segments.reserve(static_cast<std::size_t>(segment_count));
  for (int i = 0; i < segment_count; ++i) {
    segments.push_back(i);
  }
  std::stable_sort(segments.begin(), segments.end(), [&](int a, int b) {
    return FractionGap(fractions, a, closed) > FractionGap(fractions, b, closed);
  });
  if (max_segments > 0 && max_segments < static_cast<int>(segments.size())) {
    segments.resize(static_cast<std::size_t>(max_segments));
  }
  return segments;
}

inline bool InsertFractionValue(std::vector<double>* fractions, double fraction, bool closed) {
  if (fractions == nullptr || !std::isfinite(fraction)) {
    return false;
  }
  double value = fraction;
  if (closed) {
    value = value - std::floor(value);
    if (value >= 1.0 - kFractionEpsilon || value < kFractionEpsilon) {
      value = 0.0;
    }
  } else {
    if (value < -kFractionEpsilon || value > 1.0 + kFractionEpsilon) {
      return false;
    }
    value = std::clamp(value, 0.0, 1.0);
  }
  for (double existing : *fractions) {
    if (std::abs(existing - value) <= kFractionEpsilon) {
      return false;
    }
  }
  auto it = std::upper_bound(fractions->begin(), fractions->end(), value);
  fractions->insert(it, value);
  return FractionsInStrictSeamOrder(*fractions, closed);
}

inline void InsertSplitFraction(std::vector<double>* fractions, int segment_index, bool closed) {
  if (fractions == nullptr || fractions->empty() ||
      segment_index < 0 || segment_index >= static_cast<int>(fractions->size())) {
    return;
  }
  const int n = static_cast<int>(fractions->size());
  const int next = segment_index + 1 < n ? segment_index + 1 : 0;
  double end_fraction = (*fractions)[static_cast<std::size_t>(next)];
  if (closed && next == 0) {
    end_fraction += 1.0;
  }
  const double start_fraction = (*fractions)[static_cast<std::size_t>(segment_index)];
  double mid_fraction = (start_fraction + end_fraction) * 0.5;
  if (closed && mid_fraction >= 1.0) {
    mid_fraction -= 1.0;
  }
  fractions->insert(fractions->begin() + segment_index + 1, mid_fraction);
}

inline double FractionDistance(double a, double b, bool closed) {
  const double direct = std::abs(a - b);
  return closed ? std::min(direct, 1.0 - direct) : direct;
}

// Validate-and-clamp a caller-supplied outline-fraction layout into the strict
// seam-ordered form the fitters expect: closed paths wrap into [0, 1) with
// the seam at fraction 0; open paths clamp into [0, 1]. Returns false (with
// *normalized cleared up to that point) if any value is non-finite or violates
// the seam-ordered, deduplicated, in-range contract.
inline bool NormalizeOutlineFractions(const std::vector<double>& input,
                                      bool closed,
                                      std::vector<double>* normalized) {
  normalized->clear();
  normalized->reserve(input.size());
  double previous = -std::numeric_limits<double>::infinity();
  for (double f : input) {
    if (!std::isfinite(f)) {
      return false;
    }
    double value = f;
    if (closed) {
      value = value - std::floor(value);
      if (value >= 1.0 - kFractionEpsilon || value < kFractionEpsilon) {
        value = 0.0;
      }
    } else {
      if (value < -kFractionEpsilon || value > 1.0 + kFractionEpsilon) {
        return false;
      }
      value = std::clamp(value, 0.0, 1.0);
    }
    if (!normalized->empty() && value <= previous + kFractionEpsilon) {
      return false;
    }
    normalized->push_back(value);
    previous = value;
  }
  return true;
}

inline int LargestFractionGapSegment(const std::vector<double>& fractions, bool closed) {
  const int segment_count = FractionSegmentCount(fractions, closed);
  int best_segment = -1;
  double best_gap = -1.0;
  for (int segment = 0; segment < segment_count; ++segment) {
    const double gap = FractionGap(fractions, segment, closed);
    if (gap > best_gap) {
      best_gap = gap;
      best_segment = segment;
    }
  }
  return best_segment;
}

}  // namespace pff_fractions
}  // namespace bbsolver
