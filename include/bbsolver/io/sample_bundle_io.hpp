#pragma once

#include "nlohmann/json_fwd.hpp"

#include "bbsolver/domain.hpp"

namespace bbsolver {

void RequireSampleBundleJsonRoot(const nlohmann::json& root);

SampleBundle ParseSampleBundleJson(const nlohmann::json& root);

}  // namespace bbsolver
