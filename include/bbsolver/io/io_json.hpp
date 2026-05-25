#pragma once

#include "bbsolver/domain.hpp"

#include <filesystem>

namespace bbsolver {

SampleBundle ReadSampleBundleJson(const std::filesystem::path& path);
KeyBundle ReadKeyBundleJson(const std::filesystem::path& path);
void WriteKeyBundleJson(const std::filesystem::path& path, const KeyBundle& bundle);

}  // namespace bbsolver
