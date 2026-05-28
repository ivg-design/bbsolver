#include "bbsolver/diagnostics/solver_diagnostics.hpp"

#include <fstream>
#include <ios>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <filesystem>
#include <memory>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

struct DiagnosticsWriter::State {
  explicit State(std::filesystem::path output_path)
: path(std::move(output_path)),
        stream(path, std::ios::out | std::ios::trunc) {}

  std::filesystem::path path;
  mutable std::mutex mutex;
  mutable std::ofstream stream;
};

DiagnosticsWriter::DiagnosticsWriter(std::shared_ptr<State> state)
: state_(std::move(state)) {}

DiagnosticsWriter DiagnosticsWriter::ToFile(
    const std::filesystem::path& path) {
  if (path.empty()) {
    throw std::runtime_error("Diagnostics path must not be empty");
  }
  const std::filesystem::path parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  auto state = std::make_shared<State>(path);
  if (!state->stream) {
    throw std::runtime_error("Failed to open diagnostics file: " +
                             path.string());
  }
  return DiagnosticsWriter(std::move(state));
}

bool DiagnosticsWriter::Enabled() const noexcept {
  return static_cast<bool>(state_);
}

void DiagnosticsWriter::Emit(const nlohmann::json& event) const {
  if (!state_) {
    return;
  }
  const std::lock_guard<std::mutex> lock(state_->mutex);
  state_->stream << event.dump() << '\n';
  state_->stream.flush();
  if (!state_->stream) {
    throw std::runtime_error("Failed to write diagnostics event: " +
                             state_->path.string());
  }
}

}  // namespace bbsolver
