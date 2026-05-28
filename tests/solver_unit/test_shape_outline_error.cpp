#include "bbsolver/metrics/error_metrics.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <cassert>
#include <limits>
#include <vector>

namespace {

std::vector<double> ShapeFlat(bool closed,
                              const std::vector<std::vector<double>>& vertices) {
  std::vector<double> out;
  out.push_back(closed ? 1.0: 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const std::vector<double>& vertex: vertices) {
    out.push_back(vertex[0]);
    out.push_back(vertex[1]);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

std::vector<std::vector<double>> Translated(
    const std::vector<std::vector<double>>& vertices,
    double dx,
    double dy) {
  std::vector<std::vector<double>> out = vertices;
  for (std::vector<double>& vertex: out) {
    vertex[0] += dx;
    vertex[1] += dy;
  }
  return out;
}

}  // namespace

int main() {
  const std::vector<std::vector<double>> redundant_square{
      {0.0, 0.0}, {50.0, 0.0}, {100.0, 0.0}, {100.0, 50.0},
      {100.0, 100.0}, {50.0, 100.0}, {0.0, 100.0}, {0.0, 50.0},
  };
  const std::vector<std::vector<double>> fitted_square{
      {0.0, 0.0}, {100.0, 0.0}, {100.0, 100.0}, {0.0, 100.0},
  };

  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 8;
  ps.samples_per_frame = 1;
  ps.samples.push_back({0.0, ShapeFlat(true, redundant_square)});

  bbsolver::SolverConfig cfg;
  cfg.tolerance = 0.01;
  bbsolver::CompInfo comp;

  const bbsolver::ErrorReport report = bbsolver::ComputeError(
      ps,
      0,
      0,
      [&](double) { return ShapeFlat(true, fitted_square); },
      cfg,
      comp);

  assert(report.max_err <= 1e-9);
  assert(report.rms_err <= 1e-9);

  bbsolver::SolverConfig screen_cfg = cfg;
  screen_cfg.tolerance_screen_px = 1.0;
  bbsolver::LayerXform layer_xform;
  layer_xform.position = {10.0, 20.0};
  layer_xform.scale = {100.0, 100.0};
  const bbsolver::ErrorReport screen_report = bbsolver::ComputeError(
      ps,
      0,
      0,
      [&](double) { return ShapeFlat(true, fitted_square); },
      screen_cfg,
      comp,
      &layer_xform);

  assert(screen_report.max_err <= 1e-9);
  assert(screen_report.max_err_screen_px <= 1e-9);

  const std::vector<double> source_square = ShapeFlat(true, redundant_square);
  const std::vector<double> shifted_square =
      ShapeFlat(true, Translated(fitted_square, 10.0, 0.0));
  const double exact_outline_error =
      bbsolver::ShapeFlatFrameOutlineError(source_square, shifted_square);
  assert(exact_outline_error > 0.0);
  assert(bbsolver::ShapeFlatFrameOutlineError(
             source_square, shifted_square, {}, exact_outline_error + 1.0) ==
         exact_outline_error);
  assert(bbsolver::ShapeFlatFrameOutlineError(
             source_square, shifted_square, {}, exact_outline_error * 0.5) >
         exact_outline_error * 0.5);

  const bbsolver::ErrorReport exact_match_report = bbsolver::ComputeError(
      ps,
      0,
      0,
      [&](double) { return ShapeFlat(true, redundant_square); },
      cfg,
      comp,
      nullptr,
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::infinity(),
      true);

  assert(exact_match_report.max_err == 0.0);
  assert(exact_match_report.rms_err == 0.0);
  assert(exact_match_report.units_evaluated == 1);
  assert(exact_match_report.shape_outline_wall_ms == 0.0);

  bbsolver::PropertySamples malformed_ps;
  malformed_ps.property.id = "unit/path/malformed";
  malformed_ps.property.kind = bbsolver::ValueKind::Custom;
  malformed_ps.property.units_label = "shape_flat";
  malformed_ps.property.dimensions = 3;
  malformed_ps.samples_per_frame = 1;
  malformed_ps.samples.push_back({0.0, {1.0, 8.0, 0.0}});
  const bbsolver::ErrorReport malformed_exact_report = bbsolver::ComputeError(
      malformed_ps,
      0,
      0,
      [&](double) { return std::vector<double>{1.0, 8.0, 0.0}; },
      cfg,
      comp,
      nullptr,
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::infinity(),
      true);
  assert(std::isinf(malformed_exact_report.max_err));
  assert(malformed_exact_report.units_evaluated == 1);

  const bbsolver::ErrorReport malformed_actual_report = bbsolver::ComputeError(
      ps,
      0,
      0,
      [&](double) { return std::vector<double>{1.0, 8.0, 0.0}; },
      cfg,
      comp);
  assert(std::isinf(malformed_actual_report.max_err));

  const bbsolver::ErrorReport closed_mismatch_report = bbsolver::ComputeError(
      ps,
      0,
      0,
      [&](double) { return ShapeFlat(false, fitted_square); },
      cfg,
      comp);
  assert(std::isinf(closed_mismatch_report.max_err));

  bbsolver::PropertySamples long_ps;
  long_ps.property.id = "unit/path/long";
  long_ps.property.kind = bbsolver::ValueKind::Custom;
  long_ps.property.units_label = "shape_flat";
  long_ps.property.dimensions = 2 + 6 * 8;
  long_ps.samples_per_frame = 1;
  for (int sample_idx = 0; sample_idx < 240; ++sample_idx) {
    const double x = static_cast<double>(sample_idx) * 0.25;
    long_ps.samples.push_back(
        {static_cast<double>(sample_idx) / 60.0,
         ShapeFlat(true, Translated(redundant_square, x, 0.0))});
  }

  bbsolver::SolverConfig serial_cfg = cfg;
  serial_cfg.parallel_jobs = 1;
  bbsolver::SolverConfig parallel_cfg = cfg;
  parallel_cfg.parallel_jobs = 8;
  const auto translated_reconstruct = [&](double t) {
    return ShapeFlat(true, Translated(fitted_square, t * 60.0 * 0.25 + 0.5, 0.0));
  };
  const bbsolver::ErrorReport serial_report = bbsolver::ComputeError(
      long_ps,
      0,
      static_cast<int>(long_ps.samples.size()) - 1,
      translated_reconstruct,
      serial_cfg,
      comp);
  const bbsolver::ErrorReport parallel_report = bbsolver::ComputeError(
      long_ps,
      0,
      static_cast<int>(long_ps.samples.size()) - 1,
      translated_reconstruct,
      parallel_cfg,
      comp);

  assert(serial_report.max_err > 0.0);
  assert(serial_report.max_err == parallel_report.max_err);
  assert(serial_report.max_err_screen_px == parallel_report.max_err_screen_px);
  assert(serial_report.rms_err == parallel_report.rms_err);
  assert(serial_report.worst_sample_idx == parallel_report.worst_sample_idx);
  return 0;
}
