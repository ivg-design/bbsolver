#pragma once

namespace bbsolver {

inline constexpr int kSupportedBundleSchemaVersion = 1;
inline constexpr int kVerifyMismatchExitCode = 3;

void RequireSupportedBundleSchemaVersion(const char* bundle_kind,
                                         int schema_version);

}  // namespace bbsolver
