#pragma once

#include "bbsolver/domain.hpp"

#include <chrono>
#include <filesystem>
#include <optional>

namespace bbsolver {

struct KeyBundle;

bool CancelFileExists(const std::optional<std::filesystem::path>& cancel_file);

void MarkCancelledPartial(KeyBundle& keys);

int WriteCancelledPartial(const std::filesystem::path& output_path,
                          KeyBundle& keys,
                          std::chrono::steady_clock::time_point start);

}  // namespace bbsolver
