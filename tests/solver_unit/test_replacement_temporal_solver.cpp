#include "bbsolver/replacement_temporal/replacement_temporal_solver.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/io/io_json.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_options.hpp"
#include "bbsolver/replacement_temporal/replacement_temporal_relaxed_fit.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_replacement_temporal_solver: " << message << "\n";
  std::exit(1);
}

std::vector<double> ShapeFlatRect(double x, double y, double w, double h) {
  const std::vector<std::pair<double, double>> vertices = {
      {x, y},
      {x + w, y},
      {x + w, y + h},
      {x, y + h},
  };
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const auto& vertex : vertices) {
    out.push_back(vertex.first);
    out.push_back(vertex.second);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

std::vector<double> ShapeFlatQuad(double left_x, double right_x) {
  const std::vector<std::pair<double, double>> vertices = {
      {left_x, 0.0},
      {left_x, 10.0},
      {right_x, 10.0},
      {right_x, 0.0},
  };
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const auto& vertex : vertices) {
    out.push_back(vertex.first);
    out.push_back(vertex.second);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

std::vector<double> ShapeFlatCircle(double cx,
                                    double cy,
                                    double radius,
                                    int vertex_count) {
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertex_count));
  constexpr double kPi = 3.14159265358979323846264338327950288;
  for (int idx = 0; idx < vertex_count; ++idx) {
    const double theta = 2.0 * kPi * static_cast<double>(idx) /
                         static_cast<double>(vertex_count);
    out.push_back(cx + std::cos(theta) * radius);
    out.push_back(cy + std::sin(theta) * radius);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

int ShapeFlatVertexCount(const std::vector<double>& flat) {
  if (flat.size() < 2) {
    return 0;
  }
  return static_cast<int>(std::llround(flat[1]));
}

double Cubic(double t, double p0, double p1, double p2, double p3) {
  const double omt = 1.0 - t;
  return omt * omt * omt * p0 +
         3.0 * omt * omt * t * p1 +
         3.0 * omt * t * t * p2 +
         t * t * t * p3;
}

double ShapeTemporalProgress(double alpha,
                             double out_influence,
                             double in_influence) {
  const double x1 = out_influence / 100.0;
  const double x2 = 1.0 - in_influence / 100.0;
  double lo = 0.0;
  double hi = 1.0;
  for (int iter = 0; iter < 40; ++iter) {
    const double mid = 0.5 * (lo + hi);
    if (Cubic(mid, 0.0, x1, x2, 1.0) < alpha) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return Cubic(0.5 * (lo + hi), 0.0, 0.0, 1.0, 1.0);
}

bbsolver::PropertySamples MakeBumpFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/replacement_temporal/path";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 8.0 / 24.0;
  ps.samples_per_frame = 1;
  for (int idx = 0; idx <= 8; ++idx) {
    const double y = (idx == 0 || idx == 8) ? 0.0 : 5.0;
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatRect(static_cast<double>(idx), y, 10.0, 10.0)});
  }
  return ps;
}

bbsolver::PropertySamples MakeEasedFixture(double out_influence,
                                           double in_influence) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/replacement_temporal/eased";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 8.0 / 24.0;
  ps.samples_per_frame = 1;
  for (int idx = 0; idx <= 8; ++idx) {
    const double alpha = static_cast<double>(idx) / 8.0;
    const double u = ShapeTemporalProgress(alpha, out_influence, in_influence);
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatRect(40.0 * u, 0.0, 10.0, 10.0)});
  }
  return ps;
}

bbsolver::PropertySamples MakeSmoothFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/replacement_temporal/smooth";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 8.0 / 24.0;
  ps.samples_per_frame = 1;
  for (int idx = 0; idx <= 8; ++idx) {
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatRect(static_cast<double>(idx), 0.0, 10.0, 10.0)});
  }
  return ps;
}

bbsolver::PropertySamples MakeLargeSmoothFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/replacement_temporal/large_smooth";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 52;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 32.0 / 24.0;
  ps.samples_per_frame = 1;
  for (int idx = 0; idx <= 32; ++idx) {
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatCircle(static_cast<double>(idx), 0.0, 24.0, 52)});
  }
  return ps;
}

bbsolver::PropertySamples MakeIndependentModeFixture(int end_idx = 8) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/replacement_temporal/multimode";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = static_cast<double>(end_idx) / 24.0;
  ps.samples_per_frame = 1;
  const int left_stop = std::max(1, end_idx / 3);
  const int right_start = std::max(left_stop + 1, (end_idx * 2) / 3);
  for (int idx = 0; idx <= end_idx; ++idx) {
    const double left_x = 2.0 * static_cast<double>(std::min(idx, left_stop));
    const double right_x =
        20.0 + 2.0 * static_cast<double>(std::max(0, idx - right_start));
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatQuad(left_x, right_x)});
  }
  return ps;
}

std::pair<bbsolver::PropertySamples, bbsolver::PropertySamples>
MakeRelaxedEndpointFixture() {
  bbsolver::PropertySamples original;
  original.property.id = "unit/replacement_temporal/relaxed";
  original.property.kind = bbsolver::ValueKind::Custom;
  original.property.units_label = "shape_flat";
  original.property.dimensions = 2 + 6 * 4;
  original.t_start_sec = 0.0;
  original.t_end_sec = 8.0 / 24.0;
  original.samples_per_frame = 1;

  bbsolver::PropertySamples reduced = original;
  for (int idx = 0; idx <= 8; ++idx) {
    const double alpha = static_cast<double>(idx) / 8.0;
    const double source_x = -1.0 + 10.0 * alpha;
    double reduced_x = source_x;
    if (idx == 0) {
      reduced_x += 0.5;
    } else if (idx == 8) {
      reduced_x -= 0.5;
    }
    const double t = static_cast<double>(idx) / 24.0;
    original.samples.push_back({t, ShapeFlatRect(source_x, 0.0, 10.0, 10.0)});
    reduced.samples.push_back({t, ShapeFlatRect(reduced_x, 0.0, 10.0, 10.0)});
  }

  return {original, reduced};
}

bbsolver::ReplacementTemporalSolverOptions Options() {
  bbsolver::ReplacementTemporalSolverOptions options;
  options.band_options.frame_fit_options.outline_tolerance = 0.25;
  options.band_options.frame_fit_options.max_subdivisions_per_segment = 2;
  options.band_options.progress_steps = 40;
  options.band_options.max_window_samples = 16;
  options.band_options.max_evaluations = 1024;
  options.band_options.compute_progress_bands = true;
  options.band_options.min_bezier_influence = 10.0;
  options.band_options.max_bezier_influence = 90.0;
  options.band_options.bezier_influence_grid_steps = 5;
  options.band_options.bezier_influence_refinement_steps = 1;
  options.band_options.max_bezier_influence_pairs = 24;
  options.max_gap_samples = 8;
  return options;
}

bbsolver::SolverConfig Config() {
  bbsolver::SolverConfig config;
  config.tolerance = 0.25;
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = true;
  config.allow_shape_temporal_bezier = true;
  return config;
}

void TestReplacementTemporalOptionNormalizationDefaultsAndClamps() {
  const bbsolver::ReplacementTemporalSolverOptions defaults =
      bbsolver::NormalizeReplacementTemporalOptions(
          bbsolver::ReplacementTemporalSolverOptions{}, Config());
  Require(defaults.band_options.max_window_samples == 8,
          "default temporal replacement window changed");
  Require(defaults.max_gap_samples == 6,
          "default temporal replacement max gap changed");
  Require(defaults.band_options.progress_steps == 16,
          "default temporal replacement progress steps changed");
  Require(defaults.band_options.frame_fit_options.max_subdivisions_per_segment == 6,
          "default temporal replacement oracle subdivisions changed");
  Require(defaults.band_options.fit_bezier_influence_pairs,
          "default temporal replacement Bezier influence fitting changed");
  Require(defaults.allow_relaxed_endpoint_fit,
          "default temporal replacement relaxed endpoint gate changed");
  Require(defaults.allow_multimode_anchor_union,
          "default temporal replacement multimode gate changed");

  bbsolver::SolverConfig no_interp = Config();
  no_interp.allow_linear = false;
  no_interp.allow_bezier = false;
  bbsolver::ReplacementTemporalSolverOptions options;
  options.band_options.max_window_samples = 1;
  options.band_options.progress_steps = 1;
  options.band_options.max_evaluations = 1;
  options.max_gap_samples = 99;
  options.multimode_max_regions = 99;
  options.multimode_max_gap_samples = 0;
  options.multimode_max_candidate_key_ratio = -1.0;
  options.multimode_fast_accept_key_ratio = 99.0;
  options.forward_longest_span_min_vertex_count = -5;
  options.forward_longest_span_min_samples = 1;
  options.forward_longest_span_max_gap_samples = 0;
  options.forward_longest_span_max_segment_checks = 0;

  const bbsolver::ReplacementTemporalSolverOptions normalized =
      bbsolver::NormalizeReplacementTemporalOptions(options, no_interp);
  Require(normalized.band_options.max_window_samples == 2,
          "temporal replacement window lower clamp changed");
  Require(normalized.max_gap_samples == 1,
          "temporal replacement max gap upper clamp changed");
  Require(normalized.band_options.progress_steps == 2,
          "temporal replacement progress lower clamp changed");
  Require(normalized.band_options.max_evaluations == 4,
          "temporal replacement evaluation budget floor changed");
  Require(!normalized.band_options.compute_progress_bands,
          "temporal replacement production band scan reset changed");
  Require(!normalized.band_options.fit_bezier_influence_pairs,
          "temporal replacement Bezier fit gate must respect disabled Bezier");
  Require(!normalized.allow_relaxed_endpoint_fit,
          "temporal replacement relaxed endpoints must respect interpolation gate");
  Require(!normalized.allow_multimode_anchor_union,
          "temporal replacement multimode must respect linear gate");
  Require(normalized.multimode_max_regions == 16,
          "temporal replacement multimode region clamp changed");
  Require(normalized.multimode_max_gap_samples == 3,
          "temporal replacement multimode default gap changed");
  Require(normalized.multimode_max_candidate_key_ratio == 0.0,
          "temporal replacement candidate ratio lower clamp changed");
  Require(normalized.multimode_fast_accept_key_ratio == 1.0,
          "temporal replacement fast accept ratio upper clamp changed");
  Require(normalized.forward_longest_span_min_vertex_count == 0,
          "temporal replacement forward vertex lower clamp changed");
  Require(normalized.forward_longest_span_min_samples == 2,
          "temporal replacement forward sample lower clamp changed");
  Require(normalized.forward_longest_span_max_gap_samples == 256,
          "temporal replacement forward default max gap changed");
  Require(normalized.forward_longest_span_max_segment_checks == 20000,
          "temporal replacement forward default segment checks changed");
}

std::filesystem::path RepoPath(const std::filesystem::path& relative) {
  if (const char* test_source_dir = std::getenv("BBSOLVER_TEST_SOURCE_DIR")) {
    const std::filesystem::path solver_root_candidate =
        std::filesystem::path(test_source_dir) / relative;
    if (std::filesystem::exists(solver_root_candidate)) {
      return solver_root_candidate;
    }
  }
  std::filesystem::path dir = std::filesystem::current_path();
  for (int depth = 0; depth < 8; ++depth) {
    const std::filesystem::path solver_root_candidate = dir / relative;
    if (std::filesystem::exists(solver_root_candidate)) {
      return solver_root_candidate;
    }
    const std::filesystem::path monorepo_candidate = dir / "solver" / relative;
    if (std::filesystem::exists(monorepo_candidate)) {
      return monorepo_candidate;
    }
    if (!dir.has_parent_path() || dir == dir.parent_path()) {
      break;
    }
    dir = dir.parent_path();
  }
  return relative;
}

bbsolver::PropertySamples SliceSamples(const bbsolver::PropertySamples& source,
                                       int start_idx,
                                       int end_idx) {
  bbsolver::PropertySamples out = source;
  out.samples.clear();
  for (int idx = start_idx; idx <= end_idx; ++idx) {
    out.samples.push_back(source.samples[static_cast<std::size_t>(idx)]);
  }
  out.t_start_sec = out.samples.front().t_sec;
  out.t_end_sec = out.samples.back().t_sec;
  return out;
}

struct DominantFixtureSolveResult {
  int window_count = 0;
  int total_keys = 0;
  double max_err = 0.0;
  double max_err_screen_px = 0.0;
  double stage4_max_err = 0.0;
  double strict8_max_err = 0.0;
  bool stage4_ok = true;
  bool strict8_ok = true;
  bool forward_accepted = false;
  bool saw_bezier = false;
  bool saw_multimode = false;
};

struct DominantFixtureSolveOptions {
  bool allow_forward_longest_span = false;
  bool allow_multimode_anchor_union = false;
  bool allow_relaxed_endpoint_fit = false;
  bool allow_bezier = false;
  bool allow_shape_temporal_bezier = false;
  int max_gap_samples = 6;
  int oracle_subdivisions = 8;
  int stage4_subdivisions = 64;
  int strict_subdivisions = 8;
};

DominantFixtureSolveResult SolveDominant52Windows(
    const std::filesystem::path& sample_bundle_path,
    const DominantFixtureSolveOptions& solve_options = {}) {
  const bbsolver::SampleBundle bundle =
      bbsolver::ReadSampleBundleJson(sample_bundle_path);
  Require(!bundle.properties.empty(), "fixture has no properties");
  const bbsolver::PropertySamples& source = bundle.properties.front();

  bbsolver::SolverConfig config = bundle.config;
  config.tolerance = 0.5;
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = solve_options.allow_bezier;
  config.allow_shape_temporal_bezier =
      solve_options.allow_shape_temporal_bezier;

  bbsolver::ReplacementTemporalSolverOptions options;
  options.allow_forward_longest_span =
      solve_options.allow_forward_longest_span;
  options.allow_multimode_anchor_union =
      solve_options.allow_multimode_anchor_union;
  options.allow_relaxed_endpoint_fit =
      solve_options.allow_relaxed_endpoint_fit;
  options.max_gap_samples = solve_options.max_gap_samples;
  options.band_options.frame_fit_options.outline_tolerance = 0.5;
  options.band_options.frame_fit_options.max_subdivisions_per_segment =
      solve_options.oracle_subdivisions;
  options.band_options.progress_steps = 16;
  options.band_options.max_window_samples =
      solve_options.max_gap_samples + 1;
  options.forward_longest_span_min_vertex_count = 52;
  options.forward_longest_span_min_samples = 2;
  options.forward_longest_span_max_gap_samples = 256;
  options.forward_longest_span_max_segment_checks = 40000;

  DominantFixtureSolveResult result;
  int run_start = -1;
  const auto flush_run = [&](int run_end) {
    if (run_start < 0 || run_end < run_start) {
      return;
    }
    const bbsolver::PropertySamples window =
        SliceSamples(source, run_start, run_end);
    const bbsolver::PropertyKeys keys =
        bbsolver::SolveReplacementShapeFlatTemporal(
            window, window, config, bundle.comp, options);
    Require(keys.converged, "dominant-window temporal solve did not converge");
    result.forward_accepted =
        result.forward_accepted ||
        keys.notes.find("replacement_forward_longest_span_accepted=true") !=
            std::string::npos;
    result.saw_multimode =
        result.saw_multimode ||
        keys.notes.find("replacement_multimode_accepted=true") !=
            std::string::npos ||
        keys.notes.find("replacement_multimode_precheck=true") !=
            std::string::npos;
    for (const bbsolver::SegmentReport& segment : keys.segments) {
      result.saw_bezier =
          result.saw_bezier ||
          segment.reason == "replacement_shape_morph_bezier_ok" ||
          segment.reason == "replacement_shape_morph_bezier_fit_ok" ||
          segment.reason == "replacement_shape_morph_relaxed_bezier_ok" ||
          segment.reason == "replacement_shape_morph_relaxed_bezier_fit_ok";
    }

    bbsolver::PathTemporalValidationOptions stage4_options;
    stage4_options.frame_fit_options.outline_tolerance = 0.5;
    stage4_options.frame_fit_options.max_subdivisions_per_segment =
        solve_options.stage4_subdivisions;
    const bbsolver::PathTemporalValidationResult stage4 =
        bbsolver::ValidatePathTemporalCandidate(
            window, keys, stage4_options);
    Require(stage4.samples_checked == static_cast<int>(window.samples.size()),
            "stage4 validation did not check every sample");

    bbsolver::PathTemporalValidationOptions strict_options;
    strict_options.frame_fit_options.outline_tolerance = 0.5;
    strict_options.frame_fit_options.max_subdivisions_per_segment =
        solve_options.strict_subdivisions;
    const bbsolver::PathTemporalValidationResult strict =
        bbsolver::ValidatePathTemporalCandidate(
            window, keys, strict_options);
    Require(strict.samples_checked == static_cast<int>(window.samples.size()),
            "strict validation did not check every sample");

    ++result.window_count;
    result.total_keys += static_cast<int>(keys.keys.size());
    result.max_err = std::max(result.max_err, keys.max_err);
    result.max_err_screen_px =
        std::max(result.max_err_screen_px, keys.max_err_screen_px);
    result.stage4_max_err =
        std::max(result.stage4_max_err, stage4.max_outline_error);
    result.strict8_max_err =
        std::max(result.strict8_max_err, strict.max_outline_error);
    result.stage4_ok = result.stage4_ok && stage4.ok;
    result.strict8_ok = result.strict8_ok && strict.ok;
  };

  for (int idx = 0; idx < static_cast<int>(source.samples.size()); ++idx) {
    const bool is_dominant_52 =
        ShapeFlatVertexCount(source.samples[static_cast<std::size_t>(idx)].v) == 52;
    if (is_dominant_52 && run_start < 0) {
      run_start = idx;
    } else if (!is_dominant_52 && run_start >= 0) {
      flush_run(idx - 1);
      run_start = -1;
    }
  }
  if (run_start >= 0) {
    flush_run(static_cast<int>(source.samples.size()) - 1);
  }
  return result;
}

void PrintCapabilityResult(const char* label,
                           const bbsolver::PropertyKeys& keys) {
  if (std::getenv("BBSOLVER_REPLACEMENT_TEMPORAL_REPORT") == nullptr) {
    return;
  }
  std::cout << label
            << " keys=" << keys.keys.size()
            << " max_err=" << keys.max_err
            << " max_err_screen_px=" << keys.max_err_screen_px
            << " notes=" << keys.notes
            << "\n";
}

void PrintDominantResult(const char* label,
                         const DominantFixtureSolveResult& result) {
  if (std::getenv("BBSOLVER_REPLACEMENT_TEMPORAL_REPORT") == nullptr) {
    return;
  }
  std::cout << label
            << " windows=" << result.window_count
            << " keys=" << result.total_keys
            << " max_err=" << result.max_err
            << " max_err_screen_px=" << result.max_err_screen_px
            << " stage4_max_err=" << result.stage4_max_err
            << " strict8_max_err=" << result.strict8_max_err
            << " stage4_ok=" << (result.stage4_ok ? "true" : "false")
            << " strict8_ok=" << (result.strict8_ok ? "true" : "false")
            << " forward_accepted="
            << (result.forward_accepted ? "true" : "false")
            << " saw_bezier=" << (result.saw_bezier ? "true" : "false")
            << " saw_multimode=" << (result.saw_multimode ? "true" : "false")
            << "\n";
}

DominantFixtureSolveOptions LinearForwardOptions() {
  DominantFixtureSolveOptions options;
  options.allow_forward_longest_span = true;
  options.allow_multimode_anchor_union = false;
  options.allow_relaxed_endpoint_fit = false;
  options.allow_bezier = false;
  options.allow_shape_temporal_bezier = false;
  options.max_gap_samples = 6;
  return options;
}

DominantFixtureSolveOptions LinearBaselineOptions() {
  DominantFixtureSolveOptions options = LinearForwardOptions();
  options.allow_forward_longest_span = false;
  return options;
}

DominantFixtureSolveOptions MainLikeBaselineOptions() {
  DominantFixtureSolveOptions options;
  options.allow_forward_longest_span = false;
  options.allow_multimode_anchor_union = true;
  options.allow_relaxed_endpoint_fit = true;
  options.allow_bezier = true;
  options.allow_shape_temporal_bezier = true;
  options.max_gap_samples = 8;
  return options;
}

void TestRelaxedEndpointFitWritesCustomKeyValues() {
  const auto fixture = MakeRelaxedEndpointFixture();
  bbsolver::SolverConfig config = Config();
  config.tolerance = 0.3;
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.band_options.frame_fit_options.outline_tolerance = 0.3;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture.first, fixture.second, config, bbsolver::CompInfo{}, options);

  assert(keys.converged);
  assert(keys.keys.size() == 2);
  assert(keys.segments.size() == 1);
  assert(keys.segments.front().reason ==
         "replacement_shape_morph_relaxed_linear_ok");
  assert(keys.segments.front().max_err <= 0.3 + 1e-9);
  assert(std::abs(keys.keys.front().v[2] - fixture.second.samples.front().v[2]) > 0.05);
  assert(std::abs(keys.keys.back().v[2] - fixture.second.samples.back().v[2]) > 0.05);
  assert(std::abs(keys.keys.front().v[2] - fixture.first.samples.front().v[2]) <= 0.3);
  assert(std::abs(keys.keys.back().v[2] - fixture.first.samples.back().v[2]) <= 0.3);
}

void TestRelaxedHelperProgressAndEaseValues() {
  const auto fixture = MakeRelaxedEndpointFixture();
  const bbsolver::ReplacementTemporalSolverOptions options = Options();

  const std::vector<bbsolver::TemporalEase> fallback =
      bbsolver::replacement_temporal::ShapeEaseForInfluence(
          std::numeric_limits<double>::quiet_NaN());
  assert(fallback.size() == 1);
  assert(fallback.front().speed == 0.0);
  assert(fallback.front().influence == 33.3);
  assert(bbsolver::replacement_temporal::ShapeEaseForInfluence(-10.0)
             .front()
             .influence == 0.1);
  assert(bbsolver::replacement_temporal::ShapeEaseForInfluence(200.0)
             .front()
             .influence == 100.0);

  const bbsolver::TemporalEase neutral = fallback.front();
  const std::vector<double> linear_progress =
      bbsolver::replacement_temporal::SegmentProgressValues(
          fixture.second, 0, 8, false, neutral, neutral,
          options.band_options);
  assert(linear_progress.size() == 9);
  assert(linear_progress.front() == 0.0);
  assert(std::abs(linear_progress[4] - 0.5) <= 1e-12);
  assert(linear_progress.back() == 1.0);

  const std::vector<double> bezier_progress =
      bbsolver::replacement_temporal::SegmentProgressValues(
          fixture.second, 0, 8, true, neutral, neutral,
          options.band_options);
  assert(bezier_progress.size() == 9);
  assert(bezier_progress.front() >= 0.0);
  assert(bezier_progress.front() <= 1e-12);
  assert(std::abs(bezier_progress.back() - 1.0) <= 1e-12);
  for (std::size_t idx = 1; idx < bezier_progress.size(); ++idx) {
    assert(bezier_progress[idx] >= bezier_progress[idx - 1] - 1e-12);
  }

  const bbsolver::replacement_temporal::RelaxedValidation validation =
      bbsolver::replacement_temporal::ValidateRelaxedChord(
          fixture.first, 0, 8,
          fixture.first.samples.front().v,
          fixture.first.samples.back().v,
          linear_progress,
          options.band_options);
  assert(validation.ok);
  assert(validation.outline_checks == 9);
  assert(validation.max_err <=
         options.band_options.frame_fit_options.outline_tolerance + 1e-9);
}

void TestMultiModeAnchorUnionBeatsSingleSharedProgress() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.max_gap_samples = 1;
  options.allow_relaxed_endpoint_fit = false;
  options.allow_multimode_anchor_union = true;
  options.multimode_max_regions = 2;
  options.multimode_max_gap_samples = 8;
  options.multimode_region_tolerance = 0.01;
  options.band_options.frame_fit_options.outline_tolerance = 0.01;

  const bbsolver::ShapeMorphProgressBandResult single_segment =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          fixture,
          0,
          8,
          fixture.samples.front().v,
          fixture.samples.back().v,
          options.band_options);
  assert(single_segment.ok);
  assert(!single_segment.linear_progress_possible);

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, Config(), bbsolver::CompInfo{}, options);

  assert(keys.converged);
  assert(keys.keys.size() == 4);
  assert(keys.notes.find("shape_multimode_candidate") != std::string::npos);
  assert(keys.notes.find("region_ranges=0-2,2-4") != std::string::npos);
  assert(keys.notes.find("replacement_multimode_accepted=true") != std::string::npos);
  assert(keys.segments.size() == 3);
  for (const bbsolver::SegmentReport& segment : keys.segments) {
    assert(segment.reason == "replacement_shape_multimode_linear_union");
  }
}

void TestMultiModePrecheckCanSkipExpensiveSharedProgressDP() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture(32);
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.max_gap_samples = 1;
  options.allow_relaxed_endpoint_fit = false;
  options.allow_multimode_anchor_union = true;
  options.multimode_max_regions = 2;
  options.multimode_max_gap_samples = 32;
  options.multimode_region_tolerance = 0.01;
  options.multimode_fast_accept_key_ratio = 0.65;
  options.band_options.frame_fit_options.outline_tolerance = 0.01;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, Config(), bbsolver::CompInfo{}, options);

  assert(keys.converged);
  assert(keys.keys.size() == 4);
  assert(keys.notes.find("replacement_multimode_precheck=true") != std::string::npos);
  assert(keys.notes.find("replacement_shape_temporal_solver=skipped_after_validated_multimode") != std::string::npos);
}

void TestMultiModeValidationBudgetFailsBeforeOutlineValidation() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture(32);
  bbsolver::ShapeFlatMultiModeOptions options;
  options.frame_fit_options.outline_tolerance = 0.01;
  options.max_regions = 2;
  options.max_gap_samples = 32;
  options.region_tolerance = 0.01;
  options.max_validation_samples = 8;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);

  assert(!keys.converged);
  assert(keys.keys.size() == 4);
  assert(keys.notes.find("shape_multimode_validation_budget_exceeded") != std::string::npos);
  assert(keys.notes.find("union_keys=4") != std::string::npos);
  assert(keys.notes.find("region_ranges=0-2,2-4") != std::string::npos);
}

void TestOracleReasonsStillVisible() {
  const bbsolver::PropertySamples fixture = MakeBumpFixture();
  const bbsolver::ReplacementTemporalSolverOptions options = Options();

  const bbsolver::ShapeMorphProgressBandResult long_result =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          fixture,
          0,
          8,
          fixture.samples[0].v,
          fixture.samples[8].v,
          options.band_options);
  assert(long_result.ok);
  assert(long_result.reason == "infeasible_shape_morph_chord");
  assert(!long_result.monotone_progress_possible);
  assert(!long_result.linear_progress_possible);

  const bbsolver::ShapeMorphProgressBandResult short_result =
      bbsolver::EvaluateShapeFlatMorphProgressBands(
          fixture,
          4,
          6,
          fixture.samples[4].v,
          fixture.samples[6].v,
          options.band_options);
  assert(short_result.ok);
  assert(short_result.reason == "shape_morph_chord_ok");
  assert(short_result.monotone_progress_possible);
  assert(short_result.linear_progress_possible);
}

void TestWrapperRejectsInfeasibleLongChord() {
  const bbsolver::PropertySamples fixture = MakeBumpFixture();
  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, Config(), bbsolver::CompInfo{}, Options());

  assert(keys.converged);
  assert(keys.keys.size() > 2);
  assert(!keys.segments.empty());
  assert(keys.notes.find("replacement_shape_temporal_solver") != std::string::npos);
  for (const bbsolver::SegmentReport& segment : keys.segments) {
    assert(!(segment.start_idx == 0 && segment.end_idx == 8));
    assert(segment.reason == "replacement_shape_morph_linear_ok" ||
           segment.reason == "replacement_shape_morph_bezier_ok" ||
           segment.reason == "replacement_shape_morph_bezier_fit_ok");
  }
}

void TestWrapperAcceptsFullSmoothChord() {
  const bbsolver::PropertySamples fixture = MakeSmoothFixture();
  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, Config(), bbsolver::CompInfo{}, Options());

  assert(keys.converged);
  assert(keys.keys.size() == 2);
  assert(keys.segments.size() == 1);
  assert(keys.segments.front().start_idx == 0);
  assert(keys.segments.front().end_idx == 8);
  assert(keys.segments.front().reason == "replacement_shape_morph_linear_ok");
}

void TestForwardLongestSpanBeatsBoundedDPForLargeTopology() {
  const bbsolver::PropertySamples fixture = MakeLargeSmoothFixture();
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.allow_forward_longest_span = true;
  options.allow_multimode_anchor_union = false;
  options.allow_relaxed_endpoint_fit = false;
  options.max_gap_samples = 4;
  options.band_options.max_window_samples = 5;
  options.forward_longest_span_min_vertex_count = 52;
  options.forward_longest_span_min_samples = 2;
  options.forward_longest_span_max_gap_samples = 64;
  options.forward_longest_span_max_segment_checks = 2048;

  bbsolver::SolverConfig config = Config();
  config.allow_bezier = false;
  config.allow_shape_temporal_bezier = false;
  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, config, bbsolver::CompInfo{}, options);

  Require(keys.converged, "large topology forward solve did not converge");
  Require(keys.keys.size() == 2,
          "large topology forward solve did not collapse to 2 keys");
  Require(keys.segments.size() == 1,
          "large topology forward solve did not produce one segment");
  Require(keys.segments.front().start_idx == 0,
          "large topology forward segment start mismatch");
  Require(keys.segments.front().end_idx == 32,
          "large topology forward segment end mismatch");
  Require(keys.segments.front().reason ==
              "replacement_shape_morph_forward_longest_span_linear_ok",
          "large topology forward segment reason mismatch");
  Require(keys.notes.find("replacement_forward_longest_span_accepted=true") !=
              std::string::npos,
          "large topology forward candidate was not accepted");
  Require(keys.notes.find("previous_keys=") != std::string::npos,
          "large topology forward result did not report previous key count");
}

void TestForwardLongestSpanEmitsProgressEvents() {
  const bbsolver::PropertySamples fixture = MakeLargeSmoothFixture();
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.allow_forward_longest_span = true;
  options.allow_multimode_anchor_union = false;
  options.allow_relaxed_endpoint_fit = false;
  options.max_gap_samples = 4;
  options.band_options.max_window_samples = 5;
  options.forward_longest_span_min_vertex_count = 52;
  options.forward_longest_span_min_samples = 2;
  options.forward_longest_span_max_gap_samples = 64;
  options.forward_longest_span_max_segment_checks = 2048;

  std::vector<std::string> stages;
  options.placement_progress_fn =
      [&](const bbsolver::PlacementProgress& progress) {
        stages.push_back(progress.stage);
        Require(progress.step_total > 0,
                "forward progress must report a positive step total");
        Require(progress.step_index >= 0,
                "forward progress must not report a negative step");
      };

  bbsolver::SolverConfig config = Config();
  config.allow_bezier = false;
  config.allow_shape_temporal_bezier = false;
  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, config, bbsolver::CompInfo{}, options);

  Require(keys.converged, "forward progress fixture did not converge");
  const auto start_it =
      std::find(stages.begin(), stages.end(),
                "replacement_forward_longest_span_start");
  const auto progress_it =
      std::find(stages.begin(), stages.end(),
                "replacement_forward_longest_span_progress");
  const auto done_it =
      std::find(stages.begin(), stages.end(),
                "replacement_forward_longest_span_done");
  Require(start_it != stages.end(), "forward progress start event missing");
  Require(progress_it != stages.end(), "forward progress update event missing");
  Require(done_it != stages.end(), "forward progress done event missing");
  Require(start_it < progress_it && progress_it < done_it,
          "forward progress events are not ordered");
}

void TestForwardLongestSpanDefaultOffKeepsBoundedDPForLargeTopology() {
  const bbsolver::PropertySamples fixture = MakeLargeSmoothFixture();
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.allow_multimode_anchor_union = false;
  options.allow_relaxed_endpoint_fit = false;
  options.max_gap_samples = 4;
  options.band_options.max_window_samples = 5;
  options.forward_longest_span_min_vertex_count = 52;
  options.forward_longest_span_min_samples = 2;
  options.forward_longest_span_max_gap_samples = 64;
  options.forward_longest_span_max_segment_checks = 2048;

  bbsolver::SolverConfig config = Config();
  config.allow_bezier = false;
  config.allow_shape_temporal_bezier = false;
  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, config, bbsolver::CompInfo{}, options);

  Require(keys.converged, "default-off large topology solve did not converge");
  Require(keys.keys.size() == 9,
          "default-off large topology solve changed bounded-DP key count");
  Require(keys.notes.find("replacement_forward_longest_span") ==
              std::string::npos,
          "default-off result unexpectedly mentioned forward span");
}

void TestForwardLongestSpanDoesNotReplaceBezierWin() {
  const bbsolver::PropertySamples fixture = MakeEasedFixture(90.0, 10.0);
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.allow_forward_longest_span = true;
  options.forward_longest_span_min_vertex_count = 4;
  options.forward_longest_span_min_samples = 2;
  options.forward_longest_span_max_gap_samples = 16;
  options.forward_longest_span_max_segment_checks = 256;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, Config(), bbsolver::CompInfo{}, options);

  PrintCapabilityResult("bezier_win_guard", keys);
  Require(keys.converged, "Bezier guard did not converge");
  Require(keys.keys.size() == 2, "Bezier guard lost the 2-key result");
  Require(keys.segments.size() == 1, "Bezier guard segment count mismatch");
  Require(keys.segments.front().reason ==
              "replacement_shape_morph_bezier_fit_ok",
          "Bezier guard lost fitted-Bezier segment");
  Require(keys.notes.find("replacement_forward_longest_span_accepted=false") !=
              std::string::npos,
          "Bezier guard did not record rejected forward candidate");
}

void TestForwardLongestSpanDoesNotReplaceMultiModeWin() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture(32);
  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.allow_forward_longest_span = true;
  options.max_gap_samples = 1;
  options.allow_relaxed_endpoint_fit = false;
  options.allow_multimode_anchor_union = true;
  options.multimode_max_regions = 2;
  options.multimode_max_gap_samples = 32;
  options.multimode_region_tolerance = 0.01;
  options.multimode_fast_accept_key_ratio = 0.65;
  options.band_options.frame_fit_options.outline_tolerance = 0.01;
  options.forward_longest_span_min_vertex_count = 4;
  options.forward_longest_span_min_samples = 2;
  options.forward_longest_span_max_gap_samples = 32;
  options.forward_longest_span_max_segment_checks = 2048;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, Config(), bbsolver::CompInfo{}, options);

  PrintCapabilityResult("multimode_win_guard", keys);
  Require(keys.converged, "multimode guard did not converge");
  Require(keys.keys.size() == 4, "multimode guard lost the 4-key result");
  Require(keys.notes.find("replacement_multimode_precheck=true") !=
              std::string::npos,
          "multimode guard did not use multimode precheck");
  Require(keys.notes.find("replacement_shape_temporal_solver=skipped_after_validated_multimode") !=
              std::string::npos,
          "multimode guard did not skip DP after multimode");
  Require(keys.notes.find("replacement_forward_longest_span_accepted=true") ==
              std::string::npos,
          "multimode guard was replaced by forward candidate");
}

void TestWrapperFitsShapeBezierInfluencePair() {
  const bbsolver::PropertySamples fixture = MakeEasedFixture(90.0, 10.0);
  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, Config(), bbsolver::CompInfo{}, Options());

  assert(keys.converged);
  assert(keys.keys.size() == 2);
  assert(keys.segments.size() == 1);
  assert(keys.segments.front().reason ==
         "replacement_shape_morph_bezier_fit_ok");
  assert(keys.segments.front().max_err <= 1e-6);
  assert(!keys.keys.front().temporal_ease_out.empty());
  assert(!keys.keys.back().temporal_ease_in.empty());
  assert(std::abs(keys.keys.front().temporal_ease_out.front().speed) <= 1e-12);
  assert(std::abs(keys.keys.back().temporal_ease_in.front().speed) <= 1e-12);
  assert(std::abs(keys.keys.front().temporal_ease_out.front().influence - 90.0) <= 1e-6);
  assert(std::abs(keys.keys.back().temporal_ease_in.front().influence - 10.0) <= 1e-6);
}

void TestWrapperRequiresMatchingSampleTimes() {
  bbsolver::PropertySamples original = MakeSmoothFixture();
  bbsolver::PropertySamples reduced = original;
  reduced.samples[3].t_sec += 0.01;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          original, reduced, Config(), bbsolver::CompInfo{}, Options());

  assert(!keys.converged);
  assert(keys.notes == "replacement_temporal_sample_time_mismatch");
}

void TestWrapperDefaultsAreBounded() {
  const bbsolver::PropertySamples fixture = MakeSmoothFixture();
  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture,
          fixture,
          Config(),
          bbsolver::CompInfo{},
          bbsolver::ReplacementTemporalSolverOptions{});

  assert(keys.converged);
  assert(keys.notes.find("replacement_temporal_max_gap_samples=6") != std::string::npos);
  assert(keys.notes.find("replacement_temporal_progress_steps=16") != std::string::npos);
  assert(keys.notes.find("replacement_temporal_oracle_subdivisions=6") != std::string::npos);
  assert(keys.notes.find("replacement_temporal_fit_bezier_pairs=true") != std::string::npos);
  assert(keys.notes.find("replacement_temporal_relaxed_endpoints=true") != std::string::npos);
  assert(keys.notes.find("replacement_forward_longest_span") == std::string::npos);
}

void TestValidatedAllSamplesAnchorFallbackConverges() {
  const bbsolver::PropertySamples fixture = MakeSmoothFixture();
  bbsolver::SolverConfig config = Config();
  config.allow_linear = false;
  config.allow_bezier = false;
  config.allow_shape_temporal_bezier = false;

  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.allow_multimode_anchor_union = false;
  options.allow_forward_longest_span = false;
  options.max_gap_samples = 1;
  options.band_options.max_window_samples = 2;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, config, bbsolver::CompInfo{}, options);

  Require(keys.converged,
          "validated all-samples anchor fallback should be accepted");
  Require(keys.keys.size() == fixture.samples.size(),
          "all-samples anchor fallback should emit one key per sample");
  Require(keys.notes.find("falling back to all-samples-as-anchors") !=
              std::string::npos,
          "fallback note missing");
  Require(keys.notes.find("exact_anchor_fallback_validated=true") !=
              std::string::npos,
          "validated fallback note missing");
  Require(keys.notes.find("exact_anchor_fallback_hold_export=true") !=
              std::string::npos,
          "validated fallback hold-export note missing");
  Require(keys.notes.find("exact_anchor_fallback_linear_prune_accepted=false") !=
              std::string::npos,
          "validated fallback should report linear-prune decision");
  Require(keys.notes.find("prune_reason=linear_disabled") !=
              std::string::npos,
          "validated fallback should skip linear prune when linear interpolation is disabled");
  Require(keys.max_err <= 1e-9,
          "validated exact fallback should have zero sample error");
  for (std::size_t idx = 0; idx < keys.keys.size(); ++idx) {
    if (idx + 1 < keys.keys.size()) {
      Require(keys.keys[idx].interp_out == bbsolver::InterpType::Hold,
              "dense fallback outgoing side should export as Hold");
    }
    if (idx > 0) {
      Require(keys.keys[idx].interp_in == bbsolver::InterpType::Hold,
              "dense fallback incoming side should export as Hold");
    }
  }
}

void TestConvergedAllSampleResultGetsBoundedLinearPrune() {
  bbsolver::PropertySamples fixture = MakeSmoothFixture();
  for (int idx = 9; idx <= 16; ++idx) {
    fixture.samples.push_back({static_cast<double>(idx) / 24.0,
                               ShapeFlatRect(static_cast<double>(idx), 0.0, 10.0, 10.0)});
  }
  fixture.t_end_sec = fixture.samples.back().t_sec;

  bbsolver::SolverConfig config = Config();
  config.allow_linear = true;
  config.allow_bezier = false;
  config.allow_shape_temporal_bezier = false;

  bbsolver::ReplacementTemporalSolverOptions options = Options();
  options.allow_multimode_anchor_union = false;
  options.allow_forward_longest_span = false;
  options.allow_relaxed_endpoint_fit = false;
  options.max_gap_samples = 1;
  options.band_options.max_window_samples = 2;
  options.forward_longest_span_max_gap_samples = 64;
  options.forward_longest_span_max_segment_checks = 512;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveReplacementShapeFlatTemporal(
          fixture, fixture, config, bbsolver::CompInfo{}, options);

  Require(keys.converged,
          "all-sample linear prune should keep the solve converged");
  Require(keys.keys.size() == 2,
          "all-sample linear prune should collapse a smooth over-keyed solve");
  Require(keys.notes.find("all_sample_linear_prune_accepted=true") !=
              std::string::npos,
          "all-sample linear prune acceptance note missing");
  Require(keys.max_err <= 1e-9,
          "all-sample linear prune should preserve exact smooth fixture error");
}

void TestForwardLongestSpanMatchesDominantWindowSweepTargets() {
  const DominantFixtureSolveOptions options = LinearForwardOptions();
  const DominantFixtureSolveResult comp7 = SolveDominant52Windows(RepoPath(
      "tests/fixtures/path_keydumps/2026-05-18_desktop/"
      "sample_bundles/bbsolver_path_keydump_compact_20260518_054958.bbsm.json"),
      options);
  PrintDominantResult("comp7_forward_linear", comp7);
  Require(comp7.window_count == 2, "Comp7 dominant window count changed");
  Require(comp7.total_keys == 51,
          "Comp7 forward key count changed from the measured C++ result");
  Require(comp7.total_keys <= 56, "Comp7 forward result misses sweep target");
  Require(comp7.forward_accepted, "Comp7 forward candidate was not accepted");
  Require(comp7.max_err <= 0.5 + 1e-9,
          "Comp7 forward max_err exceeds tolerance");
  Require(comp7.max_err_screen_px <= 0.5 + 1e-9,
          "Comp7 forward max_err_screen_px exceeds tolerance");
  Require(comp7.stage4_ok, "Comp7 forward result failed stage4 validation");
  Require(comp7.strict8_ok, "Comp7 forward result failed strict8 validation");

  const DominantFixtureSolveResult comp8 = SolveDominant52Windows(RepoPath(
      "tests/fixtures/path_keydumps/2026-05-18_desktop/"
      "sample_bundles/bbsolver_path_keydump_compact_20260518_060021.bbsm.json"),
      options);
  PrintDominantResult("comp8_forward_linear", comp8);
  Require(comp8.window_count == 3, "Comp8 dominant window count changed");
  Require(comp8.total_keys == 76,
          "Comp8 forward key count changed from the measured C++ result");
  Require(comp8.total_keys <= 81, "Comp8 forward result misses sweep target");
  Require(comp8.forward_accepted, "Comp8 forward candidate was not accepted");
  Require(comp8.max_err <= 0.5 + 1e-9,
          "Comp8 forward max_err exceeds tolerance");
  Require(comp8.max_err_screen_px <= 0.5 + 1e-9,
          "Comp8 forward max_err_screen_px exceeds tolerance");
  Require(comp8.stage4_ok, "Comp8 forward result failed stage4 validation");
  Require(comp8.strict8_ok, "Comp8 forward result failed strict8 validation");
}

void TestDominantWindowBaselinesDoNotMeetForwardTargets() {
  const DominantFixtureSolveOptions linear_baseline = LinearBaselineOptions();
  const DominantFixtureSolveResult comp7_linear =
      SolveDominant52Windows(RepoPath(
          "tests/fixtures/path_keydumps/2026-05-18_desktop/"
          "sample_bundles/bbsolver_path_keydump_compact_20260518_054958.bbsm.json"),
          linear_baseline);
  PrintDominantResult("comp7_baseline_linear_no_forward", comp7_linear);
  Require(!comp7_linear.forward_accepted,
          "Comp7 linear baseline unexpectedly accepted forward");
  Require(comp7_linear.total_keys == 65,
          "Comp7 linear baseline key count changed");
  Require(comp7_linear.total_keys > 56,
          "Comp7 linear baseline already meets the forward target");

  const DominantFixtureSolveResult comp8_linear =
      SolveDominant52Windows(RepoPath(
          "tests/fixtures/path_keydumps/2026-05-18_desktop/"
          "sample_bundles/bbsolver_path_keydump_compact_20260518_060021.bbsm.json"),
          linear_baseline);
  PrintDominantResult("comp8_baseline_linear_no_forward", comp8_linear);
  Require(!comp8_linear.forward_accepted,
          "Comp8 linear baseline unexpectedly accepted forward");
  Require(comp8_linear.total_keys == 85,
          "Comp8 linear baseline key count changed");
  Require(comp8_linear.total_keys > 81,
          "Comp8 linear baseline already meets the forward target");

  const DominantFixtureSolveOptions main_like = MainLikeBaselineOptions();
  const DominantFixtureSolveResult comp7_main_like =
      SolveDominant52Windows(RepoPath(
          "tests/fixtures/path_keydumps/2026-05-18_desktop/"
          "sample_bundles/bbsolver_path_keydump_compact_20260518_054958.bbsm.json"),
          main_like);
  PrintDominantResult("comp7_baseline_main_like_no_forward", comp7_main_like);
  Require(!comp7_main_like.forward_accepted,
          "Comp7 main-like baseline unexpectedly accepted forward");
  Require(comp7_main_like.total_keys == 60,
          "Comp7 main-like baseline key count changed");
  Require(comp7_main_like.total_keys > 56,
          "Comp7 main-like baseline already meets the forward target");
  Require(comp7_main_like.saw_bezier,
          "Comp7 main-like baseline did not exercise Bezier capability");

  const DominantFixtureSolveResult comp8_main_like =
      SolveDominant52Windows(RepoPath(
          "tests/fixtures/path_keydumps/2026-05-18_desktop/"
          "sample_bundles/bbsolver_path_keydump_compact_20260518_060021.bbsm.json"),
          main_like);
  PrintDominantResult("comp8_baseline_main_like_no_forward", comp8_main_like);
  Require(!comp8_main_like.forward_accepted,
          "Comp8 main-like baseline unexpectedly accepted forward");
  Require(comp8_main_like.total_keys == 80,
          "Comp8 main-like baseline key count changed");
  Require(comp8_main_like.total_keys <= 81,
          "Comp8 main-like Bezier baseline no longer meets forward target");
  Require(comp8_main_like.saw_bezier,
          "Comp8 main-like baseline did not exercise Bezier capability");
}

bbsolver::PropertySamples MakeScalarSamples() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/dp/relaxed-anchor";
  ps.property.kind = bbsolver::ValueKind::Scalar;
  ps.property.dimensions = 1;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 2.0;
  ps.samples = {
      {0.0, {0.0}},
      {1.0, {1.0}},
      {2.0, {2.0}},
  };
  return ps;
}

void TestDPRejectsInconsistentSharedEndpointValues() {
  const bbsolver::PropertySamples ps = MakeScalarSamples();
  bbsolver::SolverConfig config;
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = false;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveProperty(
          ps,
          config,
          bbsolver::CompInfo{},
          [](int i,
             int j,
             const bbsolver::PropertySamples&,
             const bbsolver::SolverConfig&,
             const bbsolver::CompInfo&) {
            bbsolver::SegmentFitResult result;
            result.feasible = true;
            result.interp = bbsolver::InterpType::Linear;
            result.reason = "unit_custom_endpoint";
            if (i == 0 && j == 1) {
              result.key_value_at_j = {10.0};
              return result;
            }
            if (i == 1 && j == 2) {
              result.key_value_at_i = {20.0};
              result.key_value_at_j = {30.0};
              return result;
            }
            result.feasible = false;
            return result;
          },
          {},
          1);

  assert(!keys.converged);
  assert(keys.notes.find("no feasible segmentation") != std::string::npos);
}

void TestDPWritesConsistentSharedEndpointValues() {
  const bbsolver::PropertySamples ps = MakeScalarSamples();
  bbsolver::SolverConfig config;
  config.allow_hold = false;
  config.allow_linear = true;
  config.allow_bezier = false;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveProperty(
          ps,
          config,
          bbsolver::CompInfo{},
          [](int i,
             int j,
             const bbsolver::PropertySamples&,
             const bbsolver::SolverConfig&,
             const bbsolver::CompInfo&) {
            bbsolver::SegmentFitResult result;
            result.feasible = true;
            result.interp = bbsolver::InterpType::Linear;
            result.reason = "unit_custom_endpoint";
            if (i == 0 && j == 1) {
              result.key_value_at_j = {10.0};
              return result;
            }
            if (i == 1 && j == 2) {
              result.key_value_at_i = {10.0};
              result.key_value_at_j = {30.0};
              return result;
            }
            result.feasible = false;
            return result;
          },
          {},
          1);

  assert(keys.converged);
  assert(keys.keys.size() == 3);
  assert(keys.keys[0].v[0] == 0.0);
  assert(keys.keys[1].v[0] == 10.0);
  assert(keys.keys[2].v[0] == 30.0);
}

}  // namespace

int main() {
  TestReplacementTemporalOptionNormalizationDefaultsAndClamps();
  TestOracleReasonsStillVisible();
  TestWrapperRejectsInfeasibleLongChord();
  TestWrapperAcceptsFullSmoothChord();
  TestForwardLongestSpanDefaultOffKeepsBoundedDPForLargeTopology();
  TestForwardLongestSpanBeatsBoundedDPForLargeTopology();
  TestForwardLongestSpanEmitsProgressEvents();
  TestForwardLongestSpanDoesNotReplaceBezierWin();
  TestForwardLongestSpanDoesNotReplaceMultiModeWin();
  TestWrapperFitsShapeBezierInfluencePair();
  TestRelaxedEndpointFitWritesCustomKeyValues();
  TestRelaxedHelperProgressAndEaseValues();
  TestMultiModeAnchorUnionBeatsSingleSharedProgress();
  TestMultiModePrecheckCanSkipExpensiveSharedProgressDP();
  TestMultiModeValidationBudgetFailsBeforeOutlineValidation();
  TestWrapperRequiresMatchingSampleTimes();
  TestWrapperDefaultsAreBounded();
  TestValidatedAllSamplesAnchorFallbackConverges();
  TestConvergedAllSampleResultGetsBoundedLinearPrune();
  TestForwardLongestSpanMatchesDominantWindowSweepTargets();
  TestDominantWindowBaselinesDoNotMeetForwardTargets();
  TestDPRejectsInconsistentSharedEndpointValues();
  TestDPWritesConsistentSharedEndpointValues();
  return 0;
}
