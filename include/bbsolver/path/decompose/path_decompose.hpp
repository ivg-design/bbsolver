#pragma once

#include "bbsolver/domain.hpp"

#include <string>
#include <vector>

namespace bbsolver {

enum class PathChannel {
  Vert,
  InTangent,
  OutTangent,
};

struct PathChildSamples {
  PropertySamples samples;
  int vertex_index = 0;
  PathChannel channel = PathChannel::Vert;
};

struct PathDecomposeResult {
  bool is_shape_flat = false;
  bool stable_topology = false;
  bool closed = false;
  int vertex_count = 0;
  std::string warning;
  std::vector<PathChildSamples> children;
};

const char* PathChannelName(PathChannel channel);

// AE Path properties are serialized as:
//   [closed_flag, vertex_count, x, y, in_x, in_y, out_x, out_y, ...]
// Stable-topology paths decompose into 3 * vertex_count 2D-spatial children:
// vert, in_tangent, and out_tangent for each vertex.
PathDecomposeResult DecomposePathBundle(const PropertySamples& ps);

// Compatibility helper for callers that only need the child PropertySamples.
std::vector<PropertySamples> DecomposePathBundle(const SampleBundle& bundle);

// Reassemble child key curves into one flat shape_flat key list. Key times and
// interpolation metadata come from anchor_keys, normally the flat parent solve.
// Each child curve is evaluated at those times so K is bounded by the flat
// parent anchor count.
PropertyKeys ReassemblePathKeys(const PropertyInfo& parent_info,
                                const std::vector<PropertyKeys>& child_keys,
                                const std::vector<Key>& anchor_keys,
                                bool closed);

}  // namespace bbsolver
