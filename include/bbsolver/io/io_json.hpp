#pragma once

#include "bbsolver/domain.hpp"

#include <filesystem>

namespace bbsolver {

SampleBundle ReadSampleBundleJson(const std::filesystem::path& path);
KeyBundle ReadKeyBundleJson(const std::filesystem::path& path);
// Verifier-scoped relaxed reader. Performs structural validation but
// skips the per-key length-equality check against
// property_results.dimensions, so variable-topology shape_flat
// bundles whose keys legitimately carry shorter v[] arrays than the
// bbsm-declared max can reach the sample-aware verifier. Use only
// from the verify CLI; everywhere else use ReadKeyBundleJson, which
// retains the strict invariant for solve / apply / general IO.
KeyBundle ReadKeyBundleJsonForVerify(const std::filesystem::path& path);
void WriteKeyBundleJson(const std::filesystem::path& path, const KeyBundle& bundle);

}  // namespace bbsolver
