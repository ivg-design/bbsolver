#pragma once

#include "nlohmann/json_fwd.hpp"

#include "bbsolver/domain.hpp"

namespace bbsolver {

CompInfo ParseCompInfoJson(const nlohmann::json& obj);

LayerXform ParseLayerXformJson(const nlohmann::json& obj);

PropertyInfo ParsePropertyInfoJson(const nlohmann::json& obj);

PropertySamples ParsePropertySamplesJson(const nlohmann::json& obj);

}  // namespace bbsolver
