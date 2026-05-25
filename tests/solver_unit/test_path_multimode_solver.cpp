#include "bbsolver/path/multimode/path_multimode_solver.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

double TemporalBezierProgress(double alpha,
                              double out_influence,
                              double in_influence);

std::vector<double> ShapeFlatQuad(double left_x,
                                  double right_x,
                                  double top_y = 0.0,
                                  double bottom_y = 10.0) {
  const std::vector<std::pair<double, double>> vertices = {
      {left_x, top_y},
      {left_x, bottom_y},
      {right_x, bottom_y},
      {right_x, top_y},
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

bbsolver::PropertySamples MakeIndependentModeFixture(int end_idx = 8) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/independent";
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

bbsolver::PropertySamples MakeZigZagFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/zigzag";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 4.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 4; ++idx) {
    const double y = (idx % 2 == 0) ? 0.0 : 20.0;
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatQuad(0.0, 20.0, y, y + 10.0)});
  }
  return ps;
}

std::vector<double> ShapeFlatSingleOutlierQuad(double outlier_y) {
  const std::vector<std::pair<double, double>> vertices = {
      {0.0, 0.0},
      {0.0, outlier_y},
      {20.0, 10.0},
      {20.0, 0.0},
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

bbsolver::PropertySamples MakeSingleOutlierVertexFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/single_outlier_vertex";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 4.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 4; ++idx) {
    const double y = (idx % 2 == 0) ? 0.0 : 40.0;
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatSingleOutlierQuad(y)});
  }
  return ps;
}

std::vector<double> ShapeFlatInterleavedMaskedChannels(double progress_a,
                                                       double progress_b) {
  const std::vector<std::pair<double, double>> starts = {
      {0.0, 0.0},
      {10.0, 10.0},
      {20.0, 20.0},
      {30.0, 0.0},
  };
  const std::vector<std::pair<double, double>> deltas = {
      {40.0, 0.0},
      {0.0, 40.0},
      {40.0, 0.0},
      {0.0, 40.0},
  };
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(starts.size()));
  for (int vertex = 0; vertex < static_cast<int>(starts.size()); ++vertex) {
    const double progress = (vertex % 2 == 0) ? progress_a : progress_b;
    out.push_back(starts[static_cast<std::size_t>(vertex)].first +
                  deltas[static_cast<std::size_t>(vertex)].first * progress);
    out.push_back(starts[static_cast<std::size_t>(vertex)].second +
                  deltas[static_cast<std::size_t>(vertex)].second * progress);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

bbsolver::PropertySamples MakeInterleavedMaskedChannelFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/interleaved_mask_channels";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 8.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 8; ++idx) {
    const double alpha = static_cast<double>(idx) / 8.0;
    const double progress_a = TemporalBezierProgress(alpha, 90.0, 10.0);
    const double progress_b = TemporalBezierProgress(alpha, 10.0, 90.0);
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatInterleavedMaskedChannels(progress_a,
                                                             progress_b)});
  }
  return ps;
}

std::vector<double> ShapeFlatRelaxedRegionalTiming(double left_x,
                                                   double right_x) {
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

bbsolver::PropertySamples MakeRelaxedRecombinedRegionalFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/recombined_region_relaxed";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 2.0 / 24.0;
  ps.samples_per_frame = 1;

  ps.samples.push_back({0.0 / 24.0,
                        ShapeFlatRelaxedRegionalTiming(0.0, 80.0)});
  ps.samples.push_back({1.0 / 24.0,
                        ShapeFlatRelaxedRegionalTiming(6.0, 74.0)});
  ps.samples.push_back({2.0 / 24.0,
                        ShapeFlatRelaxedRegionalTiming(20.0, 60.0)});
  return ps;
}

std::vector<double> ShapeFlatTwoPointCurve(double handle_y) {
  std::vector<double> out;
  out.push_back(0.0);
  out.push_back(2.0);
  out.push_back(0.0);
  out.push_back(0.0);
  out.push_back(0.0);
  out.push_back(0.0);
  out.push_back(0.0);
  out.push_back(handle_y);
  out.push_back(10.0);
  out.push_back(0.0);
  out.push_back(0.0);
  out.push_back(handle_y);
  out.push_back(0.0);
  out.push_back(0.0);
  return out;
}

double CubicScalar(double t, double p0, double p1, double p2, double p3) {
  const double omt = 1.0 - t;
  return omt * omt * omt * p0 +
         3.0 * omt * omt * t * p1 +
         3.0 * omt * t * t * p2 +
         t * t * t * p3;
}

double TemporalBezierProgress(double alpha,
                              double out_influence,
                              double in_influence) {
  double lo = 0.0;
  double hi = 1.0;
  const double x1 = out_influence / 100.0;
  const double x2 = 1.0 - in_influence / 100.0;
  for (int iter = 0; iter < 40; ++iter) {
    const double mid = (lo + hi) * 0.5;
    const double x = CubicScalar(mid, 0.0, x1, x2, 1.0);
    if (x < alpha) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return CubicScalar((lo + hi) * 0.5, 0.0, 0.0, 1.0, 1.0);
}

double DefaultTemporalBezierProgress(double alpha) {
  return TemporalBezierProgress(alpha, 33.3, 33.3);
}

double ShapeFlatComponent(const std::vector<double>& flat,
                          int vertex,
                          int component) {
  const std::size_t idx =
      static_cast<std::size_t>(2 + vertex * 6 + component);
  return idx < flat.size() ? flat[idx] : 0.0;
}

bool MaskedSegmentFeasible(const bbsolver::PropertySamples& ps,
                           const std::vector<int>& slots,
                           int i,
                           int j,
                           double tolerance) {
  if (i < 0 || j <= i || j >= static_cast<int>(ps.samples.size())) {
    return false;
  }
  struct ProgressCandidate {
    double out_influence = 33.3;
    double in_influence = 33.3;
    bool linear = false;
  };
  const std::vector<ProgressCandidate> candidates = {
      {33.3, 33.3, true},
      {90.0, 10.0, false},
      {10.0, 90.0, false},
      {33.3, 33.3, false},
  };
  const double t0 = ps.samples[static_cast<std::size_t>(i)].t_sec;
  const double t1 = ps.samples[static_cast<std::size_t>(j)].t_sec;
  if (!(t1 > t0)) {
    return false;
  }
  for (const ProgressCandidate& candidate : candidates) {
    bool ok = true;
    for (int sample_idx = i; sample_idx <= j && ok; ++sample_idx) {
      const double t = ps.samples[static_cast<std::size_t>(sample_idx)].t_sec;
      const double alpha = std::clamp((t - t0) / (t1 - t0), 0.0, 1.0);
      const double progress =
          candidate.linear
              ? alpha
              : TemporalBezierProgress(alpha,
                                       candidate.out_influence,
                                       candidate.in_influence);
      for (int slot : slots) {
        for (int component = 0; component < 2; ++component) {
          const double a = ShapeFlatComponent(
              ps.samples[static_cast<std::size_t>(i)].v, slot, component);
          const double b = ShapeFlatComponent(
              ps.samples[static_cast<std::size_t>(j)].v, slot, component);
          const double expected = ShapeFlatComponent(
              ps.samples[static_cast<std::size_t>(sample_idx)].v,
              slot,
              component);
          const double actual = a + (b - a) * progress;
          if (std::abs(actual - expected) > tolerance) {
            ok = false;
            break;
          }
        }
        if (!ok) {
          break;
        }
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

int MinimalMaskedChannelKeyCount(const bbsolver::PropertySamples& ps,
                                 const std::vector<int>& slots,
                                 double tolerance) {
  const int n = static_cast<int>(ps.samples.size());
  if (n <= 0 || slots.empty()) {
    return 0;
  }
  constexpr int kInf = std::numeric_limits<int>::max() / 4;
  std::vector<int> dp(static_cast<std::size_t>(n), kInf);
  dp[0] = 1;
  for (int j = 1; j < n; ++j) {
    for (int i = 0; i < j; ++i) {
      if (dp[static_cast<std::size_t>(i)] >= kInf) {
        continue;
      }
      if (!MaskedSegmentFeasible(ps, slots, i, j, tolerance)) {
        continue;
      }
      dp[static_cast<std::size_t>(j)] =
          std::min(dp[static_cast<std::size_t>(j)],
                   dp[static_cast<std::size_t>(i)] + 1);
    }
  }
  return dp[static_cast<std::size_t>(n - 1)];
}

bbsolver::PropertySamples MakeHandleDriftFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/handle_drift";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 2;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 2.0 / 24.0;
  ps.samples_per_frame = 1;

  ps.samples.push_back({0.0 / 24.0, ShapeFlatTwoPointCurve(0.0)});
  ps.samples.push_back({1.0 / 24.0, ShapeFlatTwoPointCurve(20.0)});
  ps.samples.push_back({2.0 / 24.0, ShapeFlatTwoPointCurve(0.0)});
  return ps;
}

bbsolver::PropertySamples MakeBezierHandleFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/bezier_handle";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 2;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 4.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 4; ++idx) {
    const double alpha = static_cast<double>(idx) / 4.0;
    const double handle_y = 40.0 * DefaultTemporalBezierProgress(alpha);
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatTwoPointCurve(handle_y)});
  }
  return ps;
}

bbsolver::PropertySamples MakeRelaxedBezierSearchFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/relaxed_bezier_search";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 2;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 4.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 4; ++idx) {
    const double alpha = static_cast<double>(idx) / 4.0;
    const double handle_y =
        400.0 * TemporalBezierProgress(alpha, 25.075, 25.075);
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatTwoPointCurve(handle_y)});
  }
  return ps;
}

bbsolver::PropertySamples MakeRelaxedLinearHandleFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/relaxed_linear_handle";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 2;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 2.0 / 24.0;
  ps.samples_per_frame = 1;

  ps.samples.push_back({0.0 / 24.0, ShapeFlatTwoPointCurve(0.0)});
  ps.samples.push_back({1.0 / 24.0, ShapeFlatTwoPointCurve(120.0)});
  ps.samples.push_back({2.0 / 24.0, ShapeFlatTwoPointCurve(190.0)});
  return ps;
}

bbsolver::PropertySamples MakeOpposedSemanticTimingFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/opposed_semantic_timing";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 4;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 4.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 4; ++idx) {
    const double alpha = static_cast<double>(idx) / 4.0;
    const double left_x =
        80.0 * TemporalBezierProgress(alpha, 90.0, 10.0);
    const double right_x =
        40.0 + 80.0 * TemporalBezierProgress(alpha, 10.0, 90.0);
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatQuad(left_x, right_x)});
  }
  return ps;
}

std::vector<double> ShapeFlatRegularPolygon(int vertex_count, double offset_x) {
  std::vector<double> out;
  out.push_back(1.0);
  out.push_back(static_cast<double>(vertex_count));
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    const double theta =
        (2.0 * 3.14159265358979323846 * static_cast<double>(vertex)) /
        static_cast<double>(vertex_count);
    out.push_back(offset_x + 10.0 * std::cos(theta));
    out.push_back(10.0 * std::sin(theta));
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

bbsolver::PropertySamples MakeUniformSixVertexFixture() {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode/uniform_six_vertex";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions = 2 + 6 * 6;
  ps.t_start_sec = 0.0;
  ps.t_end_sec = 4.0 / 24.0;
  ps.samples_per_frame = 1;

  for (int idx = 0; idx <= 4; ++idx) {
    ps.samples.push_back({static_cast<double>(idx) / 24.0,
                          ShapeFlatRegularPolygon(
                              6, 2.0 * static_cast<double>(idx))});
  }
  return ps;
}

bbsolver::ShapeFlatMultiModeOptions BaseOptions() {
  bbsolver::ShapeFlatMultiModeOptions options;
  options.frame_fit_options.outline_tolerance = 0.01;
  options.frame_fit_options.max_subdivisions_per_segment = 2;
  options.max_regions = 2;
  options.max_gap_samples = 32;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  options.max_validation_samples = 10000;
  options.max_validation_work_units = 1000000;
  options.max_candidate_key_ratio = 1.0;
  return options;
}

void EnableDeepLandmarkDiagnostics(
    bbsolver::ShapeFlatLandmarkSubpathOptions* options) {
  options->diagnose_dense_runs = true;
  options->diagnose_segment_gaps = true;
  options->diagnose_outlier_slots = true;
  options->diagnose_mask_channels = true;
}

void TestAcceptedIndependentModeFixture() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatMultiModeOptions options = BaseOptions();

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);

  assert(keys.converged);
  assert(keys.keys.size() == 4);
  assert(keys.segments.size() == 3);
  assert(keys.max_err <= options.frame_fit_options.outline_tolerance + 1e-9);
  assert(keys.notes.find("shape_multimode_candidate") != std::string::npos);
  assert(keys.notes.find("region_ranges=0-2,2-4") != std::string::npos);
  assert(keys.notes.find("region_segment_checks=") != std::string::npos);
  assert(keys.notes.find("validation_work_units=") != std::string::npos);
  for (const bbsolver::SegmentReport& segment : keys.segments) {
    assert(segment.reason == "replacement_shape_multimode_linear_union");
  }
}

void TestCandidateKeyBudgetDiagnostic() {
  const bbsolver::PropertySamples fixture = MakeZigZagFixture();
  bbsolver::ShapeFlatMultiModeOptions options = BaseOptions();
  options.max_regions = 1;
  options.max_gap_samples = 4;
  options.max_candidate_key_ratio = 0.5;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);

  assert(!keys.converged);
  assert(keys.keys.size() == fixture.samples.size());
  assert(keys.notes.find("shape_multimode_candidate_key_budget_exceeded") !=
         std::string::npos);
  assert(keys.notes.find("union_keys=5") != std::string::npos);
  assert(keys.notes.find("source_samples=5") != std::string::npos);
}

void TestRegionBudgetDiagnostic() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatMultiModeOptions options = BaseOptions();
  options.max_region_segment_checks = 1;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);

  assert(!keys.converged);
  assert(keys.keys.empty());
  assert(keys.notes.find("shape_multimode_region_budget_exceeded") !=
         std::string::npos);
  assert(keys.notes.find("region_segment_checks=2") != std::string::npos);
  assert(keys.notes.find("max_region_segment_checks=1") != std::string::npos);
}

void TestValidationBudgetDiagnostic() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatMultiModeOptions options = BaseOptions();
  options.max_validation_samples = 4;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);

  assert(!keys.converged);
  assert(keys.keys.size() == 4);
  assert(keys.notes.find("shape_multimode_validation_budget_exceeded") !=
         std::string::npos);
  assert(keys.notes.find("union_keys=4") != std::string::npos);
  assert(keys.notes.find("source_samples=9") != std::string::npos);
  assert(keys.notes.find("region_ranges=0-2,2-4") != std::string::npos);
}

void TestLandmarkSubpathEmitterIsDefaultOff() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture);

  assert(subpaths.empty());
}

void TestLandmarkSubpathEmitterGroupsSortedSubpaths() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 2;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 2);
  for (std::size_t idx = 0; idx < subpaths.size(); ++idx) {
    const bbsolver::PropertyKeys& keys = subpaths[idx];
    assert(keys.property_id == fixture.property.id);
    assert(keys.converged);
    assert(keys.notes.find("landmark_subpath") != std::string::npos);
    assert(keys.notes.find("subpath_index=" + std::to_string(idx)) !=
           std::string::npos);
    assert(keys.notes.find("subpath_count=2") != std::string::npos);
    assert(keys.notes.find("source_samples=9") != std::string::npos);
    assert(keys.notes.find("key_count=3") != std::string::npos);
    assert(keys.notes.find("anchors=") != std::string::npos);
    assert(keys.notes.find("subpath_anchor_refinements=0") !=
           std::string::npos);
    assert(keys.notes.find("subpath_reconstruction_ok=true") !=
           std::string::npos);
    assert(keys.notes.find("subpath_reconstruction_max_outline_error=") !=
           std::string::npos);
    assert(keys.keys.size() == 3);
    assert(keys.keys.size() < fixture.samples.size());
    assert(!keys.keys.empty());
    assert(keys.keys.front().t_sec == fixture.samples.front().t_sec);
    assert(keys.keys.back().t_sec == fixture.samples.back().t_sec);
    assert(keys.keys.front().v.size() == 2 + 6 * 2);
    assert(static_cast<int>(std::llround(keys.keys.front().v[0])) == 0);
    assert(static_cast<int>(std::llround(keys.keys.front().v[1])) == 2);
    assert(keys.keys.front().v[4] == 0.0);
    assert(keys.keys.front().v[5] == 0.0);
    assert(keys.keys.front().v[12] == 0.0);
    assert(keys.keys.front().v[13] == 0.0);
    assert(keys.segments.size() + 1 == keys.keys.size());
    for (const bbsolver::SegmentReport& segment : keys.segments) {
      assert(segment.reason == "landmark_subpath");
      assert(segment.end_idx > segment.start_idx);
    }
  }
  assert(subpaths[0].notes.find("subpath_index=0") != std::string::npos);
  assert(subpaths[1].notes.find("subpath_index=1") != std::string::npos);
}

void TestLandmarkSubpathDefaultSkipsDeepDiagnostics() {
  const bbsolver::PropertySamples fixture = MakeZigZagFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 1;
  options.max_gap_samples = 4;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const std::string& notes = subpaths.front().notes;
  assert(notes.find("subpath_diagnostics=fast") != std::string::npos);
  assert(notes.find("subpath_dense_run_diagnostic=") == std::string::npos);
  assert(notes.find("subpath_segment_gap_hist=") == std::string::npos);
  assert(notes.find("subpath_segment_lower_bound_top=") == std::string::npos);
  assert(notes.find("subpath_outlier_vertex_top=") == std::string::npos);
  assert(notes.find("subpath_mask_channel_diagnostic=") == std::string::npos);
}

void TestLandmarkSubpathFastSummarySkipsPartitionSearch() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 2;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  options.fast_summary_only = true;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const std::string& notes = subpaths.front().notes;
  assert(notes.find("vertex_range=0-4") != std::string::npos);
  assert(notes.find("subpath_partition_chosen_ranges=0-4") !=
         std::string::npos);
  assert(notes.find("subpath_partition_interval_evaluations=1") !=
         std::string::npos);
  assert(notes.find("subpath_fast_summary=true") != std::string::npos);
  assert(notes.find("subpath_diagnostics=fast") != std::string::npos);
}

void TestVisibleShapeChannelProtocolUsesReplacementPrefix() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 2;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  options.fast_summary_only = true;
  options.emit_visible_shape_channels = true;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const std::string& notes = subpaths.front().notes;
  assert(notes.find("shape_channel_subpath;") == 0);
  assert(notes.find("landmark_subpath;") == std::string::npos);
  assert(notes.find("visible_channel=true") != std::string::npos);
  assert(notes.find("visible_protocol=shape_channel_subpath_v1") !=
         std::string::npos);
  assert(notes.find("visible_replaces_source=true") != std::string::npos);
  assert(notes.find("visibility=shape_group_full") != std::string::npos);
  assert(notes.find("visible_renderable=true") != std::string::npos);
  assert(notes.find("subpath_fast_summary=true") != std::string::npos);
}

void TestPartialVisibleShapeChannelsAreProbeOnly() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 2;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  options.emit_visible_shape_channels = true;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 2);
  for (const bbsolver::PropertyKeys& keys : subpaths) {
    const std::string& notes = keys.notes;
    assert(notes.find("shape_channel_subpath;") == 0);
    assert(notes.find("visible_channel=true") != std::string::npos);
    assert(notes.find("visibility=probe_only") != std::string::npos);
    assert(notes.find("visible_renderable=false") != std::string::npos);
    assert(notes.find("visible_channel_mode=partial_subpath_probe") !=
           std::string::npos);
    assert(notes.find("reason=partial_shape_channel_not_ae_ready") !=
           std::string::npos);
  }
}

void TestVisibleChannelProbeReportsForcedContiguousCandidates() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 2;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  options.fast_summary_only = true;
  options.probe_visible_channels = true;
  options.visible_baseline_keys = 100;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const std::string& notes = subpaths.front().notes;
  assert(notes.find("landmark_subpath;") == 0);
  assert(notes.find("shape_channel_subpath;") == std::string::npos);
  assert(subpaths.front().keys.empty());
  assert(notes.find("subpath_partition=visible_channel_probe_only") !=
         std::string::npos);
  assert(notes.find("visible_channel_probe=done") != std::string::npos);
  assert(notes.find("source_visible_key_baseline=100") != std::string::npos);
  assert(notes.find("best_contiguous_2_channel_keys=") != std::string::npos);
  assert(notes.find("best_contiguous_3_channel_keys=") != std::string::npos);
  assert(notes.find("best_contiguous_4_channel_keys=") != std::string::npos);
  assert(notes.find("selected_visible_representation=contiguous_") !=
         std::string::npos);
  assert(notes.find("visible_channel_keys=") != std::string::npos);
  assert(notes.find("visible_improvement_status=accepted_key_reduction") !=
         std::string::npos);
  assert(notes.find("visible_channel_composite_max_outline_error=") !=
         std::string::npos);
}

void TestLandmarkSubpathReconstructionDiagnosticCatchesHandleDrift() {
  const bbsolver::PropertySamples fixture = MakeHandleDriftFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 1;
  options.max_gap_samples = 4;
  options.region_tolerance = 0.5;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.keys.size() == fixture.samples.size());
  assert(keys.notes.find("key_count=3") != std::string::npos);
  assert(keys.notes.find("anchors=0,1,2") != std::string::npos);
  assert(keys.notes.find("subpath_anchor_refinements=1") !=
         std::string::npos);
  assert(keys.notes.find("subpath_reconstruction_ok=true") !=
         std::string::npos);
  assert(keys.notes.find("subpath_reconstruction_worst_sample=1") ==
         std::string::npos);
  assert(keys.notes.find("subpath_reconstruction_samples=3") !=
         std::string::npos);
}

void TestLandmarkSubpathTemporalSolverReducesBezierHandleKeys() {
  const bbsolver::PropertySamples fixture = MakeBezierHandleFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 1;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.5;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.keys.size() == 2);
  assert(keys.keys.size() < fixture.samples.size());
  assert(keys.notes.find("key_count=2") != std::string::npos);
  assert(keys.notes.find("anchors=0,4") != std::string::npos);
  assert(keys.notes.find("subpath_temporal_solver=accepted") !=
         std::string::npos);
  assert(keys.notes.find("subpath_reconstruction_ok=true") !=
         std::string::npos);
  assert(keys.segments.size() == 1);
  assert(keys.segments.front().reason.find("landmark_subpath_temporal_") !=
         std::string::npos);
}

void TestLandmarkSubpathTemporalSolverUsesRelaxedEndpoints() {
  const bbsolver::PropertySamples fixture = MakeRelaxedLinearHandleFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 1;
  options.max_gap_samples = 4;
  options.region_tolerance = 15.0;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.keys.size() == 2);
  assert(keys.notes.find("key_count=2") != std::string::npos);
  assert(keys.notes.find("anchors=0,2") != std::string::npos);
  assert(keys.notes.find("subpath_temporal_solver=accepted") !=
         std::string::npos);
  assert(keys.notes.find("subpath_reconstruction_ok=true") !=
         std::string::npos);
  assert(keys.segments.size() == 1);
  assert(keys.segments.front().reason ==
         "landmark_subpath_temporal_relaxed_linear_ok");
  assert(keys.keys.front().v != fixture.samples.front().v);
  assert(keys.keys.back().v != fixture.samples.back().v);
}

void TestLandmarkSubpathTemporalSolverSearchesRelaxedBezierInfluence() {
  const bbsolver::PropertySamples fixture = MakeRelaxedBezierSearchFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 1;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.5;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.keys.size() == 2);
  assert(keys.notes.find("key_count=2") != std::string::npos);
  assert(keys.notes.find("anchors=0,4") != std::string::npos);
  assert(keys.notes.find("subpath_temporal_solver=accepted") !=
         std::string::npos);
  assert(keys.notes.find("subpath_reconstruction_ok=true") !=
         std::string::npos);
  assert(keys.segments.size() == 1);
  assert(keys.segments.front().reason ==
         "landmark_subpath_temporal_relaxed_bezier_search_ok");
}

void TestLandmarkSubpathDenseRunDiagnosticExplainsSingletons() {
  const bbsolver::PropertySamples fixture = MakeZigZagFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 1;
  options.max_gap_samples = 4;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  EnableDeepLandmarkDiagnostics(&options);

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.keys.size() == fixture.samples.size());
  assert(keys.notes.find("subpath_diagnostics=deep") != std::string::npos);
  assert(keys.notes.find("subpath_dense_run_checks=") != std::string::npos);
  assert(keys.notes.find("subpath_dense_run_diagnostic=0-4:") !=
         std::string::npos);
  assert(keys.notes.find("dominant=infeasible_shape_morph_chord") !=
         std::string::npos);
  assert(keys.notes.find("inference=one_shared_progress_chord_infeasible") !=
         std::string::npos);
  assert(keys.notes.find("subpath_segment_gap_hist=1:4") !=
         std::string::npos);
  assert(keys.notes.find("subpath_segment_gap_max=1") !=
         std::string::npos);
  assert(keys.notes.find("subpath_segment_rejection_checks=3") !=
         std::string::npos);
  assert(keys.notes.find("subpath_segment_lower_bound_top=chord_infeasible:3") !=
         std::string::npos);
  assert(keys.notes.find("subpath_segment_rejection_top=chord_infeasible:3") !=
         std::string::npos);
  assert(keys.notes.find("subpath_semantic_split=not_selected") !=
         std::string::npos);
  assert(keys.notes.find("reason=single_base_region") != std::string::npos);
  assert(keys.notes.find(
             "subpath_dense_run_next_representation=per_region_independent_timing_or_extra_shape_channels") !=
         std::string::npos);
}

void TestLandmarkSubpathSemanticSplitCanUseIndependentTimingChannels() {
  const bbsolver::PropertySamples fixture = MakeOpposedSemanticTimingFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 2;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  EnableDeepLandmarkDiagnostics(&options);

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 2);
  int total_keys = 0;
  for (const bbsolver::PropertyKeys& keys : subpaths) {
    assert(keys.converged);
    total_keys += static_cast<int>(keys.keys.size());
    assert(keys.notes.find("subpath_representation=semantic_split") !=
           std::string::npos);
    assert(keys.notes.find(
               "subpath_semantic_split_reason=dense_full_range_one_shared_progress_chord_infeasible") !=
           std::string::npos);
    assert(keys.notes.find("subpath_reconstruction_ok=true") !=
           std::string::npos);
    assert(keys.notes.find("subpath_partition_chosen_ranges=0-2,2-4") !=
           std::string::npos);
  }
  assert(total_keys <= 6);
  assert(subpaths[0].notes.find("vertex_range=0-2") != std::string::npos);
  assert(subpaths[1].notes.find("vertex_range=2-4") != std::string::npos);
}

void TestLandmarkSubpathSemanticSplitReportsWorseBestNonfull() {
  const bbsolver::PropertySamples fixture = MakeZigZagFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 2;
  options.max_gap_samples = 4;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  EnableDeepLandmarkDiagnostics(&options);

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.notes.find("subpath_semantic_split=not_selected") !=
         std::string::npos);
  assert(keys.notes.find("reason=best_nonfull_not_lower_key_count") !=
         std::string::npos);
  assert(keys.notes.find("subpath_semantic_split_candidate=best_nonfull_partition") !=
         std::string::npos);
  assert(keys.notes.find("subpath_semantic_split_full_key_count=") !=
         std::string::npos);
  assert(keys.notes.find("subpath_semantic_split_key_count=") !=
         std::string::npos);
  assert(keys.notes.find("subpath_semantic_split_ranges=") !=
         std::string::npos);
}

void TestLandmarkSubpathOutlierVertexDiagnostics() {
  const bbsolver::PropertySamples fixture = MakeSingleOutlierVertexFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 3;
  options.max_gap_samples = 4;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;
  EnableDeepLandmarkDiagnostics(&options);

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.notes.find("subpath_outlier_vertex_checks=") !=
         std::string::npos);
  assert(keys.notes.find("subpath_outlier_vertex_top=1:") !=
         std::string::npos);
  assert(keys.notes.find("subpath_outlier_partition=not_selected") !=
         std::string::npos);
  assert(keys.notes.find("reason=best_outlier_not_lower_key_count") !=
         std::string::npos);
  assert(keys.notes.find("subpath_outlier_slot=1") != std::string::npos);
  assert(keys.notes.find("subpath_outlier_ranges=0-1,1-2,2-4") !=
         std::string::npos);
}

void TestNonContiguousMaskedChannelsBeatCurrentContiguousProtocol() {
  const bbsolver::PropertySamples fixture =
      MakeInterleavedMaskedChannelFixture();
  constexpr double kTolerance = 1e-6;

  const int full_shape_keys =
      MinimalMaskedChannelKeyCount(fixture, {0, 1, 2, 3}, kTolerance);
  const int masked_channel_keys =
      MinimalMaskedChannelKeyCount(fixture, {0, 2}, kTolerance) +
      MinimalMaskedChannelKeyCount(fixture, {1, 3}, kTolerance);
  const int contiguous_pair_keys =
      MinimalMaskedChannelKeyCount(fixture, {0, 1}, kTolerance) +
      MinimalMaskedChannelKeyCount(fixture, {2, 3}, kTolerance);
  const int singleton_contiguous_keys =
      MinimalMaskedChannelKeyCount(fixture, {0}, kTolerance) +
      MinimalMaskedChannelKeyCount(fixture, {1}, kTolerance) +
      MinimalMaskedChannelKeyCount(fixture, {2}, kTolerance) +
      MinimalMaskedChannelKeyCount(fixture, {3}, kTolerance);

  assert(MinimalMaskedChannelKeyCount(fixture, {0, 2}, kTolerance) == 2);
  assert(MinimalMaskedChannelKeyCount(fixture, {1, 3}, kTolerance) == 2);
  assert(masked_channel_keys == 4);
  assert(full_shape_keys > masked_channel_keys);
  assert(contiguous_pair_keys > masked_channel_keys);
  assert(singleton_contiguous_keys > masked_channel_keys);
}

void TestLandmarkSubpathReportsMaskChannelDiagnostic() {
  const bbsolver::PropertySamples fixture =
      MakeInterleavedMaskedChannelFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 1;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.05;
  options.max_region_segment_checks = 20000;
  EnableDeepLandmarkDiagnostics(&options);

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  const std::string& notes = keys.notes;
  assert(notes.find("subpath_mask_channel_diagnostic=candidate") !=
         std::string::npos);
  assert(notes.find("subpath_mask_channel_groups=0,2:k=2|1,3:k=2") !=
         std::string::npos);
  assert(notes.find("subpath_mask_channel_grouped_keys=4") !=
         std::string::npos);
  assert(notes.find("subpath_mask_channel_singleton_keys=8") !=
         std::string::npos);
  assert(notes.find("subpath_mask_channel_full_key_count=") !=
         std::string::npos);
  assert(notes.find(
             "subpath_mask_channel_protocol=requires_extra_shape_channel_masks") !=
         std::string::npos);
  assert(notes.find(
             "subpath_mask_channel_acceptance=requires_full_source_outline_validation") !=
         std::string::npos);
  assert(keys.keys.size() > 4);
  assert(notes.find("subpath_mask_channel_accepted") == std::string::npos);
  assert(notes.find("subpath_mask_channel_diagnostic=cancelled") ==
         std::string::npos);
}

void TestRecombinedRegionTemporalBeatsPlainUnionWithRelaxedAnchors() {
  const bbsolver::PropertySamples fixture =
      MakeRelaxedRecombinedRegionalFixture();
  bbsolver::ShapeFlatMultiModeOptions options = BaseOptions();
  options.max_regions = 2;
  options.max_gap_samples = 4;
  options.region_tolerance = 2.5;
  options.frame_fit_options.outline_tolerance = 2.5;
  options.frame_fit_options.max_subdivisions_per_segment = 8;
  options.max_region_segment_checks = 10000;
  options.max_candidate_key_ratio = 1.0;

  const bbsolver::PropertyKeys keys =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);

  assert(keys.converged);
  assert(keys.keys.size() == 2);
  assert(keys.segments.size() == 1);
  assert(keys.max_err <= options.frame_fit_options.outline_tolerance + 1e-9);
  assert(keys.notes.find("recombined_region_temporal=accepted") !=
         std::string::npos);
  assert(keys.notes.find("recombined_region_temporal_keys=2") !=
         std::string::npos);
  assert(keys.notes.find("recombined_region_temporal_region_key_counts=2,2") !=
         std::string::npos);
  assert(keys.notes.find("union_keys=3") != std::string::npos);
  assert(keys.notes.find("region_ranges=0-2,2-4") != std::string::npos);
  assert(keys.segments.front().reason ==
         "replacement_shape_multimode_recombined_region_temporal_linear");
  assert(keys.keys.front().interp_out == bbsolver::InterpType::Linear);
  assert(keys.keys.back().interp_in == bbsolver::InterpType::Linear);
  assert(keys.keys.front().v != fixture.samples.front().v);
  assert(keys.keys.back().v != fixture.samples.back().v);
}

void TestLandmarkSubpathPartitionMergesCheapIntervals() {
  const bbsolver::PropertySamples fixture = MakeUniformSixVertexFixture();
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.enabled = true;
  options.max_regions = 4;
  options.max_gap_samples = 8;
  options.region_tolerance = 0.01;
  options.max_region_segment_checks = 10000;

  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, options);

  assert(subpaths.size() == 1);
  const bbsolver::PropertyKeys& keys = subpaths.front();
  assert(keys.property_id == fixture.property.id);
  assert(keys.converged);
  assert(keys.keys.size() == 2);
  assert(keys.keys.front().v.size() == 2 + 6 * 6);
  assert(static_cast<int>(std::llround(keys.keys.front().v[0])) == 1);
  assert(static_cast<int>(std::llround(keys.keys.front().v[1])) == 6);
  assert(keys.notes.find("subpath_count=1") != std::string::npos);
  assert(keys.notes.find("vertex_range=0-6") != std::string::npos);
  assert(keys.notes.find("key_count=2") != std::string::npos);
  assert(keys.notes.find("subpath_partition=key_count_dp") !=
         std::string::npos);
  assert(keys.notes.find("subpath_partition_base_ranges=0-1,1-3,3-4,4-6") !=
         std::string::npos);
  assert(keys.notes.find("subpath_partition_chosen_ranges=0-6") !=
         std::string::npos);
  assert(keys.notes.find("subpath_partition_boundary_count=7") !=
         std::string::npos);
  assert(keys.notes.find("subpath_partition_interval_evaluations=21") !=
         std::string::npos);
  assert(keys.notes.find("subpath_reconstruction_ok=true") !=
         std::string::npos);
}

void TestLandmarkSubpathEmitterDoesNotChangeNormalCandidate() {
  const bbsolver::PropertySamples fixture = MakeIndependentModeFixture();
  bbsolver::ShapeFlatMultiModeOptions options = BaseOptions();

  const bbsolver::PropertyKeys before =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);
  bbsolver::ShapeFlatLandmarkSubpathOptions subpath_options;
  subpath_options.enabled = true;
  subpath_options.max_regions = 2;
  const std::vector<bbsolver::PropertyKeys> subpaths =
      bbsolver::EmitShapeFlatLandmarkSubpathKeys(fixture, subpath_options);
  const bbsolver::PropertyKeys after =
      bbsolver::SolveShapeFlatMultiModeTemporal(fixture, fixture, options);

  assert(!subpaths.empty());
  assert(before.converged);
  assert(after.converged);
  assert(before.keys.size() == after.keys.size());
  assert(before.segments.size() == after.segments.size());
  assert(before.notes.find("landmark_subpath") == std::string::npos);
  assert(after.notes.find("landmark_subpath") == std::string::npos);
}

}  // namespace

int main() {
  TestAcceptedIndependentModeFixture();
  TestCandidateKeyBudgetDiagnostic();
  TestRegionBudgetDiagnostic();
  TestValidationBudgetDiagnostic();
  TestLandmarkSubpathEmitterIsDefaultOff();
  TestLandmarkSubpathEmitterGroupsSortedSubpaths();
  TestLandmarkSubpathDefaultSkipsDeepDiagnostics();
  TestLandmarkSubpathFastSummarySkipsPartitionSearch();
  TestVisibleShapeChannelProtocolUsesReplacementPrefix();
  TestPartialVisibleShapeChannelsAreProbeOnly();
  TestVisibleChannelProbeReportsForcedContiguousCandidates();
  TestLandmarkSubpathReconstructionDiagnosticCatchesHandleDrift();
  TestLandmarkSubpathTemporalSolverReducesBezierHandleKeys();
  TestLandmarkSubpathTemporalSolverUsesRelaxedEndpoints();
  TestLandmarkSubpathTemporalSolverSearchesRelaxedBezierInfluence();
  TestLandmarkSubpathDenseRunDiagnosticExplainsSingletons();
  TestLandmarkSubpathSemanticSplitCanUseIndependentTimingChannels();
  TestLandmarkSubpathSemanticSplitReportsWorseBestNonfull();
  TestLandmarkSubpathOutlierVertexDiagnostics();
  TestNonContiguousMaskedChannelsBeatCurrentContiguousProtocol();
  TestLandmarkSubpathReportsMaskChannelDiagnostic();
  TestRecombinedRegionTemporalBeatsPlainUnionWithRelaxedAnchors();
  TestLandmarkSubpathPartitionMergesCheapIntervals();
  TestLandmarkSubpathEmitterDoesNotChangeNormalCandidate();
  return 0;
}
