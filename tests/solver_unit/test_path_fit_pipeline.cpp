#include "bbsolver/path/fit/path_fit_pipeline.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/path/replacement/path_replacement_acceptance.hpp"

#include <cstddef>

// Stage-2 temporal compression checks.
#include "bbsolver/metrics/ae_curve.hpp"
#include "bbsolver/dp/dp_placer.hpp"
#include "bbsolver/path/temporal/path_temporal_validation.hpp"
#include "bbsolver/fit/segment_fitter.hpp"

// Cross-verification: call FitShapeFlatFrame directly and feed its output into
// the pipeline to verify dimension/correspondence contracts.
#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

// Integration smoke: verify regime samples are consumable by the decompose step.
#include "bbsolver/path/decompose/path_decompose.hpp"
#include "bbsolver/path/replacement/path_replacement_preference.hpp"
#include "bbsolver/shape/sharp_corner_policy.hpp"
#include "bbsolver/path/geometry/path_geometry_refinement.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a minimal valid shape_flat vector with the given closed flag and
// vertex count. Vertex positions are spread along the X axis; tangents are
// zero so each vertex is a corner. The header (flat[0], flat[1]) is exact.
std::vector<double> MakeFlat(bool closed, int vertex_count) {
  std::vector<double> v;
  v.push_back(closed ? 1.0 : 0.0);
  v.push_back(static_cast<double>(vertex_count));
  for (int i = 0; i < vertex_count; ++i) {
    // x, y, in_x, in_y, out_x, out_y
    v.push_back(static_cast<double>(i) * 10.0);
    v.push_back(0.0);
    v.push_back(0.0);
    v.push_back(0.0);
    v.push_back(0.0);
    v.push_back(0.0);
  }
  return v;
}

// Build a shape_flat sample stream where frame i has vertex_counts[i] vertices.
// All frames share the same closed flag.
bbsolver::PropertySamples MakeSamples(bool closed,
                                       const std::vector<int>& vertex_counts) {
  bbsolver::PropertySamples ps;
  ps.property.id          = "unit/pipeline";
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = vertex_counts.empty()
                                ? 2
                                : 2 + 6 * vertex_counts[0];  // nominal, will be updated
  ps.samples_per_frame    = 1;
  ps.t_start_sec          = 0.0;
  ps.t_end_sec            = static_cast<double>(vertex_counts.size() - 1) / 24.0;

  for (std::size_t i = 0; i < vertex_counts.size(); ++i) {
    bbsolver::Sample s;
    s.t_sec = static_cast<double>(i) / 24.0;
    s.v     = MakeFlat(closed, vertex_counts[i]);
    ps.samples.push_back(std::move(s));
  }
  return ps;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestUniformTopology() {
  // All frames have the same vertex count -> one regime, no splits.
  const std::vector<int> counts = {8, 8, 8, 8, 8};
  const bbsolver::PropertySamples ps = MakeSamples(true, counts);

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(result.ok);
  assert(result.all_stable);
  assert(result.regimes.size() == 1);

  const bbsolver::StablePathRegime& r = result.regimes[0];
  assert(r.vertex_count == 8);
  assert(r.dimensions == 2 + 6 * 8);
  assert(r.closed);
  assert(r.start_sample_idx == 0);
  assert(r.end_sample_idx == 4);
  assert(r.samples.samples.size() == 5);
  assert(r.samples.property.dimensions == 2 + 6 * 8);

  // Downstream check: the regime's samples should have dimensions < 314 and
  // be readable by DecomposePathBundle.
  assert(r.samples.property.dimensions < 314);
  const bbsolver::PathDecomposeResult decomposed = bbsolver::DecomposePathBundle(r.samples);
  assert(decomposed.is_shape_flat);
  assert(decomposed.stable_topology);
  assert(decomposed.closed);
  assert(decomposed.vertex_count == 8);
  assert(decomposed.children.size() == 3 * 8);
}

static void TestTopologyChanges() {
  // vertex count sequence: 8,8,9,9,8 -> three regimes
  const std::vector<int> counts = {8, 8, 9, 9, 8};
  const bbsolver::PropertySamples ps = MakeSamples(true, counts);

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(result.ok);
  assert(!result.all_stable);
  assert(result.regimes.size() == 3);

  // Regime 0: samples 0-1, vertex_count=8
  {
    const bbsolver::StablePathRegime& r = result.regimes[0];
    assert(r.start_sample_idx == 0);
    assert(r.end_sample_idx   == 1);
    assert(r.vertex_count     == 8);
    assert(r.dimensions       == 2 + 6 * 8);
    assert(r.samples.samples.size() == 2);
    assert(r.samples.property.dimensions == 2 + 6 * 8);
    // t_start / t_end match first and last sample in regime
    assert(std::abs(r.samples.t_start_sec - ps.samples[0].t_sec) < 1e-9);
    assert(std::abs(r.samples.t_end_sec   - ps.samples[1].t_sec) < 1e-9);
  }

  // Regime 1: samples 2-3, vertex_count=9
  {
    const bbsolver::StablePathRegime& r = result.regimes[1];
    assert(r.start_sample_idx == 2);
    assert(r.end_sample_idx   == 3);
    assert(r.vertex_count     == 9);
    assert(r.dimensions       == 2 + 6 * 9);
    assert(r.samples.samples.size() == 2);
    assert(r.samples.property.dimensions == 2 + 6 * 9);
    assert(std::abs(r.samples.t_start_sec - ps.samples[2].t_sec) < 1e-9);
    assert(std::abs(r.samples.t_end_sec   - ps.samples[3].t_sec) < 1e-9);
  }

  // Regime 2: sample 4, vertex_count=8
  {
    const bbsolver::StablePathRegime& r = result.regimes[2];
    assert(r.start_sample_idx == 4);
    assert(r.end_sample_idx   == 4);
    assert(r.vertex_count     == 8);
    assert(r.dimensions       == 2 + 6 * 8);
    assert(r.samples.samples.size() == 1);
    assert(r.samples.property.dimensions == 2 + 6 * 8);
  }

  // All closed flags preserved.
  for (const bbsolver::StablePathRegime& r : result.regimes) {
    assert(r.closed);
    for (const bbsolver::Sample& s : r.samples.samples) {
      assert(static_cast<int>(std::llround(s.v[0])) == 1);
    }
  }

  // No regime is the original high-dimensional stream.
  for (const bbsolver::StablePathRegime& r : result.regimes) {
    assert(r.dimensions < 314);
  }
}

static void TestSingleSample() {
  // One frame only -> valid regime of size 1.
  const bbsolver::PropertySamples ps = MakeSamples(false, {12});

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(result.ok);
  assert(result.all_stable);
  assert(result.regimes.size() == 1);

  const bbsolver::StablePathRegime& r = result.regimes[0];
  assert(r.vertex_count == 12);
  assert(r.dimensions   == 2 + 6 * 12);
  assert(!r.closed);
  assert(r.samples.samples.size() == 1);
}

static void TestOpenPath() {
  // Open paths should work identically to closed.
  const std::vector<int> counts = {5, 5, 6, 5};
  const bbsolver::PropertySamples ps = MakeSamples(false, counts);

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(result.ok);
  assert(!result.all_stable);
  assert(result.regimes.size() == 3);

  assert(result.regimes[0].vertex_count == 5);
  assert(result.regimes[0].samples.samples.size() == 2);

  assert(result.regimes[1].vertex_count == 6);
  assert(result.regimes[1].samples.samples.size() == 1);

  assert(result.regimes[2].vertex_count == 5);
  assert(result.regimes[2].samples.samples.size() == 1);

  for (const bbsolver::StablePathRegime& r : result.regimes) {
    assert(!r.closed);
  }
}

static void TestClosedFlagFlipRejected() {
  // A closed-flag flip anywhere must set ok=false.
  bbsolver::PropertySamples ps = MakeSamples(true, {8, 8, 8});
  // Corrupt sample 1: make it open.
  ps.samples[1].v[0] = 0.0;  // closed=false

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(!result.ok);
  assert(result.regimes.empty());
  // Notes should mention the flip.
  assert(result.notes.find("closed flag") != std::string::npos);
}

static void TestEmptyStreamRejected() {
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(!result.ok);
  assert(result.regimes.empty());
}

static void TestNonShapeFlatRejected() {
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Scalar;
  ps.property.units_label = "";

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(!result.ok);
  assert(result.regimes.empty());
}

static void TestMalformedHeaderRejected() {
  // Header claims vertex_count=8 but vector is too short.
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  bbsolver::Sample s;
  s.t_sec = 0.0;
  s.v     = {1.0, 8.0, 0.0};  // truncated: needs 2+6*8=50 elements
  ps.samples.push_back(s);

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(!result.ok);
  assert(result.regimes.empty());
}

static void TestDimensionsNotOriginalHighDim() {
  // The key integration guarantee: even a 52-vertex source, after being split
  // into a small-vertex stream, the regime's property.dimensions must be
  // 2 + 6 * fitted_K, not 2 + 6 * 52 = 314.
  //
  // Simulate: caller assembled per-frame 8-vertex fitted frames from the
  // 52-vertex noodle. Regime must report dimensions=50, not 314.
  bbsolver::PropertySamples ps = MakeSamples(true, {8, 8, 8});
  ps.property.dimensions = 314;  // original high-dim, as it would come from AE
  ps.property.display_name = "Path 1";

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(result.ok);
  assert(result.all_stable);

  const bbsolver::StablePathRegime& r = result.regimes[0];
  // Must be 2 + 6 * 8 = 50, not 314.
  assert(r.dimensions == 50);
  assert(r.samples.property.dimensions == 50);
  // display_name and other metadata should be inherited.
  assert(r.samples.property.display_name == "Path 1");
}

static void TestPropertyMetadataPreserved() {
  // All property metadata other than dimensions should survive into each regime.
  bbsolver::PropertySamples ps = MakeSamples(true, {6, 6, 7, 7});
  ps.property.id           = "L3/ADBE Mask Shape";
  ps.property.display_name = "Mask Path";
  ps.property.layer_path   = "Comp/Layer/Mask Path";
  ps.samples_per_frame     = 2;

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);

  assert(result.ok);
  assert(result.regimes.size() == 2);
  for (const bbsolver::StablePathRegime& r : result.regimes) {
    assert(r.samples.property.id           == "L3/ADBE Mask Shape");
    assert(r.samples.property.display_name == "Mask Path");
    assert(r.samples.property.layer_path   == "Comp/Layer/Mask Path");
    assert(r.samples.property.kind         == bbsolver::ValueKind::Custom);
    assert(r.samples.property.units_label  == "shape_flat");
    assert(r.samples.samples_per_frame     == 2);
  }
}

static void TestDecomposeIntegration() {
  // Verify that a uniform-topology regime can be passed directly to
  // DecomposePathBundle without modification, and that it reports the correct
  // reduced vertex count.  This is the key temporal-consumer contract.
  const std::vector<int> counts = {4, 4, 4};
  bbsolver::PropertySamples ps = MakeSamples(true, counts);
  ps.property.dimensions = 314;  // simulate high-dim original

  const bbsolver::PathCorrespondenceResult result = bbsolver::BuildStableRegimes(ps);
  assert(result.ok);
  assert(result.all_stable);

  const bbsolver::StablePathRegime& r = result.regimes[0];
  assert(r.dimensions == 2 + 6 * 4);

  // Key assertion: temporal solver would see 26 dimensions, not 314.
  assert(r.samples.property.dimensions == 26);
  assert(r.samples.property.dimensions < 314);

  const bbsolver::PathDecomposeResult decomposed = bbsolver::DecomposePathBundle(r.samples);
  assert(decomposed.is_shape_flat);
  assert(decomposed.stable_topology);
  assert(decomposed.vertex_count == 4);
  // 3 children per vertex (vert, in, out) each 2D.
  assert(decomposed.children.size() == 3 * 4);
  for (const bbsolver::PathChildSamples& child : decomposed.children) {
    assert(child.samples.property.dimensions == 2);
    assert(child.samples.samples.size() == 3);
  }
}

// ---------------------------------------------------------------------------
// BuildSingleStableRegime tests
// ---------------------------------------------------------------------------

static void TestSingleRegimeUniform() {
  // All frames same vertex count -> ok=true, regime populated.
  const bbsolver::PropertySamples ps = MakeSamples(true, {8, 8, 8, 8});

  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps);

  assert(result.ok);
  assert(result.reason.empty());
  assert(result.regime.vertex_count == 8);
  assert(result.regime.dimensions == 2 + 6 * 8);
  assert(result.regime.samples.samples.size() == 4);
  assert(result.regime.samples.property.dimensions == 2 + 6 * 8);
}

static void TestSingleRegimeMultiTopologyFails() {
  // Topology changes -> ok=false, reason describes the split.
  const bbsolver::PropertySamples ps = MakeSamples(true, {8, 8, 9, 9, 8});

  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps);

  assert(!result.ok);
  assert(!result.reason.empty());
  // The reason must mention the split so the caller can log it clearly.
  assert(result.reason.find("regime") != std::string::npos);
}

static void TestSingleRegimeClosedFlagFlipFails() {
  bbsolver::PropertySamples ps = MakeSamples(true, {6, 6, 6});
  ps.samples[1].v[0] = 0.0;  // flip closed to open mid-stream

  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps);

  assert(!result.ok);
  assert(result.reason.find("closed flag") != std::string::npos);
}

static void TestSingleRegimeTargetVertexCountMatch() {
  // When target_vertex_count matches the regime's actual count -> ok.
  const bbsolver::PropertySamples ps = MakeSamples(false, {6, 6, 6});

  bbsolver::SingleRegimeOptions opts;
  opts.target_vertex_count = 6;
  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps, opts);

  assert(result.ok);
  assert(result.regime.vertex_count == 6);
}

static void TestSingleRegimeTargetVertexCountMismatch() {
  // When target_vertex_count differs from the actual fitted count -> !ok.
  // This is the adaptation point for when FitShapeFlatFrame gains a fixed-K
  // option: set opts.target_vertex_count == that K to enforce the contract.
  const bbsolver::PropertySamples ps = MakeSamples(false, {6, 6, 6});

  bbsolver::SingleRegimeOptions opts;
  opts.target_vertex_count = 8;  // caller expected 8 but fitter produced 6
  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps, opts);

  assert(!result.ok);
  assert(result.reason.find("target_vertex_count") != std::string::npos);
}

static void TestSingleRegimeZeroTargetIsUnconstrained() {
  // Default opts (target_vertex_count=0) imposes no count constraint.
  const bbsolver::PropertySamples ps = MakeSamples(true, {4, 4, 4});

  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps);

  assert(result.ok);
  assert(result.regime.vertex_count == 4);
}

// ---------------------------------------------------------------------------
// Cross-verification: FitShapeFlatFrame output -> BuildSingleStableRegime
// ---------------------------------------------------------------------------

// Build a rectangle with many redundant collinear points on each side.
// With 12 intermediate vertices per edge, total = 4 corners + 4*12 = 52.
// After fitting with tight tolerance, RDP should retain only the 4 corners.
static std::vector<double> MakeRedundantRectFlat(bool closed) {
  const double w = 100.0;
  const double h = 50.0;
  const int mid_per_edge = 12;  // 12 intermediate points per edge -> 52 total

  std::vector<std::pair<double, double>> pts;
  auto push = [&](double x, double y) { pts.push_back({x, y}); };

  push(0.0, 0.0);
  for (int i = 1; i <= mid_per_edge; ++i) {
    push(w * static_cast<double>(i) / (mid_per_edge + 1), 0.0);
  }
  push(w, 0.0);
  for (int i = 1; i <= mid_per_edge; ++i) {
    push(w, h * static_cast<double>(i) / (mid_per_edge + 1));
  }
  push(w, h);
  for (int i = 1; i <= mid_per_edge; ++i) {
    push(w - w * static_cast<double>(i) / (mid_per_edge + 1), h);
  }
  push(0.0, h);
  if (closed) {
    for (int i = 1; i <= mid_per_edge; ++i) {
      push(0.0, h - h * static_cast<double>(i) / (mid_per_edge + 1));
    }
  }

  std::vector<double> flat;
  flat.push_back(closed ? 1.0 : 0.0);
  flat.push_back(static_cast<double>(pts.size()));
  for (const auto& [x, y] : pts) {
    flat.push_back(x);
    flat.push_back(y);
    flat.push_back(0.0);  // zero tangents -> corner detected -> locked
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
  }
  return flat;
}

static std::vector<double> MakeRedundantSquareFlat(double x,
                                                   double y,
                                                   double side,
                                                   double top_bulge = 0.0) {
  constexpr int mid_per_edge = 12;  // 4 corners + 4*12 redundant points = 52.
  constexpr double pi = 3.14159265358979323846;

  std::vector<std::pair<double, double>> pts;
  auto push = [&](double px, double py) { pts.push_back({px, py}); };

  push(x, y);
  for (int i = 1; i <= mid_per_edge; ++i) {
    const double u = static_cast<double>(i) / (mid_per_edge + 1);
    push(x + side * u, y);
  }
  push(x + side, y);
  for (int i = 1; i <= mid_per_edge; ++i) {
    const double u = static_cast<double>(i) / (mid_per_edge + 1);
    push(x + side, y + side * u);
  }
  push(x + side, y + side);
  for (int i = 1; i <= mid_per_edge; ++i) {
    const double u = static_cast<double>(i) / (mid_per_edge + 1);
    const double bulge = top_bulge * std::sin(pi * u);
    push(x + side * (1.0 - u), y + side + bulge);
  }
  push(x, y + side);
  for (int i = 1; i <= mid_per_edge; ++i) {
    const double u = static_cast<double>(i) / (mid_per_edge + 1);
    push(x, y + side * (1.0 - u));
  }

  std::vector<double> flat;
  flat.reserve(2 + 6 * pts.size());
  flat.push_back(1.0);
  flat.push_back(static_cast<double>(pts.size()));
  for (const auto& [px, py] : pts) {
    flat.push_back(px);
    flat.push_back(py);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
    flat.push_back(0.0);
  }
  return flat;
}

static bbsolver::CompInfo Stage2Comp(int n_frames, double fps = 24.0) {
  bbsolver::CompInfo comp;
  comp.fps = fps;
  comp.duration_sec = static_cast<double>(std::max(0, n_frames - 1)) / fps;
  comp.width = 1920;
  comp.height = 1080;
  comp.work_area_start_sec = 0.0;
  comp.work_area_end_sec = comp.duration_sec;
  comp.shutter_angle_deg = 0.0;
  return comp;
}

static bbsolver::PropertySamples MakeShapeFlatSamples(
    const std::string& id,
    const std::vector<std::pair<double, std::vector<double>>>& frames) {
  bbsolver::PropertySamples ps;
  ps.property.id = id;
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions =
      frames.empty() ? 0 : static_cast<int>(frames.front().second.size());
  ps.samples_per_frame = 1;
  if (!frames.empty()) {
    ps.t_start_sec = frames.front().first;
    ps.t_end_sec = frames.back().first;
  }

  for (const auto& [t_sec, flat] : frames) {
    bbsolver::Sample sample;
    sample.t_sec = t_sec;
    sample.v = flat;
    ps.samples.push_back(std::move(sample));
  }
  return ps;
}

static std::vector<double> ReducedSquareFromSource(const std::vector<double>& source,
                                                   double tolerance,
                                                   double* outline_error) {
  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = tolerance;
  const std::vector<double> corner_fractions = {0.0, 0.25, 0.5, 0.75};
  const bbsolver::PathFrameFitResult fit =
      bbsolver::FitShapeFlatFrameAtFractions(source, corner_fractions, opts);
  assert(fit.ok);
  assert(fit.applied);
  assert(fit.fitted_vertex_count == 4);
  assert(fit.max_outline_error <= tolerance + 1e-9);
  if (outline_error != nullptr) {
    *outline_error = fit.max_outline_error;
  }
  return fit.fitted;
}

static bbsolver::Key LinearShapeKey(double t_sec, const std::vector<double>& flat) {
  bbsolver::Key key;
  key.t_sec = t_sec;
  key.v = flat;
  key.interp_in = bbsolver::InterpType::Linear;
  key.interp_out = bbsolver::InterpType::Linear;
  key.temporal_ease_in = {{0.0, 33.3}};
  key.temporal_ease_out = {{0.0, 33.3}};
  return key;
}

struct Stage2TemporalFixture {
  bbsolver::PropertySamples source;
  bbsolver::PropertySamples reduced;
  bbsolver::CompInfo comp;
  double frame_outline_error = 0.0;
};

static Stage2TemporalFixture MakeCoherentStage2Fixture() {
  constexpr int n_frames = 25;
  constexpr double fps = 24.0;
  constexpr double tolerance = 0.05;
  const bbsolver::TemporalEase out_ease{1.65, 58.0};
  const bbsolver::TemporalEase in_ease{0.35, 72.0};
  const double t0 = 0.0;
  const double t1 = static_cast<double>(n_frames - 1) / fps;

  std::vector<std::pair<double, std::vector<double>>> source_frames;
  std::vector<std::pair<double, std::vector<double>>> reduced_frames;
  double max_frame_outline_error = 0.0;

  for (int frame = 0; frame < n_frames; ++frame) {
    const double t_sec = static_cast<double>(frame) / fps;
    const double u =
        bbsolver::EvalTemporalBezier(t_sec, t0, 0.0, out_ease, t1, 1.0, in_ease);
    const std::vector<double> source =
        MakeRedundantSquareFlat(18.0 * u, -9.0 * u, 96.0 + 24.0 * u);

    double frame_outline_error = 0.0;
    std::vector<double> reduced =
        ReducedSquareFromSource(source, tolerance, &frame_outline_error);

    source_frames.push_back({t_sec, std::move(source)});
    reduced_frames.push_back({t_sec, std::move(reduced)});
    max_frame_outline_error = std::max(max_frame_outline_error, frame_outline_error);
  }

  Stage2TemporalFixture fixture;
  fixture.source = MakeShapeFlatSamples("unit/stage2/source", source_frames);
  fixture.reduced = MakeShapeFlatSamples("unit/stage2/reduced", reduced_frames);
  fixture.comp = Stage2Comp(n_frames, fps);
  fixture.frame_outline_error = max_frame_outline_error;
  return fixture;
}

static void TestFitterOutputDimensionContract() {
  // Cross-verification: FitShapeFlatFrame produces a fitted vector that
  // BuildSingleStableRegime correctly interprets via flat[1] (not .size()).
  // This guards against the main.cpp intermediate PropertySamples having the
  // wrong .dimensions field before BuildStableRegimes corrects it.
  const std::vector<double> source_flat = MakeRedundantRectFlat(true);
  const int source_vertex_count = static_cast<int>(std::llround(source_flat[1]));
  assert(source_vertex_count == 52);

  const bbsolver::PathFrameFitResult frame = bbsolver::FitShapeFlatFrame(source_flat, 0.01);
  assert(frame.ok);
  assert(frame.applied);
  // Fitter must have reduced the vertex count.
  assert(frame.fitted_vertex_count < source_vertex_count);
  // Fitter's fitted.size() must be exactly 2 + 6 * fitted_vertex_count.
  assert(static_cast<int>(frame.fitted.size()) == 2 + 6 * frame.fitted_vertex_count);
  // flat[1] in the fitted vector must match fitted_vertex_count.
  assert(static_cast<int>(std::llround(frame.fitted[1])) == frame.fitted_vertex_count);
  // Closed flag must be preserved.
  assert(static_cast<int>(std::llround(frame.fitted[0])) == 1);

  // Assemble a PropertySamples from three identical fitted frames, simulating
  // what main.cpp's FitReplacementPathProperty does. Intentionally leave
  // property.dimensions at the original high-dim value to test that
  // BuildSingleStableRegime corrects it from flat[1], not from .dimensions.
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = 2 + 6 * source_vertex_count;  // original: 314
  ps.property.id          = "unit/path_crossverify";
  for (int i = 0; i < 3; ++i) {
    bbsolver::Sample s;
    s.t_sec = static_cast<double>(i) / 24.0;
    s.v     = frame.fitted;
    ps.samples.push_back(std::move(s));
  }

  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps);

  assert(result.ok);
  // Regime dimensions must reflect the fitted vertex count, not 314.
  assert(result.regime.vertex_count == frame.fitted_vertex_count);
  assert(result.regime.dimensions == 2 + 6 * frame.fitted_vertex_count);
  assert(result.regime.dimensions < 2 + 6 * source_vertex_count);
  assert(result.regime.samples.property.dimensions == result.regime.dimensions);
}

static void TestFitterSeamPreservationContract() {
  // Cross-verification: FitShapeFlatFrame keeps vertex 0 as vertex 0 in the
  // fitted output (stable seam). Verified by reading flat[2..3] (first vertex
  // x,y) of source and fitted and confirming they match.
  const std::vector<double> source_flat = MakeRedundantRectFlat(true);
  const bbsolver::PathFrameFitResult frame = bbsolver::FitShapeFlatFrame(source_flat, 0.5);
  assert(frame.ok);

  const double source_v0_x = source_flat[2];
  const double source_v0_y = source_flat[3];
  const double fitted_v0_x = frame.fitted[2];
  const double fitted_v0_y = frame.fitted[3];

  // Vertex 0 of the fitted shape must be the same geometric point as vertex 0
  // of the source (the seam is preserved, not rotated).
  assert(std::abs(fitted_v0_x - source_v0_x) < 1e-9);
  assert(std::abs(fitted_v0_y - source_v0_y) < 1e-9);
}

static void TestFitterClosedFlagPreservationContract() {
  // FitShapeFlatFrame must not flip the closed flag.
  const std::vector<double> open_flat   = MakeRedundantRectFlat(false);
  const std::vector<double> closed_flat = MakeRedundantRectFlat(true);

  const bbsolver::PathFrameFitResult open_result   = bbsolver::FitShapeFlatFrame(open_flat,   0.5);
  const bbsolver::PathFrameFitResult closed_result = bbsolver::FitShapeFlatFrame(closed_flat, 0.5);

  assert(open_result.ok   && !open_result.closed);
  assert(closed_result.ok && closed_result.closed);
  assert(static_cast<int>(std::llround(open_result.fitted[0]))   == 0);
  assert(static_cast<int>(std::llround(closed_result.fitted[0])) == 1);
}

static void TestFitterVariableOutputFeedsToPipelineFallback() {
  // Simulate the case where per-frame fitting produces different vertex counts
  // (would happen if FitShapeFlatFrame is called with varying input topology,
  // which main.cpp currently blocks via !regimes.all_stable). Verify that
  // BuildSingleStableRegime correctly rejects the mixed stream.
  //
  // Build two shapes with different vertex counts and feed them as a sequence.
  std::vector<double> small_flat = MakeFlat(true, 4);
  std::vector<double> large_flat = MakeFlat(true, 8);

  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = 2 + 6 * 8;

  for (int frame = 0; frame < 3; ++frame) {
    bbsolver::Sample s;
    s.t_sec = static_cast<double>(frame) / 24.0;
    s.v     = (frame == 1) ? large_flat : small_flat;  // topology spike
    ps.samples.push_back(std::move(s));
  }

  const bbsolver::SingleRegimeResult result = bbsolver::BuildSingleStableRegime(ps);

  assert(!result.ok);
  // The fallback reason must be present.
  assert(!result.reason.empty());
}

// ---------------------------------------------------------------------------
// EstimateLinearKeyCount and AssessReplacementCandidate tests
// ---------------------------------------------------------------------------

// Build a PropertySamples where vertex positions move linearly over N frames:
// vertex i at frame t = base_i + t * step. This is perfectly linear in time,
// so a greedy scan should produce 2 keys (start and end).
static bbsolver::PropertySamples MakeLinearMotionSamples(int n_frames,
                                                          int vertex_count,
                                                          bool closed) {
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = 2 + 6 * vertex_count;

  for (int t = 0; t < n_frames; ++t) {
    bbsolver::Sample s;
    s.t_sec = static_cast<double>(t) / 24.0;
    s.v.push_back(closed ? 1.0 : 0.0);
    s.v.push_back(static_cast<double>(vertex_count));
    for (int i = 0; i < vertex_count; ++i) {
      // x = base_x + t * 0.3, y = base_y + t * 0.2 (smooth linear motion)
      s.v.push_back(static_cast<double>(i) * 10.0 + t * 0.3);
      s.v.push_back(static_cast<double>(i) * 5.0  + t * 0.2);
      s.v.push_back(0.0);  // zero tangents
      s.v.push_back(0.0);
      s.v.push_back(0.0);
      s.v.push_back(0.0);
    }
    ps.samples.push_back(std::move(s));
  }
  return ps;
}

// Build a PropertySamples where vertex positions jump by a large per-frame
// random-looking offset: each frame's vertex 0 moves by step_size (uniform
// forward motion) but vertex 1 alternates direction. This mimics temporally
// incoherent per-frame-fitted shapes.
static bbsolver::PropertySamples MakeIncoherentMotionSamples(int n_frames,
                                                               int vertex_count,
                                                               bool closed,
                                                               double incoherence_mag) {
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = 2 + 6 * vertex_count;

  for (int t = 0; t < n_frames; ++t) {
    bbsolver::Sample s;
    s.t_sec = static_cast<double>(t) / 24.0;
    s.v.push_back(closed ? 1.0 : 0.0);
    s.v.push_back(static_cast<double>(vertex_count));
    for (int i = 0; i < vertex_count; ++i) {
      // Alternating sign creates high second-order difference (acceleration).
      const double sign = ((t + i) % 2 == 0) ? 1.0 : -1.0;
      s.v.push_back(static_cast<double>(i) * 10.0 + sign * incoherence_mag);
      s.v.push_back(static_cast<double>(i) * 5.0  + sign * incoherence_mag);
      s.v.push_back(0.0);
      s.v.push_back(0.0);
      s.v.push_back(0.0);
      s.v.push_back(0.0);
    }
    ps.samples.push_back(std::move(s));
  }
  return ps;
}

static bbsolver::PropertySamples MakeTimedTwoVertexSamples(
    const std::vector<double>& times,
    const std::vector<double>& values) {
  assert(times.size() == values.size());
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = 2 + 6 * 2;

  for (std::size_t i = 0; i < times.size(); ++i) {
    bbsolver::Sample s;
    s.t_sec = times[i];
    s.v = {
        0.0, 2.0,
        values[i], 0.0, 0.0, 0.0, 0.0, 0.0,
        values[i] + 10.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    };
    ps.samples.push_back(std::move(s));
  }
  return ps;
}

static void TestEstimateLinearKeyCountPerfectlyLinear() {
  // Linear motion over 10 frames -> 2 keys (start + end).
  const bbsolver::PropertySamples ps = MakeLinearMotionSamples(10, 8, true);
  const int keys = bbsolver::EstimateLinearKeyCount(ps, 0.5);
  assert(keys == 2);
}

static void TestEstimateLinearKeyCountSingleSample() {
  const bbsolver::PropertySamples ps = MakeSamples(true, {6});
  assert(bbsolver::EstimateLinearKeyCount(ps, 0.5) == 1);
}

static void TestEstimateLinearKeyCountUsesSampleTimes() {
  // Values are linear in real time, but not in sample index. Index-based
  // interpolation would need 3 keys here; time-based interpolation needs 2.
  const bbsolver::PropertySamples ps =
      MakeTimedTwoVertexSamples({0.0, 0.5, 2.0}, {0.0, 1.0, 4.0});
  assert(bbsolver::EstimateLinearKeyCount(ps, 0.01) == 2);
}

static void TestEstimateLinearKeyCountDimensionMismatchConservative() {
  const bbsolver::PropertySamples ps = MakeSamples(true, {4, 5, 4});
  assert(bbsolver::EstimateLinearKeyCount(ps, 0.5) == 3);
}

static void TestEstimateLinearKeyCountIncoherent() {
  // Alternating-direction motion with magnitude >> tolerance -> one key per
  // sample (the greedy scan can't merge any consecutive pair).
  const bbsolver::PropertySamples ps =
      MakeIncoherentMotionSamples(10, 4, true, 5.0);  // 5 px >> 0.5 tolerance
  const int keys = bbsolver::EstimateLinearKeyCount(ps, 0.5);
  // Every sample must be a key since consecutive pairs differ by ~10 px.
  assert(keys == 10);
}

static void TestEstimateLinearKeyCountWithinTolerance() {
  // Sub-tolerance motion -> almost all samples can be merged.
  const bbsolver::PropertySamples ps =
      MakeIncoherentMotionSamples(8, 4, true, 0.1);  // 0.1 px << 0.5 tolerance
  const int keys = bbsolver::EstimateLinearKeyCount(ps, 0.5);
  // Motion is small enough that many frames can be merged.
  assert(keys <= 4);
}

static void TestEstimateLinearKeyCountMonotonic() {
  // Adding more incoherence should never DECREASE the key count.
  const bbsolver::PropertySamples ps_tight =
      MakeIncoherentMotionSamples(12, 4, true, 0.1);
  const bbsolver::PropertySamples ps_loose =
      MakeIncoherentMotionSamples(12, 4, true, 5.0);
  const int keys_tight = bbsolver::EstimateLinearKeyCount(ps_tight, 0.5);
  const int keys_loose = bbsolver::EstimateLinearKeyCount(ps_loose, 0.5);
  assert(keys_tight <= keys_loose);
}

static void TestAssessReplacementCandidateCoherentBeatsOriginal() {
  // A temporally coherent candidate (linear motion) should have estimated keys
  // well below the original incoherent stream -> worth_attempting.
  const int n_frames    = 20;
  const int orig_verts  = 10;
  const int cand_verts  = 6;

  // Original: large alternating motion (high incoherence, many keys needed).
  const bbsolver::PropertySamples original =
      MakeIncoherentMotionSamples(n_frames, orig_verts, true, 4.0);

  // Candidate regime: perfectly linear motion -> 2 keys.
  const bbsolver::PropertySamples cand_ps =
      MakeLinearMotionSamples(n_frames, cand_verts, true);
  const bbsolver::PathCorrespondenceResult regimes =
      bbsolver::BuildStableRegimes(cand_ps);
  assert(regimes.ok && regimes.all_stable);

  const bbsolver::ReplacementCandidateAssessment assessment =
      bbsolver::AssessReplacementCandidate(regimes.regimes.front(), original, 0.5);

  assert(assessment.worth_attempting);
  assert(assessment.estimated_candidate_keys < assessment.estimated_original_keys);
  assert(assessment.key_reduction_ratio < 1.0);
  // Decompose cost = n_frames * vertex_count
  assert(assessment.decompose_cost == n_frames * cand_verts);
  assert(!assessment.reason.empty());
}

static void TestAssessReplacementCandidateIncoherentFails() {
  // An incoherent candidate (same incoherence as original) -> not worth attempting.
  const int n_frames   = 20;
  const int orig_verts = 8;
  const int cand_verts = 8;
  const double mag     = 4.0;  // well above 0.5 tolerance

  const bbsolver::PropertySamples original =
      MakeIncoherentMotionSamples(n_frames, orig_verts, true, mag);
  const bbsolver::PropertySamples cand_ps =
      MakeIncoherentMotionSamples(n_frames, cand_verts, true, mag);

  const bbsolver::PathCorrespondenceResult regimes =
      bbsolver::BuildStableRegimes(cand_ps);
  assert(regimes.ok && regimes.all_stable);

  const bbsolver::ReplacementCandidateAssessment assessment =
      bbsolver::AssessReplacementCandidate(regimes.regimes.front(), original, 0.5);

  // Both streams need ~n_frames keys -> ratio ~= 1.0 -> not worth attempting.
  assert(!assessment.worth_attempting);
  assert(assessment.estimated_candidate_keys >= assessment.estimated_original_keys);
  assert(assessment.key_reduction_ratio >= 1.0);
  assert(assessment.reason.find("insufficient") != std::string::npos);
}

static void TestAssessReplacementCandidateEqualKeysFewerVerticesPasses() {
  // If the precheck estimates equal key counts but the candidate has fewer
  // vertices, let the full solve reach EvaluateReplacementAcceptance. This is
  // the bridge to equal-key/fewer-vertex wins.
  const int n_frames   = 20;
  const int orig_verts = 8;
  const int cand_verts = 6;
  const double mag     = 4.0;

  const bbsolver::PropertySamples original =
      MakeIncoherentMotionSamples(n_frames, orig_verts, true, mag);
  const bbsolver::PropertySamples cand_ps =
      MakeIncoherentMotionSamples(n_frames, cand_verts, true, mag);

  const bbsolver::PathCorrespondenceResult regimes =
      bbsolver::BuildStableRegimes(cand_ps);
  assert(regimes.ok && regimes.all_stable);

  const bbsolver::ReplacementCandidateAssessment assessment =
      bbsolver::AssessReplacementCandidate(regimes.regimes.front(), original, 0.5);

  assert(assessment.worth_attempting);
  assert(assessment.estimated_candidate_keys == assessment.estimated_original_keys);
  assert(assessment.reason.find("equal_key_vertex_reduction_precheck") != std::string::npos);
}

static void TestAssessReplacementCandidateNoodleLikeScenario() {
  // Simulate the noodle failure case: 73 frames, 52-vertex original has some
  // linear-interpolatable segments, but a per-frame-incoherent 43-vertex
  // candidate needs one key per frame -> falls back correctly.
  //
  // Original: 52 vertices moving coherently.
  // Candidate: 43 vertices with high incoherence (one key per frame).
  //
  // The assessment must report !worth_attempting.
  const int n_frames   = 73;
  const int orig_verts = 52;
  const int cand_verts = 43;

  // Original has relatively smooth motion -> some segments merge.
  const bbsolver::PropertySamples original =
      MakeLinearMotionSamples(n_frames, orig_verts, true);
  // Candidate has high alternating noise -> every sample is a key.
  const bbsolver::PropertySamples cand_ps =
      MakeIncoherentMotionSamples(n_frames, cand_verts, true, 3.0);

  const bbsolver::PathCorrespondenceResult regimes =
      bbsolver::BuildStableRegimes(cand_ps);
  assert(regimes.ok && regimes.all_stable);
  assert(regimes.regimes.front().vertex_count == cand_verts);

  const bbsolver::ReplacementCandidateAssessment assessment =
      bbsolver::AssessReplacementCandidate(regimes.regimes.front(), original, 0.5);

  // Candidate needs 73 keys (one per frame); original needs 2 (linear).
  // The assessment correctly blocks the expensive solve.
  assert(!assessment.worth_attempting);
  assert(assessment.estimated_candidate_keys > assessment.estimated_original_keys);
  assert(assessment.decompose_cost == n_frames * cand_verts);

  // Notes must mention the cost so main.cpp can log it.
  assert(assessment.reason.find("estimated_candidate_keys") != std::string::npos);
  assert(assessment.reason.find("decompose_cost") != std::string::npos);
}

static void TestAssessReplacementCandidateStage1VertexReductionRejectsKeyIncrease() {
  // Vertex reduction alone is not enough: if the cheap key estimate predicts
  // more keys than the original temporal baseline, reject before the expensive
  // replacement solve. The final acceptance gate also rejects key increases.
  const int n_frames   = 73;
  const int orig_verts = 52;
  const int cand_verts = 25;

  const bbsolver::PropertySamples original =
      MakeLinearMotionSamples(n_frames, orig_verts, true);
  const bbsolver::PropertySamples cand_ps =
      MakeIncoherentMotionSamples(n_frames, cand_verts, true, 3.0);

  const bbsolver::PathCorrespondenceResult regimes =
      bbsolver::BuildStableRegimes(cand_ps);
  assert(regimes.ok && regimes.all_stable);
  assert(regimes.regimes.front().vertex_count == cand_verts);

  const bbsolver::ReplacementCandidateAssessment assessment =
      bbsolver::AssessReplacementCandidate(regimes.regimes.front(), original, 0.5);

  assert(!assessment.worth_attempting);
  assert(assessment.estimated_candidate_keys > assessment.estimated_original_keys);
  assert(assessment.reason.find("temporal coherence insufficient") != std::string::npos);
}

static void TestAssessReplacementCandidateReportsReason() {
  // Verify that both passing and failing assessments populate reason.
  const int n = 10;
  const bbsolver::PropertySamples original = MakeLinearMotionSamples(n, 8, true);
  const bbsolver::PropertySamples cand_ps  = MakeLinearMotionSamples(n, 4, true);

  const bbsolver::PathCorrespondenceResult regimes =
      bbsolver::BuildStableRegimes(cand_ps);
  assert(regimes.ok && regimes.all_stable);

  const bbsolver::ReplacementCandidateAssessment a =
      bbsolver::AssessReplacementCandidate(regimes.regimes.front(), original, 0.5);

  assert(!a.reason.empty());
  // Both streams are linear -> keys approximately equal. This is not an
  // obvious one-key-per-sample failure, so the full final solve can decide.
  assert(a.estimated_candidate_keys == a.estimated_original_keys);
  assert(a.worth_attempting);
}

// ---------------------------------------------------------------------------
// EvaluateReplacementAcceptance tests
// ---------------------------------------------------------------------------

// Convenience wrapper that fills in default valid-original parameters.
static bbsolver::ReplacementAcceptanceVerdict Evaluate(
    int  candidate_keys,
    int  candidate_fitted_vertices,
    int  original_keys,
    int  original_source_vertices,
    bool original_converged = true,
    double original_max_err = 0.0,
    double candidate_max_err = 0.0,
    bool candidate_converged = true,
    double tolerance = 0.5,
    bool prefer_vertices = false,
    double max_key_growth_ratio = 4.0,
    double min_vertex_reduction_ratio = 0.20,
    int original_sample_count = 0) {
  return bbsolver::EvaluateReplacementAcceptance(
      candidate_keys,
      candidate_max_err,
      candidate_converged,
      candidate_fitted_vertices,
      original_keys,
      original_max_err,
      original_source_vertices,
      original_converged,
      tolerance,
      prefer_vertices,
      max_key_growth_ratio,
      min_vertex_reduction_ratio,
      original_sample_count);
}

static void TestAcceptanceStrictlyFewerKeys() {
  // Candidate has fewer keys -> always accept.
  const auto v = Evaluate(30, 43, 55, 52);
  assert(v.use_candidate);
  assert(v.decision_note.find("candidate_keys=30") != std::string::npos);
  assert(v.decision_note.find("original_keys=55") != std::string::npos);
}

static void TestAcceptanceStrictlyMoreKeys() {
  // Candidate has more keys -> fallback. Note preserves existing format.
  const auto v = Evaluate(73, 43, 55, 52);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_candidate_keys=73") != std::string::npos);
  assert(v.decision_note.find("original_fallback_keys=55") != std::string::npos);
}

static void TestAcceptanceStage1MoreKeysMaterialReduction() {
  // Vertex reduction is not enough by itself for a user-facing win: if it
  // increases keys versus the temporal baseline, fallback to the original solve.
  const auto v = Evaluate(72, 25, 55, 52);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_candidate_keys=72") != std::string::npos);
  assert(v.decision_note.find("original_fallback_keys=55") != std::string::npos);
}

static void TestAcceptanceStage1RejectsUnboundedKeyPenalty() {
  // Stage-1 is not permission to accept unlimited temporal blow-up.
  const auto v = Evaluate(100, 25, 55, 52);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_candidate_keys=100") != std::string::npos);
}

static void TestAcceptanceVertexPreferredDefaultOffRejectsMoreKeys() {
  // The production default remains key-count conservative.
  const auto v = Evaluate(96, 34, 52, 45, true, 0.0, 0.0, true, 0.5,
                          /*prefer_vertices=*/false,
                          /*max_key_growth_ratio=*/2.0,
                          /*min_vertex_reduction_ratio=*/0.20,
                          /*original_sample_count=*/135);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_candidate_keys=96") != std::string::npos);
  assert(v.decision_note.find("original_fallback_keys=52") != std::string::npos);
}

static void TestAcceptanceVertexPreferredRejectsGuardedKeyGrowth() {
  // Vertex-priority mode must not accept more keys than the temporal baseline.
  // The main solve should fall back and let the post-temporal vertex pass try
  // to prune without trading away keyframe reduction.
  const auto v = Evaluate(96, 34, 52, 45, true, 0.0, 0.0, true, 0.5,
                          /*prefer_vertices=*/true,
                          /*max_key_growth_ratio=*/4.0,
                          /*min_vertex_reduction_ratio=*/0.20,
                          /*original_sample_count=*/135);
  assert(!v.use_candidate);
  assert(v.decision_note.find("vertex_preference_rejected_key_growth") !=
         std::string::npos);
  assert(v.decision_note.find("post_temporal_vertex_prune_allowed") !=
         std::string::npos);
  assert(v.decision_note.find("candidate_keys=96") != std::string::npos);
  assert(v.decision_note.find("candidate_fitted_vertices=34") != std::string::npos);
}

static void TestAcceptanceVertexPreferredRejectsLooseToleranceNoodleTradeoff() {
  // This is the observed build049 regression class: a loose tolerance can make
  // a low-vertex replacement look attractive, but 77 replacement keys must not
  // preempt the 31-key temporal result. The second pass handles vertex pruning.
  const auto v = Evaluate(77, 17, 31, 52, true, 0.0, 2.624113, true, 3.0,
                          /*prefer_vertices=*/true,
                          /*max_key_growth_ratio=*/4.0,
                          /*min_vertex_reduction_ratio=*/0.20,
                          /*original_sample_count=*/135);
  assert(!v.use_candidate);
  assert(v.decision_note.find("vertex_preference_rejected_key_growth") !=
         std::string::npos);
  assert(v.decision_note.find("post_temporal_vertex_prune_allowed") !=
         std::string::npos);
  assert(v.decision_note.find("candidate_keys=77") != std::string::npos);
  assert(v.decision_note.find("candidate_fitted_vertices=17") != std::string::npos);
}

static void TestAcceptanceVertexPreferredRejectsExcessiveKeyGrowth() {
  const auto v = Evaluate(220, 34, 52, 45, true, 0.0, 0.0, true, 0.5,
                          /*prefer_vertices=*/true,
                          /*max_key_growth_ratio=*/4.0,
                          /*min_vertex_reduction_ratio=*/0.20,
                          /*original_sample_count=*/240);
  assert(!v.use_candidate);
  assert(v.decision_note.find("vertex_preference_rejected") != std::string::npos);
}

static void TestAcceptanceVertexPreferredRejectsRawFrameRegression() {
  const auto v = Evaluate(136, 34, 52, 45, true, 0.0, 0.0, true, 0.5,
                          /*prefer_vertices=*/true,
                          /*max_key_growth_ratio=*/3.0,
                          /*min_vertex_reduction_ratio=*/0.20,
                          /*original_sample_count=*/135);
  assert(!v.use_candidate);
  assert(v.decision_note.find("vertex_preference_rejected") != std::string::npos);
}

static void TestAcceptanceVertexPreferredRejectsWeakVertexReduction() {
  const auto v = Evaluate(96, 40, 52, 45, true, 0.0, 0.0, true, 0.5,
                          /*prefer_vertices=*/true,
                          /*max_key_growth_ratio=*/4.0,
                          /*min_vertex_reduction_ratio=*/0.20,
                          /*original_sample_count=*/135);
  assert(!v.use_candidate);
  assert(v.decision_note.find("vertex_preference_rejected") != std::string::npos);
}

static bbsolver::ReplacementFastVertexPreferenceInput FastVertexInput() {
  bbsolver::ReplacementFastVertexPreferenceInput input;
  input.prefer_vertices = true;
  input.source_sample_count = 20;
  input.candidate_key_count = 12;
  input.estimated_original_keys = 12;
  input.validation_samples_checked = 20;
  input.candidate_converged = true;
  input.candidate_max_err = 0.25;
  input.path_tolerance = 0.5;
  input.fitted_vertices = 10;
  input.source_vertices_for_ratio = 20;
  input.min_vertex_reduction_ratio = 0.20;
  return input;
}

static void TestFastVertexPreferenceAcceptsGuardedReduction() {
  const auto verdict =
      bbsolver::EvaluateReplacementFastVertexPreference(FastVertexInput());
  assert(verdict.accept);
  assert(verdict.candidate_reduces_keys);
  assert(verdict.estimated_key_gate);
  assert(std::abs(verdict.vertex_reduction_ratio - 0.5) < 1e-12);
}

static void TestFastVertexPreferenceRequiresKeyReduction() {
  auto input = FastVertexInput();
  input.candidate_key_count = 19;
  input.estimated_original_keys = 20;
  const auto verdict =
      bbsolver::EvaluateReplacementFastVertexPreference(input);
  assert(!verdict.accept);
  assert(!verdict.candidate_reduces_keys);
  assert(verdict.estimated_key_gate);
}

static void TestFastVertexPreferenceRequiresValidationSamples() {
  auto input = FastVertexInput();
  input.validation_samples_checked = 0;
  const auto verdict =
      bbsolver::EvaluateReplacementFastVertexPreference(input);
  assert(!verdict.accept);
  assert(verdict.candidate_reduces_keys);
  assert(verdict.estimated_key_gate);
}

static void TestBuildFastVertexPreferenceInputCopiesSummaryAndGates() {
  bbsolver::ReplacementValidationSummary summary;
  summary.candidate_converged = true;
  summary.candidate_max_err = 0.25;
  const auto input = bbsolver::BuildReplacementFastVertexPreferenceInput(
      true,
      20,
      12,
      13,
      20,
      summary,
      0.5,
      10,
      20,
      0.2);
  assert(input.prefer_vertices);
  assert(input.source_sample_count == 20);
  assert(input.candidate_key_count == 12);
  assert(input.estimated_original_keys == 13);
  assert(input.validation_samples_checked == 20);
  assert(input.candidate_converged);
  assert(input.candidate_max_err == 0.25);
  assert(input.path_tolerance == 0.5);
  assert(input.fitted_vertices == 10);
  assert(input.source_vertices_for_ratio == 20);
  assert(input.min_vertex_reduction_ratio == 0.2);
}

static void TestBuildFastVertexPreferenceNoteInputCopiesFields() {
  bbsolver::PathTemporalValidationResult validation;
  validation.notes = "source_outline_validation=ok";
  bbsolver::SharpCornerValidationResult sharp;
  sharp.enabled = true;
  sharp.notes = "sharp_corner_validation=ok";
  bbsolver::ReplacementFastVertexPreferenceVerdict preference;
  preference.vertex_reduction_ratio = 0.5;

  const auto input = bbsolver::BuildReplacementFastVertexPreferenceNoteInput(
      0.25,
      validation,
      true,
      sharp,
      12,
      20,
      13,
      14,
      10,
      20,
      preference);
  assert(input.candidate_max_err == 0.25);
  assert(input.source_validation_notes == "source_outline_validation=ok");
  assert(input.visible_outline_reference);
  assert(input.sharp_validation_enabled);
  assert(input.sharp_validation_notes == "sharp_corner_validation=ok");
  assert(input.candidate_key_count == 12);
  assert(input.source_sample_count == 20);
  assert(input.estimated_candidate_keys == 13);
  assert(input.estimated_original_keys == 14);
  assert(input.fitted_vertices == 10);
  assert(input.source_vertices_for_ratio == 20);
  assert(input.vertex_reduction_ratio == 0.5);
}

static void TestValidationSummaryUsesSourceValidationWhenAvailable() {
  bbsolver::PathTemporalValidationResult validation;
  validation.samples_checked = 3;
  validation.ok = true;
  validation.max_outline_error = 0.25;
  bbsolver::SharpCornerValidationResult sharp;
  sharp.ok = true;
  bbsolver::PropertyKeys candidate_keys;
  candidate_keys.converged = false;
  candidate_keys.max_err = 9.0;

  const auto summary = bbsolver::SummarizeReplacementCandidateValidation(
      validation, sharp, candidate_keys);
  assert(summary.candidate_converged);
  assert(summary.candidate_max_err == 0.25);
}

static void TestValidationSummaryFallsBackToSolverResultWithoutSamples() {
  bbsolver::PathTemporalValidationResult validation;
  validation.samples_checked = 0;
  validation.ok = false;
  validation.max_outline_error = 0.25;
  bbsolver::SharpCornerValidationResult sharp;
  sharp.ok = true;
  bbsolver::PropertyKeys candidate_keys;
  candidate_keys.converged = true;
  candidate_keys.max_err = 0.75;

  const auto summary = bbsolver::SummarizeReplacementCandidateValidation(
      validation, sharp, candidate_keys);
  assert(summary.candidate_converged);
  assert(summary.candidate_max_err == 0.75);

  sharp.ok = false;
  const auto sharp_rejected =
      bbsolver::SummarizeReplacementCandidateValidation(
          validation, sharp, candidate_keys);
  assert(!sharp_rejected.candidate_converged);
  assert(sharp_rejected.candidate_max_err == 0.75);
}

static void TestApplyValidationSummaryToKeysOnlyWhenSamplesChecked() {
  bbsolver::PathTemporalValidationResult validation;
  validation.samples_checked = 3;
  bbsolver::ReplacementValidationSummary summary;
  summary.candidate_converged = true;
  summary.candidate_max_err = 0.125;
  bbsolver::PropertyKeys keys;
  keys.converged = false;
  keys.max_err = 2.0;

  assert(bbsolver::ApplyReplacementValidationSummaryToKeys(
      validation, summary, &keys));
  assert(keys.converged);
  assert(keys.max_err == 0.125);

  validation.samples_checked = 0;
  summary.candidate_converged = false;
  summary.candidate_max_err = 4.0;
  assert(!bbsolver::ApplyReplacementValidationSummaryToKeys(
      validation, summary, &keys));
  assert(keys.converged);
  assert(keys.max_err == 0.125);
  assert(!bbsolver::ApplyReplacementValidationSummaryToKeys(
      validation, summary, nullptr));
}

static bbsolver::ReplacementRetryEligibilityInput RetryEligibilityInput() {
  bbsolver::ReplacementRetryEligibilityInput input;
  input.verdict_use_candidate = false;
  input.candidate_key_count = 8;
  input.original_key_count = 12;
  input.validation_samples_checked = 4;
  input.source_validation_ok = false;
  input.sharp_validation_ok = true;
  input.fitted_vertices = 8;
  input.source_min_vertices = 12;
  return input;
}

static void TestRetryEligibilityAcceptsNearMissWithKeyGate() {
  const auto eligibility =
      bbsolver::EvaluateReplacementRetryEligibility(RetryEligibilityInput());
  assert(eligibility.retry_key_gate);
  assert(!eligibility.failed_only_sharp_gate);
  assert(eligibility.should_retry);
}

static void TestRetryEligibilityBlocksSharpOnlyFailure() {
  auto input = RetryEligibilityInput();
  input.source_validation_ok = true;
  input.sharp_validation_ok = false;
  const auto eligibility =
      bbsolver::EvaluateReplacementRetryEligibility(input);
  assert(eligibility.retry_key_gate);
  assert(eligibility.failed_only_sharp_gate);
  assert(!eligibility.should_retry);
}

static void TestRetryEligibilityBlocksKeyGrowth() {
  auto input = RetryEligibilityInput();
  input.candidate_key_count = 13;
  const auto eligibility =
      bbsolver::EvaluateReplacementRetryEligibility(input);
  assert(!eligibility.retry_key_gate);
  assert(!eligibility.should_retry);
}

static void TestBuildRetryEligibilityInputCopiesValidationFields() {
  bbsolver::PathTemporalValidationResult validation;
  validation.samples_checked = 4;
  validation.ok = true;
  bbsolver::SharpCornerValidationResult sharp;
  sharp.ok = false;

  const auto input = bbsolver::BuildReplacementRetryEligibilityInput(
      false, 8, 12, validation, sharp, 8, 12);

  assert(!input.verdict_use_candidate);
  assert(input.candidate_key_count == 8);
  assert(input.original_key_count == 12);
  assert(input.validation_samples_checked == 4);
  assert(input.source_validation_ok);
  assert(!input.sharp_validation_ok);
  assert(input.fitted_vertices == 8);
  assert(input.source_min_vertices == 12);
}

static void TestRetryEligibilityRequiresValidationSamplesAndHeadroom() {
  auto input = RetryEligibilityInput();
  input.validation_samples_checked = 0;
  assert(!bbsolver::EvaluateReplacementRetryEligibility(input).should_retry);

  input = RetryEligibilityInput();
  input.fitted_vertices = 11;
  assert(!bbsolver::EvaluateReplacementRetryEligibility(input).should_retry);

  input = RetryEligibilityInput();
  input.verdict_use_candidate = true;
  assert(!bbsolver::EvaluateReplacementRetryEligibility(input).should_retry);
}

static void TestAcceptanceEqualKeysFewerVertices() {
  // Equal key count, but candidate has fewer fitted vertices -> accept.
  const auto v = Evaluate(55, 43, 55, 52);
  assert(v.use_candidate);
  assert(v.decision_note.find("path_replacement_accepted_equal_keys") != std::string::npos);
  assert(v.decision_note.find("candidate_fitted_vertices=43") != std::string::npos);
  assert(v.decision_note.find("original_source_vertices=52") != std::string::npos);
}

static void TestAcceptanceEqualKeysSameVertices() {
  // Equal keys AND equal vertex count -> fallback.
  const auto v = Evaluate(55, 52, 55, 52);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_fallback_equal_keys") != std::string::npos);
}

static void TestAcceptanceEqualKeysMoreVertices() {
  // Equal keys but candidate somehow has MORE vertices -> fallback.
  const auto v = Evaluate(55, 60, 55, 52);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_fallback_equal_keys") != std::string::npos);
}

static void TestAcceptanceOriginalNotConverged() {
  // Original did not converge, but replacement still cannot increase key count.
  const auto v = Evaluate(73, 43, 55, 52, /*original_converged=*/false);
  assert(!v.use_candidate);
  assert(v.decision_note.find("original_not_valid_key_gate") != std::string::npos);

  const auto fewer = Evaluate(30, 43, 55, 52, /*original_converged=*/false);
  assert(fewer.use_candidate);
  assert(fewer.decision_note.find("original_not_valid") != std::string::npos);
}

static void TestAcceptanceOriginalExceedsTolerance() {
  // Original solved but exceeds tolerance; still reject if replacement increases keys.
  const auto v = Evaluate(73, 43, 55, 52,
                           /*original_converged=*/true,
                           /*original_max_err=*/0.6,
                           /*candidate_max_err=*/0.0,
                           /*candidate_converged=*/true,
                           /*tolerance=*/0.5);
  assert(!v.use_candidate);
  assert(v.decision_note.find("original_not_valid_key_gate") != std::string::npos);

  const auto fewer = Evaluate(30, 43, 55, 52,
                              /*original_converged=*/true,
                              /*original_max_err=*/0.6,
                              /*candidate_max_err=*/0.0,
                              /*candidate_converged=*/true,
                              /*tolerance=*/0.5);
  assert(fewer.use_candidate);
  assert(fewer.decision_note.find("original_not_valid") != std::string::npos);
}

static void TestAcceptanceOriginalAtToleranceBoundary() {
  // Original max_err exactly at tolerance + eps -> still valid fallback.
  // Uses the 1e-9 slack from the implementation.
  const double tol = 0.5;
  const auto v_at = Evaluate(73, 43, 55, 52,
                              true, tol, 0.0, true, tol);   // original_max_err == tol
  assert(!v_at.use_candidate);   // valid fallback, candidate has more keys

  const auto v_over = Evaluate(73, 43, 55, 52,
                                true, tol + 0.1, 0.0, true, tol); // original_max_err > tol
  assert(v_over.use_candidate);  // original invalid
}

static void TestAcceptanceCandidateInvalidFallsBack() {
  const auto v_err = Evaluate(30, 43, 55, 52,
                               /*original_converged=*/true,
                               /*original_max_err=*/0.0,
                               /*candidate_max_err=*/0.6,
                               /*candidate_converged=*/true,
                               /*tolerance=*/0.5);
  assert(!v_err.use_candidate);
  assert(v_err.decision_note.find("candidate_invalid") != std::string::npos);

  const auto v_unconverged = Evaluate(30, 43, 55, 52,
                                       /*original_converged=*/true,
                                       /*original_max_err=*/0.0,
                                       /*candidate_max_err=*/0.0,
                                       /*candidate_converged=*/false,
                                       /*tolerance=*/0.5);
  assert(!v_unconverged.use_candidate);
  assert(v_unconverged.decision_note.find("candidate_invalid") != std::string::npos);
}

static void TestAcceptanceBothInvalidChoosesLessBadResult() {
  const auto v_candidate = Evaluate(30, 43, 55, 52,
                                     /*original_converged=*/false,
                                     /*original_max_err=*/5.0,
                                     /*candidate_max_err=*/0.6,
                                     /*candidate_converged=*/true,
                                     /*tolerance=*/0.5);
  assert(v_candidate.use_candidate);
  assert(v_candidate.decision_note.find("both_invalid") != std::string::npos);
  assert(v_candidate.decision_note.find("original_not_valid") == std::string::npos);

  const auto v_original = Evaluate(30, 43, 55, 52,
                                    /*original_converged=*/true,
                                    /*original_max_err=*/0.7,
                                    /*candidate_max_err=*/1.2,
                                    /*candidate_converged=*/true,
                                    /*tolerance=*/0.5);
  assert(!v_original.use_candidate);
  assert(v_original.decision_note.find("fallback_both_invalid") != std::string::npos);
}

static void TestAcceptanceNoodleScenario() {
  // The actual noodle case: 73 candidate keys, 55 original keys.
  // Original is valid; candidate has more keys -> fallback.
  const auto v = Evaluate(73, 43, 55, 52);
  assert(!v.use_candidate);
  // Note must expose key counts for diagnostics.
  assert(v.decision_note.find("73") != std::string::npos);
  assert(v.decision_note.find("55") != std::string::npos);
}

static void TestAcceptanceEqualKeysNoodleImproved() {
  // Hypothetical: after temporal coherence fix, noodle candidate matches original
  // key count but uses fewer vertices -> should now be accepted.
  const auto v = Evaluate(55, 43, 55, 52);
  assert(v.use_candidate);
  assert(v.decision_note.find("path_replacement_accepted_equal_keys") != std::string::npos);
}

static void TestAcceptanceEqualKeysMateriallyReducedAtTolerance() {
  // Staged path replacement can be valuable even when temporal key count ties
  // the original: the replacement is still a smaller per-frame path if it
  // converged within tolerance and materially reduced the vertex count.
  const auto v = Evaluate(55,
                          25,
                          55,
                          52,
                          /*original_converged=*/true,
                          /*original_max_err=*/0.2,
                          /*candidate_max_err=*/0.5,
                          /*candidate_converged=*/true,
                          /*tolerance=*/0.5);
  assert(v.use_candidate);
  assert(v.decision_note.find("path_replacement_accepted_equal_keys") != std::string::npos);
  assert(v.decision_note.find("candidate_fitted_vertices=25") != std::string::npos);
  assert(v.decision_note.find("original_source_vertices=52") != std::string::npos);
}

static void TestAcceptanceEqualKeysMateriallyReducedOverToleranceRejected() {
  // Vertex reduction alone is not enough; the solved replacement must still be
  // within outline/error tolerance.
  const auto v = Evaluate(55,
                          25,
                          55,
                          52,
                          /*original_converged=*/true,
                          /*original_max_err=*/0.2,
                          /*candidate_max_err=*/0.500001,
                          /*candidate_converged=*/true,
                          /*tolerance=*/0.5);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_fallback_candidate_invalid") != std::string::npos);
}

static void TestAcceptanceEqualKeysNonReducedCandidateRejectedEvenWhenValid() {
  // A normal candidate with no vertex reduction must not replace a valid
  // original just because it ties on key count and error.
  const auto v = Evaluate(55,
                          52,
                          55,
                          52,
                          /*original_converged=*/true,
                          /*original_max_err=*/0.2,
                          /*candidate_max_err=*/0.0,
                          /*candidate_converged=*/true,
                          /*tolerance=*/0.5);
  assert(!v.use_candidate);
  assert(v.decision_note.find("path_replacement_fallback_equal_keys") != std::string::npos);
  assert(v.decision_note.find("candidate_fitted_vertices=52") != std::string::npos);
}

static void TestAcceptanceNoteAlwaysPopulated() {
  // Every branch must produce a non-empty decision_note.
  assert(!Evaluate(30, 43, 55, 52).decision_note.empty());                      // fewer keys
  assert(!Evaluate(73, 43, 55, 52).decision_note.empty());                      // more keys
  assert(!Evaluate(55, 43, 55, 52).decision_note.empty());                      // equal, accept
  assert(!Evaluate(55, 52, 55, 52).decision_note.empty());                      // equal, fallback
  assert(!Evaluate(73, 43, 55, 52, false).decision_note.empty());               // original invalid
  assert(!Evaluate(73, 43, 55, 52, true, 0.6, 0.0, true, 0.5).decision_note.empty());
  assert(!Evaluate(30, 43, 55, 52, true, 0.0, 0.6, true, 0.5).decision_note.empty());
}

// ---------------------------------------------------------------------------
// Fraction-coherence integration tests
// ---------------------------------------------------------------------------

// End-to-end pipeline contract: fitting multiple deformed frames at the SAME
// set of arc-length fractions guarantees equal vertex counts across all frames,
// which is the prerequisite for all_stable=true in BuildSingleStableRegime.
// This test uses FitShapeFlatFrameAtFractions directly to isolate the stable
// fraction layout contract from automatic fraction extraction.
static void TestFractionCoherenceProducesStableRegime() {
  const std::vector<double> base_flat = MakeRedundantRectFlat(true);
  // 8 equal-spacing fractions. They don't need to land on corners; the
  // constraint being tested is that the same fractions across all frames
  // forces identical vertex counts, producing a stable regime.
  const std::vector<double> fractions = {0.0, 0.125, 0.25, 0.375,
                                         0.5, 0.625, 0.75, 0.875};
  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 2.0;  // rectangle sides are exactly linear

  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = static_cast<int>(base_flat.size());

  for (int frame = 0; frame < 5; ++frame) {
    // Deform the rectangle by uniform translation per frame (coherent motion).
    std::vector<double> deformed = base_flat;
    const double shift = static_cast<double>(frame) * 2.0;
    for (std::size_t i = 2; i < deformed.size(); i += 6) {
      deformed[i]     += shift;
      deformed[i + 1] += shift * 0.5;
    }

    bbsolver::PathFrameFitResult fit =
        bbsolver::FitShapeFlatFrameAtFractions(deformed, fractions, opts);
    assert(fit.ok);
    assert(fit.applied);
    // Every frame must produce exactly fractions.size() vertices.
    assert(fit.fitted_vertex_count == static_cast<int>(fractions.size()));

    bbsolver::Sample s;
    s.t_sec = static_cast<double>(frame) / 24.0;
    s.v     = std::move(fit.fitted);
    ps.samples.push_back(std::move(s));
  }

  // Identical vertex counts across all frames -> single stable regime.
  const bbsolver::SingleRegimeResult regime = bbsolver::BuildSingleStableRegime(ps);
  assert(regime.ok);
  assert(regime.regime.vertex_count == static_cast<int>(fractions.size()));
  assert(regime.regime.samples.samples.size() == 5);
  assert(regime.regime.dimensions == 2 + 6 * static_cast<int>(fractions.size()));
}

// Round-trip seam: FitShapeFlatFrame's automatic outline_fractions can be fed
// into FitShapeFlatFrameAtFractions on a deformed second frame and must produce
// the same fitted_vertex_count.
static void TestAutomaticFitFractionsRoundTrip() {
  const std::vector<double> base_flat = MakeRedundantRectFlat(true);
  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance   = 2.0;
  opts.target_vertex_count = 4;

  bbsolver::PathFrameFitResult ref_fit = bbsolver::FitShapeFlatFrame(base_flat, opts);
  assert(ref_fit.ok);
  assert(ref_fit.applied);

  assert(!ref_fit.outline_fractions.empty());
  assert(static_cast<int>(ref_fit.outline_fractions.size()) ==
         ref_fit.fitted_vertex_count);

  // Slightly deformed second frame (translate x by 5 px).
  std::vector<double> deformed = base_flat;
  for (std::size_t i = 2; i < deformed.size(); i += 6) {
    deformed[i] += 5.0;
  }

  bbsolver::PathFrameFitResult frac_fit = bbsolver::FitShapeFlatFrameAtFractions(
      deformed, ref_fit.outline_fractions, opts);
  assert(frac_fit.ok);
  assert(frac_fit.applied);
  // Fraction-based fit must match the reference frame's vertex count.
  assert(frac_fit.fitted_vertex_count == ref_fit.fitted_vertex_count);
  // Fractions must be echoed back.
  assert(frac_fit.outline_fractions.size() == ref_fit.outline_fractions.size());
}

// Pipeline-level simulation of the multi-seed selection in
// FitReplacementPathProperty: try seeds in order; when seed A fails (!applied
// or exceeds tolerance), move to seed B; the first passing seed with the lowest
// max outline error wins.
//
// Seed A: fractions.size() == source_vertex_count -> applied=false on every
// frame (deterministic failure). Seed B: 8 valid fractions -> applied=true,
// within tolerance. The winning seed's output forms a single stable regime.
static void TestMultiSeedSelectsLaterSeedWhenFirstFails() {
  const std::vector<double> base_flat = MakeRedundantRectFlat(true);
  const int source_vertices = static_cast<int>(std::llround(base_flat[1]));

  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 2.0;

  // Seed A: too many fractions (== source_vertex_count) -> applied=false.
  std::vector<double> seed_a;
  seed_a.reserve(static_cast<std::size_t>(source_vertices));
  for (int i = 0; i < source_vertices; ++i) {
    seed_a.push_back(static_cast<double>(i) / source_vertices);
  }

  // Seed B: 8 valid fractions, well below source_vertex_count.
  const std::vector<double> seed_b = {0.0, 0.125, 0.25, 0.375,
                                      0.5, 0.625, 0.75, 0.875};

  // Build 3 slightly-translated frames.
  struct FrameFlat { double t_sec; std::vector<double> flat; };
  std::vector<FrameFlat> frames;
  for (int f = 0; f < 3; ++f) {
    FrameFlat ff;
    ff.t_sec = static_cast<double>(f) / 24.0;
    ff.flat  = base_flat;
    const double shift = static_cast<double>(f) * 1.5;
    for (std::size_t i = 2; i < ff.flat.size(); i += 6) {
      ff.flat[i] += shift;
    }
    frames.push_back(std::move(ff));
  }

  // Verify seed A fails on any frame: too-many-fractions is deterministic.
  {
    bbsolver::PathFrameFitResult probe =
        bbsolver::FitShapeFlatFrameAtFractions(frames[0].flat, seed_a, opts);
    assert(probe.ok && !probe.applied);
  }

  // Simulate seed B trial: must pass for every frame with consistent vertex
  // count and within tolerance.
  bbsolver::PropertySamples ps;
  ps.property.kind        = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions  = static_cast<int>(base_flat.size());

  double seed_b_max_err = 0.0;
  for (const FrameFlat& ff : frames) {
    bbsolver::PathFrameFitResult fit =
        bbsolver::FitShapeFlatFrameAtFractions(ff.flat, seed_b, opts);
    assert(fit.ok && fit.applied);
    assert(fit.fitted_vertex_count == static_cast<int>(seed_b.size()));
    assert(fit.max_outline_error <= opts.outline_tolerance + 1e-9);
    seed_b_max_err = std::max(seed_b_max_err, fit.max_outline_error);

    bbsolver::Sample s;
    s.t_sec = ff.t_sec;
    s.v     = std::move(fit.fitted);
    ps.samples.push_back(std::move(s));
  }
  (void)seed_b_max_err;

  // Seed B's winning output must form a single stable regime.
  const bbsolver::SingleRegimeResult regime = bbsolver::BuildSingleStableRegime(ps);
  assert(regime.ok);
  assert(regime.regime.vertex_count == static_cast<int>(seed_b.size()));
  assert(regime.regime.samples.samples.size() == frames.size());
  assert(regime.regime.dimensions == 2 + 6 * static_cast<int>(seed_b.size()));
}

// ---------------------------------------------------------------------------
// Stage-2 reduced-path temporal compression tests
// ---------------------------------------------------------------------------

static void TestStage2ReducedPathTemporalBezierCompressesAgainstSource() {
  const Stage2TemporalFixture fixture = MakeCoherentStage2Fixture();
  assert(fixture.frame_outline_error <= 1e-9);
  assert(fixture.source.property.dimensions > fixture.reduced.property.dimensions);

  bbsolver::SolverConfig cfg;
  cfg.tolerance = 0.05;
  cfg.allow_shape_temporal_bezier = true;
  cfg.max_iters_per_segment = 160;

  const bbsolver::PropertyKeys reduced_keys =
      bbsolver::SolveProperty(fixture.reduced, cfg, fixture.comp, bbsolver::FitSegment);

  assert(reduced_keys.converged);
  assert(reduced_keys.keys.size() == 2);
  assert(reduced_keys.segments.size() == 1);
  assert(reduced_keys.segments.front().reason == "shape_temporal_bezier_ok");

  bbsolver::PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options.outline_tolerance = cfg.tolerance;
  const bbsolver::PathTemporalValidationResult source_validation =
      bbsolver::ValidatePathTemporalCandidate(
          fixture.source, reduced_keys, validation_options);
  assert(source_validation.ok);
  assert(source_validation.max_outline_error <= cfg.tolerance + 1e-9);
  assert(source_validation.max_outline_error <= fixture.frame_outline_error + 1e-6);
}

// ---------------------------------------------------------------------------
// Stage-2 geometry refinement tests
// ---------------------------------------------------------------------------

static void TestGeometryRefinementAtFractions() {
  // The stage-2 fixture has 25 frames of a 52-vertex square that moves smoothly
  // through AE bezier space. RefinePathGeometryAtFractions should re-fit every
  // source frame at corner fractions {0, 0.25, 0.5, 0.75} and achieve the same
  // tight tolerance as the per-frame fits in MakeCoherentStage2Fixture.
  const Stage2TemporalFixture fixture = MakeCoherentStage2Fixture();
  const std::vector<double> corner_fractions = {0.0, 0.25, 0.5, 0.75};

  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 0.05;

  const bbsolver::PathGeometryRefinementResult refined =
      bbsolver::RefinePathGeometryAtFractions(
          fixture.source, corner_fractions, opts);

  assert(refined.ok);
  assert(refined.applied);
  assert(refined.frames_refined ==
         static_cast<int>(fixture.source.samples.size()));
  assert(refined.refined_max_error <= opts.outline_tolerance + 1e-9);
  assert(refined.refined_samples.samples.size() ==
         fixture.source.samples.size());
  assert(refined.refined_samples.property.dimensions == 2 + 6 * 4);
  assert(!refined.notes.empty());

  for (std::size_t i = 0; i < fixture.source.samples.size(); ++i) {
    const bbsolver::Sample& s = refined.refined_samples.samples[i];
    assert(s.v.size() == static_cast<std::size_t>(2 + 6 * 4));
    assert(static_cast<int>(std::llround(s.v[1])) == 4);
    assert(std::abs(s.t_sec - fixture.source.samples[i].t_sec) < 1e-9);
  }
}

static void TestGeometryRefinementFailsOnMalformedSource() {
  // A truncated shape_flat (claims 4 vertices but only has 8 scalars) must
  // cause ok=false, applied=false, and a non-empty failure note.
  bbsolver::PropertySamples bad_source;
  bad_source.property.kind = bbsolver::ValueKind::Custom;
  bad_source.property.units_label = "shape_flat";
  bbsolver::Sample s;
  s.t_sec = 0.0;
  s.v = {1.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};  // too short for 4 vertices
  bad_source.samples.push_back(s);

  const std::vector<double> fractions = {0.0, 0.25, 0.5, 0.75};
  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 0.5;

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(bad_source, fractions, opts);

  assert(!result.ok);
  assert(!result.applied);
  assert(result.frames_refined == 0);
  assert(!result.notes.empty());
}

static void TestGeometryRefinementRequiresTolerancePass() {
  std::vector<std::pair<double, std::vector<double>>> frames;
  frames.push_back({0.0, MakeRedundantSquareFlat(0.0, 0.0, 100.0, 12.0)});
  bbsolver::PropertySamples source =
      MakeShapeFlatSamples("unit/stage2/tolerance_fail", frames);

  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 0.01;

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(
          source, {0.0, 0.25, 0.5, 0.75}, opts);

  assert(!result.ok);
  assert(!result.applied);
  assert(result.notes.find("failed_at_t=") != std::string::npos);
}

// Acceptance must be based on source-outline validation, not internal solver
// state. When property_keys.converged=false and max_err is large (internal
// solver residuals) but ValidatePathTemporalCandidate reports ok=true (actual
// outline deviation within tolerance), the candidate should be accepted.
static void TestAcceptanceUsesOutlineValidationNotInternalConvergence() {
  constexpr double fps = 24.0;
  constexpr double tolerance = 2.5;

  // Source: 3 identical static frames -> linear interpolation is exact.
  const std::vector<double> source_frame = MakeRedundantSquareFlat(0.0, 0.0, 100.0);
  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/pipeline/outline_vs_internal",
      {{0.0 / fps, source_frame},
       {1.0 / fps, source_frame},
       {2.0 / fps, source_frame}});

  const std::vector<double> reduced_frame =
      ReducedSquareFromSource(source_frame, tolerance, nullptr);

  // Candidate: 2 keys that reproduce the source exactly at both endpoints.
  // Internal solver state is set to FAILING (converged=false, max_err huge).
  bbsolver::PropertyKeys candidate_keys;
  candidate_keys.property_id = "unit/pipeline/outline_candidate";
  candidate_keys.converged   = false;   // internal solver: not converged
  candidate_keys.max_err     = 999.0;  // internal scalar error: absurdly high
  candidate_keys.keys.push_back(LinearShapeKey(0.0 / fps, reduced_frame));
  candidate_keys.keys.push_back(LinearShapeKey(2.0 / fps, reduced_frame));

  bbsolver::PathTemporalValidationOptions val_opts;
  val_opts.frame_fit_options.outline_tolerance = tolerance;

  const bbsolver::PathTemporalValidationResult validation =
      bbsolver::ValidatePathTemporalCandidate(source, candidate_keys, val_opts);

  // Source-outline validation must PASS: linear interp between identical
  // shapes is geometrically exact.
  assert(validation.ok);
  assert(validation.max_outline_error <= tolerance + 1e-9);
  assert(validation.samples_checked == 3);

  // The pipeline logic: outline validation overrides internal convergence.
  const bool outline_converged = validation.samples_checked > 0
      ? validation.ok               // TRUE from outline
      : candidate_keys.converged;   // FALSE from internal
  const double outline_err = validation.samples_checked > 0
      ? validation.max_outline_error  // small
      : candidate_keys.max_err;       // 999.0

  assert(outline_converged);        // Sourced from validation, not internal
  assert(outline_err <= tolerance); // Sourced from outline, not 999.0

  const bbsolver::ReplacementAcceptanceVerdict verdict =
      bbsolver::EvaluateReplacementAcceptance(
          static_cast<int>(candidate_keys.keys.size()),  // 2
          outline_err,
          outline_converged,
          /*candidate_fitted_vertices=*/4,
          /*original_keys=*/3,
          /*original_max_err=*/0.0,
          /*original_source_vertices=*/52,
          /*original_converged=*/true,
          tolerance);

  // Fewer keys (2 < 3) and outline-converged -> accept.
  assert(verdict.use_candidate);
  // The internal converged=false / max_err=999 must NOT appear as the cause
  // of rejection since acceptance is governed by outline validation.
  assert(verdict.decision_note.find("candidate_invalid") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Variable-topology / raw-frame hardening tests
// ---------------------------------------------------------------------------

// ValidatePathTemporalCandidate must handle a source with variable vertex
// counts across frames. Each source frame is independently valid shape_flat
// even if frame counts differ; outline error is measured per frame.
static void TestValidateVariableTopologySourceAgainstReducedCandidate() {
  constexpr double fps = 24.0;

  // Source: frames with varying vertex counts (8, 52, 8).
  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/variable_topology/source",
      {{0.0 / fps, MakeFlat(true, 8)},
       {1.0 / fps, MakeRedundantRectFlat(true)},  // 52 vertices
       {2.0 / fps, MakeFlat(true, 8)}});

  // Candidate: 2 keys with 4-vertex reduced shapes. The geometry differs
  // from the source, so outline error will be nonzero.
  bbsolver::PropertyKeys candidate;
  candidate.property_id = "unit/variable_topology/candidate";
  candidate.keys.push_back(LinearShapeKey(0.0 / fps, MakeFlat(true, 4)));
  candidate.keys.push_back(LinearShapeKey(2.0 / fps, MakeFlat(true, 4)));

  bbsolver::PathTemporalValidationOptions opts;
  opts.frame_fit_options.outline_tolerance = 0.5;

  const bbsolver::PathTemporalValidationResult result =
      bbsolver::ValidatePathTemporalCandidate(source, candidate, opts);

  // All 3 frames evaluated, including the variable-count 52-vertex frame.
  assert(result.samples_checked == 3);
  // Outline error is measurable (geometric mismatch between 4-vert and 8/52-vert).
  assert(result.max_outline_error > 0.0);
  // Notes always populated.
  assert(!result.notes.empty());
}

// RefinePathGeometryAtFractions must accept frames already at the target vertex
// count (fit.applied=false because source_vertex_count == expected_count) as
// long as the outline error is within tolerance. This is the variable-topology
// edge case: some frames may not need reduction.
static void TestRefineGeometryHandlesFrameAlreadyAtTargetVertexCount() {
  const std::vector<double> big_frame = MakeRedundantRectFlat(true);  // 52 vertices
  const std::vector<double> corner_fractions = {0.0, 0.25, 0.5, 0.75};

  bbsolver::PathFrameFitOptions opts;
  opts.outline_tolerance = 2.0;

  // Derive a proper 4-vertex shape from the big frame at the target fractions.
  const bbsolver::PathFrameFitResult ref_fit =
      bbsolver::FitShapeFlatFrameAtFractions(big_frame, corner_fractions, opts);
  assert(ref_fit.ok && ref_fit.applied && ref_fit.fitted_vertex_count == 4);
  const std::vector<double>& small_frame = ref_fit.fitted;  // already 4 vertices

  // Source: big frame (52 verts, needs reduction) + small frame (4 verts,
  // already at target count, applied=false from FitShapeFlatFrameAtFractions).
  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/refine/variable_topology",
      {{0.0 / 24.0, big_frame},
       {1.0 / 24.0, small_frame}});

  const bbsolver::PathGeometryRefinementResult result =
      bbsolver::RefinePathGeometryAtFractions(source, corner_fractions, opts);

  // Both frames succeed: big frame via applied=true, small frame via tolerance
  // check (max_outline_error near zero even with applied=false).
  assert(result.ok);
  assert(result.applied);
  assert(result.frames_refined == 2);
  assert(result.refined_samples.property.dimensions == 2 + 6 * 4);
  for (const bbsolver::Sample& s : result.refined_samples.samples) {
    assert(static_cast<int>(std::llround(s.v[1])) == 4);
    assert(static_cast<int>(s.v.size()) == 2 + 6 * 4);
  }
}

// Simulates the ShapeFlatFrameKeyFallback pattern: one PropertyKey per source
// frame, key.v == source frame's shape_flat. When evaluated at exact source
// sample times (which are also key times), EvalKeysAt returns the unmodified
// key value, so the outline error against the source is zero.
static void TestRawFrameKeysProduceZeroOutlineErrorAtSampleTimes() {
  constexpr double fps = 24.0;

  const std::vector<double> frame_a = MakeRedundantSquareFlat(0.0, 0.0, 100.0);
  const std::vector<double> frame_b = MakeRedundantSquareFlat(50.0, 20.0, 80.0);

  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/raw_keys/source",
      {{0.0 / fps, frame_a},
       {1.0 / fps, frame_b},
       {2.0 / fps, frame_a}});

  // Build raw-frame keys: one key per source sample, key.v = source frame's v.
  // This is exactly what ShapeFlatFrameKeyFallback produces.
  bbsolver::PropertyKeys raw_keys;
  raw_keys.property_id = "unit/raw_keys/candidate";
  raw_keys.converged = true;
  for (const bbsolver::Sample& s : source.samples) {
    raw_keys.keys.push_back(LinearShapeKey(s.t_sec, s.v));
  }

  bbsolver::PathTemporalValidationOptions opts;
  opts.frame_fit_options.outline_tolerance = 0.01;

  const bbsolver::PathTemporalValidationResult result =
      bbsolver::ValidatePathTemporalCandidate(source, raw_keys, opts);

  // At exact key times EvalKeysAt returns the original value -> zero error.
  assert(result.ok);
  assert(result.max_outline_error < 1e-9);
  assert(result.samples_checked == 3);
}

// Replacement acceptance compares candidate key count against the currently
// supplied original baseline. A raw-frame fallback can still be beaten, but
// the decision must come from strict key reduction, not from accepting a key
// increase solely because vertices were reduced.
static void TestAcceptanceUsesRawFrameCountAsOriginalKeyCount() {
  // Noodle probe scenario: 73 source samples -> 73 raw fallback keys.
  // Candidate: 72 keys, 25 vertices.
  const bbsolver::ReplacementAcceptanceVerdict verdict_reduced =
      bbsolver::EvaluateReplacementAcceptance(
          /*candidate_keys=*/72,
          /*candidate_max_err=*/0.4,
          /*candidate_converged=*/true,
          /*candidate_fitted_vertices=*/25,
          /*original_keys=*/73,  // raw_frame_keys from fallback
          /*original_max_err=*/0.0,
          /*original_source_vertices=*/52,
          /*original_converged=*/true,
          /*tolerance=*/0.5);

  // Strict key reduction: 72 < 73.
  assert(verdict_reduced.use_candidate);
  assert(verdict_reduced.decision_note.find("candidate_keys=72 < original_keys=73") !=
         std::string::npos);

  // Modest vertex reduction (43 of 52): candidate needs more keys than source
  // frames -> rejected.
  const bbsolver::ReplacementAcceptanceVerdict verdict_modest =
      bbsolver::EvaluateReplacementAcceptance(
          /*candidate_keys=*/74,
          /*candidate_max_err=*/0.4,
          /*candidate_converged=*/true,
          /*candidate_fitted_vertices=*/43,
          /*original_keys=*/73,
          /*original_max_err=*/0.0,
          /*original_source_vertices=*/52,
          /*original_converged=*/true,
          /*tolerance=*/0.5);

  assert(!verdict_modest.use_candidate);
  assert(verdict_modest.decision_note.find("path_replacement_candidate_keys") !=
         std::string::npos);
}

static void TestStage2SourceOutlineValidationRejectsMissedMidFrameBulge() {
  constexpr double fps = 24.0;
  constexpr double tolerance = 0.5;
  const std::vector<double> endpoint_source =
      MakeRedundantSquareFlat(0.0, 0.0, 100.0);
  const std::vector<double> bulged_source =
      MakeRedundantSquareFlat(0.0, 0.0, 100.0, 8.0);
  const std::vector<double> reduced_endpoint =
      ReducedSquareFromSource(endpoint_source, tolerance, nullptr);

  const std::vector<std::pair<double, std::vector<double>>> source_frames = {
      {0.0 / fps, endpoint_source},
      {1.0 / fps, bulged_source},
      {2.0 / fps, endpoint_source},
  };
  const bbsolver::PropertySamples source =
      MakeShapeFlatSamples("unit/stage2/source_bulge", source_frames);
  const bbsolver::CompInfo comp = Stage2Comp(3, fps);

  std::vector<bbsolver::Key> low_key_candidate;
  low_key_candidate.push_back(LinearShapeKey(0.0 / fps, reduced_endpoint));
  low_key_candidate.push_back(LinearShapeKey(2.0 / fps, reduced_endpoint));

  bbsolver::SolverConfig cfg;
  cfg.tolerance = tolerance;
  cfg.allow_shape_temporal_bezier = true;

  bbsolver::PropertyKeys candidate_keys;
  candidate_keys.property_id = "unit/stage2/low_key_candidate";
  candidate_keys.keys = low_key_candidate;

  bbsolver::PathTemporalValidationOptions validation_options;
  validation_options.frame_fit_options.outline_tolerance = cfg.tolerance;
  const bbsolver::PathTemporalValidationResult source_validation =
      bbsolver::ValidatePathTemporalCandidate(
          source, candidate_keys, validation_options);
  assert(!source_validation.ok);
  assert(source_validation.max_outline_error > cfg.tolerance);

  const bbsolver::ReplacementAcceptanceVerdict verdict =
      bbsolver::EvaluateReplacementAcceptance(
          static_cast<int>(low_key_candidate.size()),
          source_validation.max_outline_error,
          source_validation.ok,
          /*candidate_fitted_vertices=*/4,
          /*original_keys=*/3,
          /*original_max_err=*/0.0,
          /*original_source_vertices=*/52,
          /*original_converged=*/true,
          cfg.tolerance);
  assert(!verdict.use_candidate);
  assert(verdict.decision_note.find("candidate_invalid") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Near-miss retry logic tests
// ---------------------------------------------------------------------------

// The near-miss retry triggers when candidate_max_err is between tolerance and
// tolerance * 1.5. Verify the three cases: below tolerance (no retry needed),
// in the near-miss window (retry fires), and above 1.5x (too far, raw fallback).
static void TestNearMissFactorBoundary() {
  constexpr double tolerance = 0.5;
  // Values relative to tolerance.
  const double below   = 0.48;   // passes directly; no retry needed
  const double near    = 0.515;  // in [tolerance, tolerance*1.5] -> near-miss
  const double too_far = 0.85;   // > tolerance*1.5 -> not a near-miss

  // Below tolerance: EvaluateReplacementAcceptance accepts directly.
  {
    const auto v = bbsolver::EvaluateReplacementAcceptance(
        /*candidate_keys=*/72, below, /*candidate_converged=*/true,
        /*candidate_fitted_vertices=*/22,
        /*original_keys=*/135, /*original_max_err=*/0.0,
        /*original_source_vertices=*/28, /*original_converged=*/true,
        tolerance);
    assert(v.use_candidate);  // passes, no retry needed
  }

  // Near-miss: EvaluateReplacementAcceptance rejects (candidate_invalid), but
  // the decision is within the kNearMissRetryFactor window.
  {
    const auto v = bbsolver::EvaluateReplacementAcceptance(
        /*candidate_keys=*/72, near, /*candidate_converged=*/true,
        /*candidate_fitted_vertices=*/22,
        /*original_keys=*/135, /*original_max_err=*/0.0,
        /*original_source_vertices=*/28, /*original_converged=*/true,
        tolerance);
    assert(!v.use_candidate);
    assert(v.decision_note.find("candidate_invalid") != std::string::npos);
    // near-miss check: near <= tolerance * 1.5
    assert(near <= tolerance * 1.5);
    assert(near > tolerance);
  }

  // Too far: also rejected, but outside the retry window (no retry would fire).
  {
    const auto v = bbsolver::EvaluateReplacementAcceptance(
        /*candidate_keys=*/72, too_far, /*candidate_converged=*/true,
        /*candidate_fitted_vertices=*/22,
        /*original_keys=*/135, /*original_max_err=*/0.0,
        /*original_source_vertices=*/28, /*original_converged=*/true,
        tolerance);
    assert(!v.use_candidate);
    // too_far is outside the near-miss window -> retry would NOT fire
    assert(too_far > tolerance * 1.5);
  }
}

// Simulate the current build007 noodle-path cache: the 22-vertex candidate is
// a near miss, the 24-vertex retry still falls back, and the 26-vertex retry
// reaches the cached passing shape/key count. The production retry loop gets
// its targets from BuildShapeFlatReplacementTargetLadder, so pin that contract
// here as well as the acceptance verdicts at each rung before the first pass.
static void TestNearMissBuild007LadderAndAcceptance() {
  constexpr double tolerance = 0.5;
  constexpr int    source_samples = 135;  // = raw fallback key count
  constexpr int    source_min_vertices = 28;
  constexpr int    initial_target_vertices = 22;
  constexpr int    candidate_keys = 94;

  bbsolver::PathReplacementTargetLadderOptions ladder_options;
  ladder_options.min_target_vertices = initial_target_vertices;
  ladder_options.max_candidate_targets = 4;
  const std::vector<int> ladder =
      bbsolver::BuildShapeFlatReplacementTargetLadder(
          initial_target_vertices,
          source_min_vertices,
          ladder_options);
  const std::vector<int> expected_ladder = {22, 24, 26, 27};
  assert(ladder == expected_ladder);

  struct Build007Attempt {
    int target_vertices = 0;
    double candidate_max_err = 0.0;
    bool use_candidate = false;
  };
  const std::vector<Build007Attempt> attempts = {
      // First attempt: 22 vertices, error 0.515 -> rejected near miss.
      {22, 0.515, false},
      // First retry: current cache reports raw fallback at 24; model the
      // acceptance gate with an over-tolerance near-miss value.
      {24, 0.505, false},
      // Second retry: current cache accepts 26 vertices after residual-budget
      // temporal solving.
      {26, 0.289, true},
  };

  for (const Build007Attempt& attempt : attempts) {
    assert(std::find(ladder.begin(), ladder.end(), attempt.target_vertices) !=
           ladder.end());
    const auto verdict = bbsolver::EvaluateReplacementAcceptance(
        candidate_keys,
        attempt.candidate_max_err,
        /*candidate_converged=*/true,
        attempt.target_vertices,
        source_samples,
        /*original_max_err=*/0.0,
        source_min_vertices,
        /*original_converged=*/true,
        tolerance);

    assert(verdict.use_candidate == attempt.use_candidate);
    if (attempt.use_candidate) {
      assert(attempt.candidate_max_err <= tolerance);
      assert(verdict.decision_note.find("candidate_keys=94") !=
             std::string::npos);
      assert(verdict.decision_note.find("original_keys=135") !=
             std::string::npos);
    } else {
      assert(attempt.candidate_max_err > tolerance);
      assert(attempt.candidate_max_err <= tolerance * 1.5);
      assert(verdict.decision_note.find("candidate_invalid") !=
             std::string::npos);
    }
  }

  // The real ladder includes 27 as the last legal retry target. In the current
  // cache production should not need it because 26 is the first passing rung,
  // but its independent acceptance value is still under tolerance.
  const auto final_ladder_verdict = bbsolver::EvaluateReplacementAcceptance(
      candidate_keys,
      /*candidate_max_err=*/0.289,
      /*candidate_converged=*/true,
      /*candidate_fitted_vertices=*/27,
      source_samples,
      /*original_max_err=*/0.0,
      source_min_vertices,
      /*original_converged=*/true,
      tolerance);
  assert(final_ladder_verdict.use_candidate);
}

// When all retries also fail validation (e.g., the motion is truly beyond what
// any reduced topology can represent), the pipeline must fall back to raw frame
// keys. The raw fallback key count equals the source sample count.
static void TestRawFallbackIsLastResortWhenAllRetriesFail() {
  constexpr double tolerance = 0.5;
  constexpr int    source_samples = 135;

  // All retry attempts: simulate a sparse ladder (24, 26, 27), all failing.
  for (int retry_verts : {24, 26, 27}) {
    const auto retry_verdict = bbsolver::EvaluateReplacementAcceptance(
        /*candidate_keys=*/source_samples,  // worst case: one key per sample
        /*candidate_max_err=*/0.55,          // still above tolerance
        /*candidate_converged=*/true,
        /*candidate_fitted_vertices=*/retry_verts,
        /*original_keys=*/source_samples,
        /*original_max_err=*/0.0,
        /*original_source_vertices=*/28,
        /*original_converged=*/true,
        tolerance);
    // None of the retries pass: either candidate_invalid or not worth it.
    assert(!retry_verdict.use_candidate);
  }

  // After exhausting retries, the raw fallback is applied. Verify the raw
  // fallback keys have the correct count (= source sample count) and that
  // ValidatePathTemporalCandidate against them produces zero outline error.
  const std::vector<double> src_frame = MakeRedundantSquareFlat(0.0, 0.0, 100.0);
  const bbsolver::PropertySamples source = MakeShapeFlatSamples(
      "unit/near_miss/fallback",
      {{0.0 / 24.0, src_frame},
       {1.0 / 24.0, src_frame},
       {2.0 / 24.0, src_frame}});

  // Construct raw-frame keys (simulates ShapeFlatFrameKeyFallback output).
  bbsolver::PropertyKeys raw_keys;
  raw_keys.property_id = "unit/near_miss/raw";
  raw_keys.converged = true;
  for (const bbsolver::Sample& s : source.samples) {
    raw_keys.keys.push_back(LinearShapeKey(s.t_sec, s.v));
  }
  assert(raw_keys.keys.size() == source.samples.size());

  // Raw keys at source sample times produce zero outline error.
  bbsolver::PathTemporalValidationOptions opts;
  opts.frame_fit_options.outline_tolerance = tolerance;
  const bbsolver::PathTemporalValidationResult check =
      bbsolver::ValidatePathTemporalCandidate(source, raw_keys, opts);
  assert(check.ok);
  assert(check.max_outline_error < 1e-9);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
  TestUniformTopology();
  TestTopologyChanges();
  TestSingleSample();
  TestOpenPath();
  TestClosedFlagFlipRejected();
  TestEmptyStreamRejected();
  TestNonShapeFlatRejected();
  TestMalformedHeaderRejected();
  TestDimensionsNotOriginalHighDim();
  TestPropertyMetadataPreserved();
  TestDecomposeIntegration();
  // BuildSingleStableRegime
  TestSingleRegimeUniform();
  TestSingleRegimeMultiTopologyFails();
  TestSingleRegimeClosedFlagFlipFails();
  TestSingleRegimeTargetVertexCountMatch();
  TestSingleRegimeTargetVertexCountMismatch();
  TestSingleRegimeZeroTargetIsUnconstrained();
  // Cross-verification: FitShapeFlatFrame -> pipeline
  TestFitterOutputDimensionContract();
  TestFitterSeamPreservationContract();
  TestFitterClosedFlagPreservationContract();
  TestFitterVariableOutputFeedsToPipelineFallback();
  // EstimateLinearKeyCount
  TestEstimateLinearKeyCountPerfectlyLinear();
  TestEstimateLinearKeyCountSingleSample();
  TestEstimateLinearKeyCountUsesSampleTimes();
  TestEstimateLinearKeyCountDimensionMismatchConservative();
  TestEstimateLinearKeyCountIncoherent();
  TestEstimateLinearKeyCountWithinTolerance();
  TestEstimateLinearKeyCountMonotonic();
  // AssessReplacementCandidate
  TestAssessReplacementCandidateCoherentBeatsOriginal();
  TestAssessReplacementCandidateIncoherentFails();
  TestAssessReplacementCandidateEqualKeysFewerVerticesPasses();
  TestAssessReplacementCandidateNoodleLikeScenario();
  TestAssessReplacementCandidateStage1VertexReductionRejectsKeyIncrease();
  TestAssessReplacementCandidateReportsReason();
  // Fraction-coherence integration
  TestFractionCoherenceProducesStableRegime();
  TestAutomaticFitFractionsRoundTrip();
  TestMultiSeedSelectsLaterSeedWhenFirstFails();
  // Stage-2 geometry refinement
  TestGeometryRefinementAtFractions();
  TestGeometryRefinementFailsOnMalformedSource();
  TestGeometryRefinementRequiresTolerancePass();
  TestAcceptanceUsesOutlineValidationNotInternalConvergence();
  // Variable-topology / raw-frame hardening
  TestValidateVariableTopologySourceAgainstReducedCandidate();
  TestRefineGeometryHandlesFrameAlreadyAtTargetVertexCount();
  TestRawFrameKeysProduceZeroOutlineErrorAtSampleTimes();
  TestAcceptanceUsesRawFrameCountAsOriginalKeyCount();
  // Stage-2 reduced-path temporal compression
  TestStage2ReducedPathTemporalBezierCompressesAgainstSource();
  TestStage2SourceOutlineValidationRejectsMissedMidFrameBulge();
  // Near-miss retry logic
  TestNearMissFactorBoundary();
  TestNearMissBuild007LadderAndAcceptance();
  TestRawFallbackIsLastResortWhenAllRetriesFail();
  // EvaluateReplacementAcceptance
  TestAcceptanceStrictlyFewerKeys();
  TestAcceptanceStrictlyMoreKeys();
  TestAcceptanceStage1MoreKeysMaterialReduction();
  TestAcceptanceStage1RejectsUnboundedKeyPenalty();
  TestAcceptanceVertexPreferredDefaultOffRejectsMoreKeys();
  TestAcceptanceVertexPreferredRejectsGuardedKeyGrowth();
  TestAcceptanceVertexPreferredRejectsLooseToleranceNoodleTradeoff();
  TestAcceptanceVertexPreferredRejectsExcessiveKeyGrowth();
  TestAcceptanceVertexPreferredRejectsRawFrameRegression();
  TestAcceptanceVertexPreferredRejectsWeakVertexReduction();
  TestFastVertexPreferenceAcceptsGuardedReduction();
  TestFastVertexPreferenceRequiresKeyReduction();
  TestFastVertexPreferenceRequiresValidationSamples();
  TestBuildFastVertexPreferenceInputCopiesSummaryAndGates();
  TestBuildFastVertexPreferenceNoteInputCopiesFields();
  TestValidationSummaryUsesSourceValidationWhenAvailable();
  TestValidationSummaryFallsBackToSolverResultWithoutSamples();
  TestApplyValidationSummaryToKeysOnlyWhenSamplesChecked();
  TestRetryEligibilityAcceptsNearMissWithKeyGate();
  TestRetryEligibilityBlocksSharpOnlyFailure();
  TestRetryEligibilityBlocksKeyGrowth();
  TestBuildRetryEligibilityInputCopiesValidationFields();
  TestRetryEligibilityRequiresValidationSamplesAndHeadroom();
  TestAcceptanceEqualKeysFewerVertices();
  TestAcceptanceEqualKeysSameVertices();
  TestAcceptanceEqualKeysMoreVertices();
  TestAcceptanceOriginalNotConverged();
  TestAcceptanceOriginalExceedsTolerance();
  TestAcceptanceOriginalAtToleranceBoundary();
  TestAcceptanceCandidateInvalidFallsBack();
  TestAcceptanceBothInvalidChoosesLessBadResult();
  TestAcceptanceNoodleScenario();
  TestAcceptanceEqualKeysNoodleImproved();
  TestAcceptanceEqualKeysMateriallyReducedAtTolerance();
  TestAcceptanceEqualKeysMateriallyReducedOverToleranceRejected();
  TestAcceptanceEqualKeysNonReducedCandidateRejectedEvenWhenValid();
  TestAcceptanceNoteAlwaysPopulated();
  return 0;
}
