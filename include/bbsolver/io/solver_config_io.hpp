#pragma once

#include "bbsolver/domain.hpp"

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

// Parse a SolverConfig from a JSON object. Defaults come from the
// default-constructed SolverConfig. Legacy camelCase aliases
// (motionSmoothTolerance, motionSmoothUseEase, motionSmoothBezierX1,...,
// motionPathSmoothingTolerance, shape_temporal_bezier_gate_ratio,
// path_spatial_fit, path_replacement_fit, shape_temporal_bezier) are honored
// for backward compatibility; when both
// the canonical snake_case key and its legacy alias are present the
// snake_case value wins, matching the pre-extraction io_json.cpp behavior.
//
// This is an internal solver header: callers always go through
// io_json.cpp's ReadSampleBundleJson / WriteKeyBundleJson; nothing outside
// the solver core links nlohmann/json directly.
SolverConfig ParseSolverConfigJson(const nlohmann::json& obj);

}  // namespace bbsolver
