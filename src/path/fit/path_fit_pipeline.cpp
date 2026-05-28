#include "bbsolver/path/fit/path_fit_pipeline.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>

namespace bbsolver {
namespace {

constexpr int kPathHeaderScalars = 2;
constexpr int kScalarsPerVertex  = 6;

bool IsShapeFlatPath(const PropertySamples& ps) {
  return ps.property.kind == ValueKind::Custom && ps.property.units_label == "shape_flat";
}

struct RawHeader {
  bool ok          = false;
  bool closed      = false;
  int vertex_count = 0;
};

RawHeader DecodeHeader(const std::vector<double>& flat) {
  RawHeader h;
  if (static_cast<int>(flat.size()) < kPathHeaderScalars) {
    return h;
  }
  h.closed       = static_cast<int>(std::llround(flat[0])) != 0;
  h.vertex_count = static_cast<int>(std::llround(flat[1]));
  const int expected = kPathHeaderScalars + h.vertex_count * kScalarsPerVertex;
  if (h.vertex_count < 1 || static_cast<int>(flat.size()) != expected) {
    return h;
  }
  h.ok = true;
  return h;
}

int MaxShapeFlatVertexCount(const PropertySamples& ps) {
  int max_vertices = 0;
  for (const Sample& sample: ps.samples) {
    const RawHeader h = DecodeHeader(sample.v);
    if (!h.ok) {
      return 0;
    }
    max_vertices = std::max(max_vertices, h.vertex_count);
  }
  return max_vertices;
}

StablePathRegime BuildRegime(const PropertySamples& ps,
                             std::size_t regime_start,
                             std::size_t regime_end_exclusive,
                             int vertex_count,
                             bool closed) {
  StablePathRegime regime;
  regime.start_sample_idx = static_cast<int>(regime_start);
  regime.end_sample_idx   = static_cast<int>(regime_end_exclusive) - 1;
  regime.vertex_count     = vertex_count;
  regime.dimensions       = kPathHeaderScalars + vertex_count * kScalarsPerVertex;
  regime.closed           = closed;

  regime.samples                     = ps;
  regime.samples.property.dimensions = regime.dimensions;
  regime.samples.t_start_sec         = ps.samples[regime_start].t_sec;
  regime.samples.t_end_sec           = ps.samples[regime_end_exclusive - 1].t_sec;
  regime.samples.samples.clear();
  regime.samples.samples.reserve(regime_end_exclusive - regime_start);
  for (std::size_t j = regime_start; j < regime_end_exclusive; ++j) {
    regime.samples.samples.push_back(ps.samples[j]);
  }
  return regime;
}

}  // namespace

PathCorrespondenceResult BuildStableRegimes(const PropertySamples& ps) {
  PathCorrespondenceResult result;

  if (!IsShapeFlatPath(ps)) {
    result.notes = "not shape_flat";
    return result;
  }
  if (ps.samples.empty()) {
    result.notes = "empty sample stream";
    return result;
  }

  // Decode and validate every frame header up front so failures are reported
  // before any regimes are emitted.
  std::vector<RawHeader> headers;
  headers.reserve(ps.samples.size());
  for (const Sample& s: ps.samples) {
    const RawHeader h = DecodeHeader(s.v);
    if (!h.ok) {
      result.notes = "malformed shape_flat header at t=" + std::to_string(s.t_sec);
      return result;
    }
    headers.push_back(h);
  }

  // Global invariant: closed flag must not flip at any sample.
  const bool first_closed = headers[0].closed;
  for (std::size_t i = 1; i < headers.size(); ++i) {
    if (headers[i].closed != first_closed) {
      result.notes = "closed flag changed at sample index " + std::to_string(i) +
                     " (t=" + std::to_string(ps.samples[i].t_sec) + "); " +
                     "stable closed flag required across entire work area";
      return result;
    }
  }

  // Scan for vertex-count transitions and emit one regime per stable run.
  std::size_t regime_start = 0;
  for (std::size_t i = 1; i <= headers.size(); ++i) {
    const bool at_end = (i == headers.size());
    if (!at_end && headers[i].vertex_count == headers[regime_start].vertex_count) {
      continue;
    }
    result.regimes.push_back(BuildRegime(
        ps, regime_start, i, headers[regime_start].vertex_count, first_closed));
    regime_start = i;
  }

  result.ok         = true;
  result.all_stable = (result.regimes.size() == 1);

  if (result.all_stable) {
    const StablePathRegime& r = result.regimes[0];
    result.notes = "stable; vertex_count=" + std::to_string(r.vertex_count) +
                   "; dimensions=" + std::to_string(r.dimensions) +
                   "; samples=" + std::to_string(r.samples.samples.size());
  } else {
    result.notes = "split into " + std::to_string(result.regimes.size()) + " regimes";
  }
  return result;
}

SingleRegimeResult BuildSingleStableRegime(const PropertySamples& ps,
                                           const SingleRegimeOptions& opts) {
  SingleRegimeResult out;
  PathCorrespondenceResult regimes = BuildStableRegimes(ps);
  if (!regimes.ok) {
    out.reason = regimes.notes;
    return out;
  }
  if (!regimes.all_stable) {
    out.reason = regimes.notes;  // "split into N regimes"
    return out;
  }
  // Optional fixed-K check: when caller knows the fitter was given a target
  // vertex count, verify the regime matched it before handing off to the
  // temporal solver.
  if (opts.target_vertex_count > 0 &&
      regimes.regimes.front().vertex_count != opts.target_vertex_count) {
    out.reason = "regime vertex_count=" +
                 std::to_string(regimes.regimes.front().vertex_count) +
                 " != target_vertex_count=" + std::to_string(opts.target_vertex_count);
    return out;
  }
  out.ok     = true;
  out.regime = std::move(regimes.regimes.front());
  return out;
}

int EstimateLinearKeyCount(const PropertySamples& ps, double tolerance) {
  const int n = static_cast<int>(ps.samples.size());
  if (n <= 1) {
    return n;
  }
  const std::size_t dims = ps.samples.front().v.size();
  for (const Sample& sample: ps.samples) {
    if (sample.v.size() != dims) {
      return n;
    }
  }

  // Greedy segment extension: from seg_start, advance seg_end one step at a
  // time checking that every intermediate sample lies within L-inf tolerance of
  // the linear interpolation between samples[seg_start] and samples[seg_end].
  // Once a sample fails the check, close the segment and start a new one.
  int key_count = 1;  // first sample is always a keyframe
  int seg_start = 0;

  while (seg_start < n - 1) {
    int seg_end = seg_start + 1;  // minimum: one step forward

    for (int candidate_end = seg_start + 2; candidate_end < n; ++candidate_end) {
      const std::vector<double>& va = ps.samples[static_cast<std::size_t>(seg_start)].v;
      const std::vector<double>& vb = ps.samples[static_cast<std::size_t>(candidate_end)].v;
      const double ta = ps.samples[static_cast<std::size_t>(seg_start)].t_sec;
      const double tb = ps.samples[static_cast<std::size_t>(candidate_end)].t_sec;
      const double dt = tb - ta;

      bool ok = true;
      for (int mid = seg_start + 1; mid < candidate_end && ok; ++mid) {
        const double tm = ps.samples[static_cast<std::size_t>(mid)].t_sec;
        const double u = std::abs(dt) > 1e-12
                             ? (tm - ta) / dt
: static_cast<double>(mid - seg_start) /
                                   static_cast<double>(candidate_end - seg_start);
        const std::vector<double>& vm = ps.samples[static_cast<std::size_t>(mid)].v;
        for (std::size_t d = 0; d < dims; ++d) {
          const double pred = va[static_cast<std::size_t>(d)] +
                              u * (vb[static_cast<std::size_t>(d)] - va[static_cast<std::size_t>(d)]);
          if (std::abs(pred - vm[static_cast<std::size_t>(d)]) > tolerance) {
            ok = false;
            break;
          }
        }
      }

      if (ok) {
        seg_end = candidate_end;
      } else {
        break;  // greedy: stop at first extension that fails
      }
    }

    ++key_count;
    seg_start = seg_end;
  }

  return key_count;
}

ReplacementCandidateAssessment AssessReplacementCandidate(
    const StablePathRegime& candidate,
    const PropertySamples& original,
    double tolerance) {
  ReplacementCandidateAssessment out;

  const int n_samples  = static_cast<int>(candidate.samples.samples.size());
  const int n_original = static_cast<int>(original.samples.size());

  // Cost proxy: how many per-channel child Bezier solves would decompose need?
  // Each vertex contributes 3 two-dimensional children.
  out.decompose_cost = n_samples * candidate.vertex_count;

  if (n_samples == 0 || n_original == 0) {
    out.reason = "empty sample stream";
    return out;
  }

  out.estimated_candidate_keys = EstimateLinearKeyCount(candidate.samples, tolerance);
  out.estimated_original_keys  = EstimateLinearKeyCount(original, tolerance);

  out.key_reduction_ratio =
      out.estimated_original_keys > 0
          ? static_cast<double>(out.estimated_candidate_keys) /
                static_cast<double>(out.estimated_original_keys)
: std::numeric_limits<double>::infinity();

  const bool one_key_per_sample =
      out.estimated_candidate_keys >= std::max(2, n_samples - 1);
  const int original_max_vertices = MaxShapeFlatVertexCount(original);
  const bool equal_key_vertex_reduction =
      out.estimated_candidate_keys == out.estimated_original_keys &&
      original_max_vertices > 0 &&
      candidate.vertex_count < original_max_vertices;
  if (one_key_per_sample &&
      out.estimated_candidate_keys >= out.estimated_original_keys &&
      !equal_key_vertex_reduction) {
    out.worth_attempting = false;
    out.reason = "estimated_candidate_keys=" +
                 std::to_string(out.estimated_candidate_keys) +
                 " >= estimated_original_keys=" +
                 std::to_string(out.estimated_original_keys) +
                 "; temporal coherence insufficient; candidate effectively needs one key per sample" +
                 "; decompose_cost=" + std::to_string(out.decompose_cost);
  } else {
    out.worth_attempting = true;
    out.reason = "estimated_candidate_keys=" +
                 std::to_string(out.estimated_candidate_keys) +
                 " vs estimated_original_keys=" +
                 std::to_string(out.estimated_original_keys) +
                 "; decompose_cost=" + std::to_string(out.decompose_cost);
    if (equal_key_vertex_reduction) {
      out.reason +=
          "; equal_key_vertex_reduction_precheck; candidate_vertices=" +
          std::to_string(candidate.vertex_count) +
          " < original_source_vertices=" +
          std::to_string(original_max_vertices);
    }
  }
  return out;
}

}  // namespace bbsolver
