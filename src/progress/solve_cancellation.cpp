#include "bbsolver/progress/solve_cancellation.hpp"
#include "bbsolver/domain.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <ratio>
#include <system_error>

#include "bbsolver/io/io_json.hpp"

namespace bbsolver {

bool CancelFileExists(const std::optional<std::filesystem::path>& cancel_file) {
  if (!cancel_file) {
    return false;
  }
  std::error_code ec;
  return std::filesystem::exists(*cancel_file, ec);
}

void MarkCancelledPartial(KeyBundle& keys) {
  for (auto& property : keys.property_results) {
    property.converged = false;
    property.notes = property.notes.empty() ? "cancelled" : property.notes + "; cancelled";
  }
}

int WriteCancelledPartial(const std::filesystem::path& output_path,
                          KeyBundle& keys,
                          std::chrono::steady_clock::time_point start) {
  MarkCancelledPartial(keys);
  const auto now = std::chrono::steady_clock::now();
  keys.solve_time_ms = std::chrono::duration<double, std::milli>(now - start).count();
  WriteKeyBundleJson(output_path, keys);
  return 5;
}

}  // namespace bbsolver
