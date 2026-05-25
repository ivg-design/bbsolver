#include "bbsolver/motion_smooth/motion_smooth_shape_loop.hpp"

#include <cstddef>
#include <vector>

namespace bbsolver {

std::vector<double> EvenTimesForValueCount(double start_t,
                                           double end_t,
                                           std::size_t count) {
  std::vector<double> times;
  if (count == 0) {
    return times;
  }
  times.reserve(count);
  if (count == 1 || end_t <= start_t) {
    times.push_back(start_t);
    return times;
  }
  const double duration = end_t - start_t;
  for (std::size_t i = 0; i < count; ++i) {
    times.push_back(
        start_t + duration * static_cast<double>(i) /
                      static_cast<double>(count - 1));
  }
  return times;
}

}  // namespace bbsolver
