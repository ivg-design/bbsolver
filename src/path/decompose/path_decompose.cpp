#include "bbsolver/path/decompose/path_decompose.hpp"
#include "bbsolver/domain.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <cstddef>
#include <vector>

#include "bbsolver/verify/verifier.hpp"

namespace bbsolver {
namespace {

constexpr int kPathHeaderScalars = 2;
constexpr int kScalarsPerVertex = 6;
struct DecodedPathSample {
  bool ok = false;
  bool closed = false;
  int vertex_count = 0;
};

bool IsShapeFlatPath(const PropertySamples& ps) {
  return ps.property.kind == ValueKind::Custom && ps.property.units_label == "shape_flat";
}

double ComponentOrZero(const std::vector<double>& values, std::size_t idx) {
  return idx < values.size() ? values[idx] : 0.0;
}

DecodedPathSample DecodePathHeader(const std::vector<double>& flat) {
  DecodedPathSample decoded;
  if (flat.size() < static_cast<std::size_t>(kPathHeaderScalars)) {
    return decoded;
  }
  decoded.closed = static_cast<int>(std::llround(flat[0])) != 0;
  decoded.vertex_count = static_cast<int>(std::llround(flat[1]));
  const int required = kPathHeaderScalars + decoded.vertex_count * kScalarsPerVertex;
  decoded.ok = decoded.vertex_count > 0 && required <= static_cast<int>(flat.size());
  return decoded;
}

std::size_t ChannelOffset(int vertex_index, PathChannel channel) {
  const std::size_t base =
      static_cast<std::size_t>(kPathHeaderScalars + vertex_index * kScalarsPerVertex);
  switch (channel) {
    case PathChannel::Vert:
      return base;
    case PathChannel::InTangent:
      return base + 2;
    case PathChannel::OutTangent:
      return base + 4;
  }
  return base;
}

ValueKind SpatialKindForPathChild() {
  return ValueKind::TwoD_Spatial;
}

PropertySamples MakeChild(const PropertySamples& ps,
                          int vertex_index,
                          PathChannel channel) {
  PropertySamples child;
  child.property = ps.property;
  child.property.id = ps.property.id + "/" + PathChannelName(channel) + "/" +
                      std::to_string(vertex_index);
  child.property.match_name = ps.property.match_name + "/" + PathChannelName(channel) + "/" +
                              std::to_string(vertex_index);
  child.property.display_name = ps.property.display_name + " " +
                                PathChannelName(channel) + " v" +
                                std::to_string(vertex_index);
  child.property.layer_path = ps.property.layer_path + "/" + PathChannelName(channel) + "/" +
                              std::to_string(vertex_index);
  child.property.kind = SpatialKindForPathChild();
  child.property.dimensions = 2;
  child.property.is_spatial = true;
  child.property.is_separated = false;
  child.property.units_label = "px";
  child.property.min_value.clear();
  child.property.max_value.clear();
  child.t_start_sec = ps.t_start_sec;
  child.t_end_sec = ps.t_end_sec;
  child.samples_per_frame = ps.samples_per_frame;
  child.layer_xform_at_start = ps.layer_xform_at_start;
  child.hash_of_expression = ps.hash_of_expression;
  child.samples.reserve(ps.samples.size());

  const std::size_t offset = ChannelOffset(vertex_index, channel);
  for (const Sample& sample : ps.samples) {
    Sample child_sample;
    child_sample.t_sec = sample.t_sec;
    child_sample.v = {
        ComponentOrZero(sample.v, offset),
        ComponentOrZero(sample.v, offset + 1),
    };
    child.samples.push_back(std::move(child_sample));
  }
  return child;
}

const PropertyKeys* FirstNonEmpty(const std::vector<PropertyKeys>& child_keys) {
  const auto it = std::find_if(
      child_keys.begin(),
      child_keys.end(),
      [](const PropertyKeys& child) { return !child.keys.empty(); });
  return it == child_keys.end() ? nullptr : &*it;
}

}  // namespace

const char* PathChannelName(PathChannel channel) {
  switch (channel) {
    case PathChannel::Vert:
      return "vert";
    case PathChannel::InTangent:
      return "in_tangent";
    case PathChannel::OutTangent:
      return "out_tangent";
  }
  return "vert";
}

PathDecomposeResult DecomposePathBundle(const PropertySamples& ps) {
  PathDecomposeResult result;
  result.is_shape_flat = IsShapeFlatPath(ps);
  if (!result.is_shape_flat) {
    return result;
  }
  if (ps.samples.empty()) {
    result.warning = "topology unstable: no samples";
    return result;
  }

  const DecodedPathSample first = DecodePathHeader(ps.samples.front().v);
  if (!first.ok) {
    result.warning = "topology unstable: malformed first shape_flat sample";
    return result;
  }
  result.closed = first.closed;
  result.vertex_count = first.vertex_count;

  for (const Sample& sample : ps.samples) {
    const DecodedPathSample decoded = DecodePathHeader(sample.v);
    if (!decoded.ok ||
        decoded.vertex_count != first.vertex_count ||
        decoded.closed != first.closed) {
      result.warning = "topology unstable";
      return result;
    }
  }

  result.stable_topology = true;
  result.children.reserve(static_cast<std::size_t>(first.vertex_count * 3));
  for (int vertex_index = 0; vertex_index < first.vertex_count; ++vertex_index) {
    for (PathChannel channel : {PathChannel::Vert, PathChannel::InTangent, PathChannel::OutTangent}) {
      PathChildSamples child;
      child.samples = MakeChild(ps, vertex_index, channel);
      child.vertex_index = vertex_index;
      child.channel = channel;
      result.children.push_back(std::move(child));
    }
  }
  return result;
}

std::vector<PropertySamples> DecomposePathBundle(const SampleBundle& bundle) {
  std::vector<PropertySamples> out;
  for (const PropertySamples& ps : bundle.properties) {
    PathDecomposeResult decomposed = DecomposePathBundle(ps);
    if (decomposed.stable_topology) {
      for (PathChildSamples& child : decomposed.children) {
        out.push_back(std::move(child.samples));
      }
    }
  }
  return out;
}

PropertyKeys ReassemblePathKeys(const PropertyInfo& parent_info,
                                const std::vector<PropertyKeys>& child_keys,
                                const std::vector<Key>& anchor_keys,
                                bool closed) {
  PropertyKeys out;
  out.property_id = parent_info.id;
  out.converged = true;
  if (child_keys.empty() || child_keys.size() % 3 != 0) {
    out.converged = false;
    out.notes = "path reassembly requires 3 child channels per vertex";
    return out;
  }
  if (anchor_keys.empty()) {
    out.converged = false;
    out.notes = "path reassembly requires flat anchor keys";
    return out;
  }

  const PropertyKeys* first_non_empty = FirstNonEmpty(child_keys);
  if (first_non_empty == nullptr) {
    out.converged = false;
    out.notes = "empty per-channel path keys";
    return out;
  }

  for (const PropertyKeys& child : child_keys) {
    out.converged = out.converged && child.converged && !child.keys.empty();
    out.max_err = std::max(out.max_err, child.max_err);
    out.max_err_screen_px = std::max(out.max_err_screen_px, child.max_err_screen_px);
  }

  const std::size_t vertex_count = child_keys.size() / 3;
  out.keys.reserve(anchor_keys.size());
  for (const Key& anchor : anchor_keys) {
    Key flat_key = anchor;
    flat_key.v.assign(kPathHeaderScalars + vertex_count * kScalarsPerVertex, 0.0);
    flat_key.v[0] = closed ? 1.0 : 0.0;
    flat_key.v[1] = static_cast<double>(vertex_count);

    for (std::size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
      for (std::size_t channel_index = 0; channel_index < 3; ++channel_index) {
        const std::size_t child_index = vertex_index * 3 + channel_index;
        const std::vector<double> value = EvalKeysAt(child_keys[child_index].keys, anchor.t_sec);
        const std::size_t offset =
            kPathHeaderScalars + vertex_index * kScalarsPerVertex + channel_index * 2;
        flat_key.v[offset] = ComponentOrZero(value, 0);
        flat_key.v[offset + 1] = ComponentOrZero(value, 1);
      }
    }
    out.keys.push_back(std::move(flat_key));
  }

  out.notes = "path_decomposed";
  return out;
}

}  // namespace bbsolver
