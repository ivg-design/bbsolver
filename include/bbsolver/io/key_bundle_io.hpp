#pragma once

#include "bbsolver/domain.hpp"

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

void RequireKeyBundleJsonRoot(const nlohmann::json& root);

KeyBundle ParseKeyBundleJson(const nlohmann::json& root);

nlohmann::json BuildKeyBundleJson(const KeyBundle& bundle);

}  // namespace bbsolver
