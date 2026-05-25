#pragma once

#include <vector>

namespace bbsolver {

struct ShapeTangentLockStats {
  int pairs_seen = 0;
  int pairs_locked = 0;
  double max_deviation_before_deg = 0.0;
};

ShapeTangentLockStats LockShapeFlatRotationalTangents(
    std::vector<double>* value);

ShapeTangentLockStats LockShapeFlatRotationalTangents(
    std::vector<std::vector<double>>* values);

ShapeTangentLockStats LockShapeFlatRotationalTangentsExcept(
    std::vector<std::vector<double>>* values,
    const std::vector<bool>& skip_indices);

}  // namespace bbsolver
