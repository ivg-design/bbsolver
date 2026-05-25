#pragma once

#include "nlohmann/json_fwd.hpp"

namespace bbsolver {

void RequirePropertySamplesJson(const nlohmann::json& property_json);

}  // namespace bbsolver
