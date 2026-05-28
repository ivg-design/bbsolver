// Implements bbsolver::ExpandShapeFlatOutlineFractions (declared in
// path_frame_fit.hpp). Given a seed canonical outline-fraction layout and a
// set of source frames, this lane greedily densifies the layout by inserting
// midpoint slots between adjacent fractions and replay-tests each candidate
// against every frame via EvaluateFractionLayout. Insertions are accepted
// only when they do not worsen the worst-frame error (or improve it by at
// least min_error_improvement when caller-specified). Loop bounds are
// max_fraction_count and max_insertions; the loop exits early when the
// tolerance is met.
//
// Diagnostics decision: **none / pure layout**. This is an acceptance-style
// helper: it returns a PathFractionExpansionResult whose `warning` field
// carries human-readable status strings (`"no shape_flat frames"`,
// `"malformed shape_flat frame"`, `"mixed open/closed shape_flat frames"`,
// `"invalid outline fractions"`, `"insufficient outline fractions"`,
// `"closed outline fractions must start at source seam"`, `"open outline
// fractions must include endpoints"`, `"outline fraction count reaches source
// vertex count"`, `"fraction expansion has no remaining slot budget"`,
// `"fraction expansion found no candidate gaps"`, `"fraction expansion found
// no replayable candidate"`, `"fraction expansion found no improving
// candidate"`, `"fraction expansion reached max_fraction_count"`). Nothing
// here emits a DiagnosticsWriter event, touches progress / cancellation, or
// changes operator state; callers route the warning string through their
// own observability path if they want.
//
// Extracted from path_frame_fit.cpp without algorithmic change: the function
// body moves byte-faithfully; every literal string, every loop bound, every
// epsilon (1e-9 tolerance slack, kFractionEpsilon for seam dedup) is
// preserved exactly.

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "bbsolver/path/geometry/path_fraction_helpers.hpp"
#include "bbsolver/path/fit/path_fraction_layout_eval.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {

PathFractionExpansionResult ExpandShapeFlatOutlineFractions(
    const std::vector<std::vector<double>>& shape_flat_frames,
    const std::vector<double>& seed_fractions,
    const PathFrameFitOptions& fit_options,
    const PathFractionExpansionOptions& expansion_options) {
  using pff_fractions::FractionSegmentsByDescendingGap;
  using pff_fractions::FractionsInStrictSeamOrder;
  using pff_fractions::InsertFractionValue;
  using pff_fractions::InsertSplitFraction;
  using pff_fractions::NormalizeOutlineFractions;
  using pff_fractions::kFractionEpsilon;
  using pff_geom::DecodedShape;
  using pff_geom::DecodeShapeFlat;

  PathFractionExpansionResult result;
  result.initial_fraction_count = static_cast<int>(seed_fractions.size());
  result.final_fraction_count = result.initial_fraction_count;
  result.outline_fractions = seed_fractions;

  if (shape_flat_frames.empty()) {
    result.warning = "no shape_flat frames";
    return result;
  }

  const DecodedShape first_decoded = DecodeShapeFlat(shape_flat_frames.front());
  if (!first_decoded.ok) {
    result.warning = "malformed shape_flat frame";
    return result;
  }
  result.closed = first_decoded.closed;

  int min_source_vertices = first_decoded.vertex_count;
  for (const std::vector<double>& frame: shape_flat_frames) {
    const DecodedShape decoded = DecodeShapeFlat(frame);
    if (!decoded.ok) {
      result.warning = "malformed shape_flat frame";
      return result;
    }
    if (decoded.closed != result.closed) {
      result.warning = "mixed open/closed shape_flat frames";
      return result;
    }
    min_source_vertices = std::min(min_source_vertices, decoded.vertex_count);
  }

  std::vector<double> current;
  if (!NormalizeOutlineFractions(seed_fractions, result.closed, &current)) {
    result.warning = "invalid outline fractions";
    return result;
  }

  const int min_fractions = result.closed ? 3: 2;
  if (static_cast<int>(current.size()) < min_fractions) {
    result.warning = "insufficient outline fractions";
    return result;
  }
  if (result.closed && std::abs(current.front()) > kFractionEpsilon) {
    result.warning = "closed outline fractions must start at source seam";
    return result;
  }
  if (!result.closed &&
      (std::abs(current.front()) > kFractionEpsilon ||
       std::abs(current.back() - 1.0) > kFractionEpsilon)) {
    result.warning = "open outline fractions must include endpoints";
    return result;
  }
  if (static_cast<int>(current.size()) >= min_source_vertices) {
    result.warning = "outline fraction count reaches source vertex count";
    return result;
  }

  int max_fraction_count = std::max(0, min_source_vertices - 1);
  if (expansion_options.max_fraction_count > 0) {
    max_fraction_count = std::min(max_fraction_count, expansion_options.max_fraction_count);
  }
  if (expansion_options.max_insertions > 0) {
    max_fraction_count = std::min(
        max_fraction_count,
        static_cast<int>(current.size()) + expansion_options.max_insertions);
  }
  result.max_fraction_count = max_fraction_count;

  FractionLayoutEvaluation current_eval =
      EvaluateFractionLayout(shape_flat_frames, current, fit_options);
  if (!current_eval.ok) {
    result.warning = current_eval.warning;
    return result;
  }

  const double tolerance = std::max(fit_options.outline_tolerance, 0.0);
  result.ok = true;
  result.outline_fractions = current;
  result.final_fraction_count = static_cast<int>(current.size());
  result.initial_max_outline_error = current_eval.max_error;
  result.final_max_outline_error = current_eval.max_error;
  result.tolerance_met = current_eval.max_error <= tolerance + 1e-9;
  if (result.tolerance_met) {
    return result;
  }
  if (max_fraction_count <= static_cast<int>(current.size())) {
    result.warning = "fraction expansion has no remaining slot budget";
    return result;
  }

  const double min_improvement = std::max(0.0, expansion_options.min_error_improvement);
  while (static_cast<int>(current.size()) < max_fraction_count) {
    std::vector<int> segments = FractionSegmentsByDescendingGap(
        current, result.closed, expansion_options.max_candidate_gaps_per_pass);
    if (segments.empty()) {
      result.warning = "fraction expansion found no candidate gaps";
      break;
    }

    bool found_candidate = false;
    std::vector<double> best_fractions;
    FractionLayoutEvaluation best_eval;
    if (current_eval.has_worst_fraction) {
      std::vector<double> candidate = current;
      if (InsertFractionValue(&candidate, current_eval.worst_fraction, result.closed)) {
        FractionLayoutEvaluation candidate_eval =
            EvaluateFractionLayout(shape_flat_frames, candidate, fit_options);
        ++result.candidate_evaluations;
        if (candidate_eval.ok) {
          found_candidate = true;
          best_fractions = std::move(candidate);
          best_eval = std::move(candidate_eval);
        }
      }
    }
    for (int segment_index: segments) {
      std::vector<double> candidate = current;
      InsertSplitFraction(&candidate, segment_index, result.closed);
      if (!FractionsInStrictSeamOrder(candidate, result.closed)) {
        continue;
      }

      FractionLayoutEvaluation candidate_eval =
          EvaluateFractionLayout(shape_flat_frames, candidate, fit_options);
      ++result.candidate_evaluations;
      if (!candidate_eval.ok) {
        continue;
      }
      if (!found_candidate || candidate_eval.max_error < best_eval.max_error) {
        found_candidate = true;
        best_fractions = std::move(candidate);
        best_eval = std::move(candidate_eval);
      }
    }

    if (!found_candidate) {
      result.warning = "fraction expansion found no replayable candidate";
      break;
    }

    const bool acceptable =
        min_improvement > 0.0
            ? best_eval.max_error <= current_eval.max_error - min_improvement
: best_eval.max_error <= current_eval.max_error + 1e-9;
    if (!acceptable) {
      result.warning = "fraction expansion found no improving candidate";
      break;
    }

    current = std::move(best_fractions);
    current_eval = std::move(best_eval);
    ++result.insertions;
    result.applied = true;
    result.outline_fractions = current;
    result.final_fraction_count = static_cast<int>(current.size());
    result.final_max_outline_error = current_eval.max_error;
    result.tolerance_met = current_eval.max_error <= tolerance + 1e-9;
    if (result.tolerance_met) {
      result.warning.clear();
      break;
    }
  }

  if (!result.tolerance_met && result.warning.empty() &&
      static_cast<int>(current.size()) >= max_fraction_count) {
    result.warning = "fraction expansion reached max_fraction_count";
  }
  return result;
}

}  // namespace bbsolver
