#pragma once

#include "nlohmann/json_fwd.hpp"

#include "bbsolver/domain.hpp"

namespace bbsolver {

bool HasSampleKeyTimingJsonFields(const nlohmann::json& obj);

KeyTiming ParseSampleKeyTimingJson(const nlohmann::json& obj);

Sample ParseSampleJson(const nlohmann::json& obj);

}  // namespace bbsolver
