#include "bbsolver/solve/solver_reporting.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/routing/property_classification.hpp"

#include <algorithm>
#include <ostream>
#include <string>

namespace bbsolver {

void AppendSolverNote(PropertyKeys& keys, const std::string& note) {
  if (note.empty()) {
    return;
  }
  if (!keys.notes.empty() && keys.notes.find(note) != std::string::npos) {
    return;
  }
  AppendJoinedNote(keys.notes, note);
}

void AppendJoinedNote(std::string& notes, const std::string& note) {
  if (note.empty()) {
    return;
  }
  notes = notes.empty() ? note : notes + "; " + note;
}

void AppendSampleTimingNote(std::string& note,
                            int keys_count,
                            int preserved_timing_count) {
  if (keys_count <= 0) {
    return;
  }
  if (preserved_timing_count == keys_count) {
    note += "; source_key_timing_preserved=true";
  } else if (preserved_timing_count > 0) {
    note += "; source_key_timing_preserved_partial=" +
            std::to_string(preserved_timing_count) + "/" +
            std::to_string(keys_count);
  } else {
    note += "; source_key_timing_missing=true";
  }
}

std::string AccuracyGateOptimizationNote(
    const PropertySamples& source_samples,
    const SolverConfig& config,
    const PropertyKeys& keys) {
  if (keys.notes.find("cancelled") != std::string::npos) {
    return {};
  }
  if (!keys.converged) {
    return {};
  }
  const int source_count = static_cast<int>(source_samples.samples.size());
  const int key_count = static_cast<int>(keys.keys.size());
  if (source_count < 4 || key_count <= 0) {
    return {};
  }
  if (key_count * 10 < source_count * 9) {
    return {};
  }

  const bool no_practical_reduction = key_count >= source_count;
  const int reduction = std::max(0, source_count - key_count);
  const double reduction_pct =
      100.0 * static_cast<double>(reduction) /
      static_cast<double>(std::max(1, source_count));
  std::string note = no_practical_reduction
      ? "no_practical_optimization_at_accuracy_gate=true"
      : "marginal_optimization_at_accuracy_gate=true";
  note +=
      "; optimization_blocker=accuracy_gate"
      "; current_tolerance=" + std::to_string(config.tolerance) +
      "; current_screen_px=" + std::to_string(config.tolerance_screen_px) +
      "; output_keys=" + std::to_string(key_count) +
      "; source_samples=" + std::to_string(source_count) +
      "; key_reduction=" + std::to_string(reduction) +
      "; key_reduction_percent=" + std::to_string(reduction_pct) +
      "; suggestion=increase_tolerance_or_screen_px_to_attempt_more_reduction";
  return note;
}

void WarnUnifiedSpatialPropertyIfNeeded(const PropertySamples& property_samples,
                                        std::ostream& out) {
  if (!IsUnseparatedSpatial(property_samples)) {
    return;
  }
  out << "bbsolver: warning: spatial property '"
      << property_samples.property.id
      << "' uses unified AE spatial fitting; keep sample ranges bounded for "
         "interactive bakes\n";
}

void ApplyFinalStaticTrimNote(PropertyKeys& keys, const std::string& note) {
  if (note.empty()) {
    return;
  }
  if (!keys.keys.empty()) {
    keys.keys.back().interp_out = InterpType::Hold;
  }
  AppendSolverNote(keys, note);
}

}  // namespace bbsolver
