#pragma once

#include <filesystem>
#include <memory>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

namespace bbsolver {

class DiagnosticsWriter {
 public:
  DiagnosticsWriter() = default;

  static DiagnosticsWriter ToFile(const std::filesystem::path& path);

  bool Enabled() const noexcept;
  void Emit(const nlohmann::json& event) const;

 private:
  struct State;

  explicit DiagnosticsWriter(std::shared_ptr<State> state);

  std::shared_ptr<State> state_;
};

}  // namespace bbsolver
