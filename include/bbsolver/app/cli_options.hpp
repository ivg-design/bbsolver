#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace bbsolver {

inline constexpr const char* kBbsolverVersion = "bbsolver 1.0.0";

struct SolveOptions {
  double tolerance = 0.5;
  double screen_px = 0.0;
  int jobs = 0;
  int progress_fd = -1;
  std::optional<std::filesystem::path> diagnostics_file;
  std::optional<std::filesystem::path> cancel_file;
  bool decompose_paths = false;
  bool fit_canonical_paths = false;
  bool fit_replacement_paths = false;
  bool emit_landmark_subpaths = false;
  std::optional<std::string> solve_optimization_mode;
  bool verbose = false;
};

void PrintUsage(std::ostream& out);

bool HasJsonSuffix(const std::filesystem::path& path);

SolveOptions ParseSolveOptions(int argc, char** argv, int start_index);

}  // namespace bbsolver
