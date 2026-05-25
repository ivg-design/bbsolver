#include "bbsolver/path/replacement/path_replacement_seed_selection.hpp"
#include "bbsolver/path/replacement/path_replacement_fraction_layout.hpp"

#include <cstddef>
#include <vector>

namespace bbsolver {

std::vector<int> SelectReplacementPhase2SeedIndices(
    const std::vector<ReplacementFrameFitRecord>& records,
    int target_vertices) {
  std::vector<int> seed_indices;
  auto push_unique = [&](int idx) {
    if (idx < 0 || idx >= static_cast<int>(records.size())) {
      return;
    }
    if (static_cast<int>(records[static_cast<std::size_t>(idx)]
                             .outline_fractions.size()) != target_vertices) {
      return;
    }
    for (int existing : seed_indices) {
      if (existing == idx) {
        return;
      }
    }
    seed_indices.push_back(idx);
  };

  for (int i = 0; i < static_cast<int>(records.size()); ++i) {
    if (static_cast<int>(records[static_cast<std::size_t>(i)]
                             .outline_fractions.size()) == target_vertices) {
      push_unique(i);
      break;
    }
  }
  push_unique(static_cast<int>(records.size()) / 2);
  push_unique(static_cast<int>(records.size()) - 1);
  int max_err_idx = 0;
  for (int i = 1; i < static_cast<int>(records.size()); ++i) {
    if (records[static_cast<std::size_t>(i)].max_outline_error >
        records[static_cast<std::size_t>(max_err_idx)].max_outline_error) {
      max_err_idx = i;
    }
  }
  push_unique(max_err_idx);
  return seed_indices;
}

}  // namespace bbsolver
