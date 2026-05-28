#pragma once

#include "bbsolver/domain.hpp"

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

void RequireKeyBundleJsonRoot(const nlohmann::json& root);

// Verifier-scoped relaxed validation. Performs all the standard
// KeyBundle structural checks (object shape, t_sec numeric, v array
// non-empty, schema/version) but skips the per-key length-equality
// check against property_results.dimensions. Used only by the verify
// CLI so variable-topology shape_flat bundles — whose keys legitimately
// carry per-key v[] lengths shorter than the bbsm-declared max — can
// reach the sample-aware verifier instead of being rejected at parse
// time. The main strict path (RequireKeyBundleJsonRoot) is unchanged
// for solve / apply / general IO.
void RequireKeyBundleJsonRootForVerify(const nlohmann::json& root);

KeyBundle ParseKeyBundleJson(const nlohmann::json& root);

nlohmann::json BuildKeyBundleJson(const KeyBundle& bundle);

}  // namespace bbsolver
