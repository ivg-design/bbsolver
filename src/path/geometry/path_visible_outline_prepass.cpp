#include "bbsolver/path/geometry/path_visible_outline_prepass.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"
#include "bbsolver/path/config/path_solver_config.hpp"
#include "bbsolver/routing/property_classification.hpp"
#include "bbsolver/shape/shape_flat_topology.hpp"

namespace bbsolver {

VisibleOutlinePrepassResult TryVisibleOutlinePrepass(
    const PropertySamples& original,
    const SolverConfig& config) {
  VisibleOutlinePrepassResult result;
  result.samples = original;
  if (!IsShapeFlatPath(original) || original.samples.empty()) {
    result.notes = "visible_outline_prepass_skipped: non_shape_flat";
    return result;
  }

  PathFrameFitOptions extract_options = VisibleOutlineFrameFitOptions(config);
  extract_options.outline_tolerance = EffectivePathTolerance(config);
  int source_min = std::numeric_limits<int>::max();
  int source_max = 0;
  for (const Sample& sample : original.samples) {
    const int source_vertices = ShapeFlatVertexCount(sample.v);
    if (source_vertices <= 0) {
      result.notes =
          "visible_outline_prepass_skipped: malformed_shape_flat";
      return result;
    }
    source_min = std::min(source_min, source_vertices);
    source_max = std::max(source_max, source_vertices);
  }

  std::vector<std::size_t> probe_indices;
  const std::size_t sample_count = original.samples.size();
  const std::size_t probe_count = std::min<std::size_t>(9, sample_count);
  if (probe_count <= 1) {
    probe_indices.push_back(0);
  } else {
    for (std::size_t probe = 0; probe < probe_count; ++probe) {
      probe_indices.push_back(
          (probe * (sample_count - 1)) / (probe_count - 1));
    }
  }
  std::sort(probe_indices.begin(), probe_indices.end());
  probe_indices.erase(
      std::unique(probe_indices.begin(), probe_indices.end()),
      probe_indices.end());

  int probe_visible_frames = 0;
  std::string last_warning;

  auto visible_source_for_sample =
      [&](const Sample& sample,
          bool* applied,
          std::string* warning) -> std::vector<double> {
    if (applied != nullptr) {
      *applied = false;
    }
    VisibleShapeFlatOutlineResult visible =
        ExtractVisibleShapeFlatOutline(sample.v, extract_options);
    if (!visible.ok) {
      if (warning != nullptr) {
        *warning = visible.warning;
      }
      return {};
    }
    if (warning != nullptr) {
      *warning = visible.warning;
    }
    if (applied != nullptr) {
      *applied = visible.applied;
    }
    return visible.applied ? std::move(visible.outline) : sample.v;
  };

  for (std::size_t sample_index : probe_indices) {
    const Sample& sample = original.samples[sample_index];
    bool applied = false;
    std::string warning;
    std::vector<double> visible_source =
        visible_source_for_sample(sample, &applied, &warning);
    if (visible_source.empty()) {
      result.notes =
          "visible_outline_prepass_skipped: " + warning;
      return result;
    }
    last_warning = warning;
    if (applied) {
      ++probe_visible_frames;
    }
  }

  if (probe_visible_frames == 0) {
    result.notes =
        "visible_outline_prepass_skipped: " +
        (last_warning.empty() ? std::string("no_self_intersections")
                              : last_warning);
    return result;
  }

  PropertySamples preprocessed = original;
  preprocessed.samples.clear();
  preprocessed.samples.reserve(original.samples.size());
  std::vector<std::vector<double>> visible_sources;
  visible_sources.reserve(original.samples.size());
  std::vector<std::vector<double>> compacted_sources;
  compacted_sources.reserve(original.samples.size());
  int extracted_outline_min = std::numeric_limits<int>::max();
  int extracted_outline_max = 0;
  int outline_min = std::numeric_limits<int>::max();
  int outline_max = 0;
  int applied_frames = 0;
  int compacted_frames = 0;
  int uniformed_frames = 0;
  int uniform_target_vertices = 0;
  int max_dimensions = 0;
  double compact_max_error = 0.0;
  double uniform_max_error = 0.0;
  for (std::size_t i = 0; i < original.samples.size(); ++i) {
    bool applied = false;
    std::string warning;
    std::vector<double> visible_source =
        visible_source_for_sample(original.samples[i], &applied, &warning);
    if (visible_source.empty()) {
      result.notes =
          "visible_outline_prepass_skipped: " + warning;
      return result;
    }
    if (applied) {
      ++applied_frames;
      const int extracted_vertices = ShapeFlatVertexCount(visible_source);
      extracted_outline_min =
          std::min(extracted_outline_min, extracted_vertices);
      extracted_outline_max =
          std::max(extracted_outline_max, extracted_vertices);
    }
    visible_sources.push_back(std::move(visible_source));
  }
  if (applied_frames == 0) {
    result.notes =
        "visible_outline_prepass_skipped: no_full_frame_self_intersections";
    return result;
  }

  const int compact_target_vertices =
      extracted_outline_min == std::numeric_limits<int>::max()
          ? 0
          : extracted_outline_min;
  for (std::size_t i = 0; i < visible_sources.size(); ++i) {
    std::vector<double> visible_source = std::move(visible_sources[i]);
    const int before_vertices = ShapeFlatVertexCount(visible_source);
    if (compact_target_vertices >= 3 &&
        before_vertices > compact_target_vertices) {
      PathFrameFitOptions compact_options = extract_options;
      compact_options.source_vertices_are_semantic_anchors = false;
      compact_options.target_vertex_count = compact_target_vertices;
      PathFrameFitResult compact_fit =
          FitShapeFlatFrame(visible_source, compact_options);
      if (compact_fit.ok &&
          compact_fit.applied &&
          compact_fit.fitted_vertex_count > 0 &&
          compact_fit.fitted_vertex_count < before_vertices &&
          compact_fit.max_outline_error <=
              extract_options.outline_tolerance + 1e-6) {
        visible_source = std::move(compact_fit.fitted);
        compact_max_error =
            std::max(compact_max_error, compact_fit.max_outline_error);
        ++compacted_frames;
      }
    }
    const int outline_vertices = ShapeFlatVertexCount(visible_source);
    outline_min = std::min(outline_min, outline_vertices);
    outline_max = std::max(outline_max, outline_vertices);
    compacted_sources.push_back(std::move(visible_source));
  }

  if (outline_min != std::numeric_limits<int>::max() &&
      outline_max > outline_min &&
      outline_max < source_max) {
    uniform_target_vertices = outline_max;
    int uniform_min = std::numeric_limits<int>::max();
    int uniform_max = 0;
    for (std::vector<double>& visible_source : compacted_sources) {
      const int before_vertices = ShapeFlatVertexCount(visible_source);
      if (uniform_target_vertices >= 3 &&
          before_vertices != uniform_target_vertices) {
        PathFrameFitOptions uniform_options = extract_options;
        uniform_options.source_vertices_are_semantic_anchors = false;
        uniform_options.target_vertex_count = uniform_target_vertices;
        PathFrameFitResult uniform_fit =
            FitShapeFlatFrame(visible_source, uniform_options);
        if (uniform_fit.ok &&
            uniform_fit.fitted_vertex_count == uniform_target_vertices &&
            uniform_fit.max_outline_error <=
                extract_options.outline_tolerance + 1e-6) {
          visible_source = std::move(uniform_fit.fitted);
          uniform_max_error =
              std::max(uniform_max_error, uniform_fit.max_outline_error);
          ++uniformed_frames;
        }
      }
      const int normalized_vertices = ShapeFlatVertexCount(visible_source);
      uniform_min = std::min(uniform_min, normalized_vertices);
      uniform_max = std::max(uniform_max, normalized_vertices);
    }
    outline_min = uniform_min;
    outline_max = uniform_max;
  }

  if (outline_min != outline_max) {
    result.notes =
        "visible_outline_prepass_skipped: unstable_visible_outline_topology"
        "; source_vertices=" + std::to_string(source_min) +
        (source_min == source_max ? std::string{}
                                  : "-" + std::to_string(source_max)) +
        "; outline_vertices=" + std::to_string(outline_min) +
        "-" + std::to_string(outline_max) +
        (extracted_outline_min == std::numeric_limits<int>::max()
             ? std::string{}
             : "; extracted_outline_vertices=" +
                   std::to_string(extracted_outline_min) +
                   (extracted_outline_min == extracted_outline_max
                        ? std::string{}
                        : "-" + std::to_string(extracted_outline_max))) +
        "; compacted_visible_outline_frames=" +
        std::to_string(compacted_frames) +
        "; compact_visible_outline_max_error=" +
        std::to_string(compact_max_error) +
        (uniform_target_vertices > 0
             ? "; uniform_visible_outline_target=" +
                   std::to_string(uniform_target_vertices) +
                   "; uniform_visible_outline_frames=" +
                   std::to_string(uniformed_frames) +
                   "; uniform_visible_outline_max_error=" +
                   std::to_string(uniform_max_error)
             : std::string{});
    return result;
  }

  for (std::size_t i = 0; i < compacted_sources.size(); ++i) {
    std::vector<double> visible_source = std::move(compacted_sources[i]);
    max_dimensions =
        std::max(max_dimensions, static_cast<int>(visible_source.size()));
    Sample sample;
    sample.t_sec = original.samples[i].t_sec;
    sample.v = std::move(visible_source);
    sample.key_timing = original.samples[i].key_timing;
    preprocessed.samples.push_back(std::move(sample));
  }
  if (outline_max >= source_max && outline_min >= source_min) {
    result.notes =
        "visible_outline_prepass_skipped: no_vertex_benefit"
        "; source_vertices=" + std::to_string(source_min) +
        (source_min == source_max ? std::string{}
                                  : "-" + std::to_string(source_max)) +
        "; outline_vertices=" + std::to_string(outline_min) +
        (outline_min == outline_max ? std::string{}
                                    : "-" + std::to_string(outline_max));
    return result;
  }
  preprocessed.property.dimensions = max_dimensions;

  result.applied = true;
  result.samples = std::move(preprocessed);
  result.source_min_vertices = source_min;
  result.source_max_vertices = source_max;
  result.outline_min_vertices = outline_min == std::numeric_limits<int>::max()
                                    ? 0 : outline_min;
  result.outline_max_vertices = outline_max;
  result.fitted_vertices = result.outline_min_vertices;
  result.max_outline_error = 0.0;
  result.notes =
      "visible_outline_prepass; source_vertices=" +
      std::to_string(source_min) +
      (source_min == source_max ? std::string{}
                                : "-" + std::to_string(source_max)) +
      "; outline_vertices=" + std::to_string(result.outline_min_vertices) +
      (result.outline_min_vertices == result.outline_max_vertices
           ? std::string{}
           : "-" + std::to_string(result.outline_max_vertices)) +
      (extracted_outline_min == std::numeric_limits<int>::max()
           ? std::string{}
           : "; extracted_outline_vertices=" +
                 std::to_string(extracted_outline_min) +
                 (extracted_outline_min == extracted_outline_max
                      ? std::string{}
                      : "-" + std::to_string(extracted_outline_max))) +
      "; frames=" + std::to_string(original.samples.size()) +
      "; visible_outline_frames=" + std::to_string(applied_frames) +
      "; compacted_visible_outline_frames=" +
      std::to_string(compacted_frames) +
      "; compact_visible_outline_max_error=" +
      std::to_string(compact_max_error) +
      (uniform_target_vertices > 0
           ? "; uniform_visible_outline_target=" +
                 std::to_string(uniform_target_vertices) +
                 "; uniform_visible_outline_frames=" +
                 std::to_string(uniformed_frames) +
                 "; uniform_visible_outline_max_error=" +
                 std::to_string(uniform_max_error)
           : std::string{}) +
      "; frame_outline_error=not_evaluated";
  return result;
}

}  // namespace bbsolver
