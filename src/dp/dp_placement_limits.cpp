#include "bbsolver/dp/dp_placement_limits.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>

namespace bbsolver {

int AutoMaxGap(const CompInfo& comp, int sample_count) {
  int gap = static_cast<int>(std::round(2.0 * std::max(1.0, comp.fps)));
  if (gap < 4) {
    gap = 4;
  }
  if (gap > sample_count - 1) {
    gap = sample_count - 1;
  }
  return gap;
}

}  // namespace bbsolver
