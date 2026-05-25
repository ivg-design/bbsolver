#include "bbsolver/app/cli_options.hpp"

#include <ostream>
#include <stdexcept>
#include <filesystem>
#include <string>

namespace bbsolver {

void PrintUsage(std::ostream& out) {
  out << "Usage:\n"
      << "  bbsolver solve <in.bbsm.json> <out.bbky.json> [--tolerance 0.5] [--screen-px 0] [--jobs N] [--progress-fd N] [--diagnostics PATH] [--cancel-file PATH] [--decompose-paths] [--fit-canonical-paths] [--fit-replacement-paths] [--emit-landmark-subpaths] [--solve-mode full|temporal-only|vertex-only|motion-smooth|motion-path-smooth] [--verbose]\n"
      << "  bbsolver verify <bundle.bbky.json> <samples.bbsm.json>\n"
      << "  bbsolver dump <bundle.bbsm.json|bundle.bbky.json>\n"
      << "  bbsolver --version\n";
}

bool HasJsonSuffix(const std::filesystem::path& path) {
  const std::string name = path.filename().string();
  return name.size() >= 5 && name.substr(name.size() - 5) == ".json";
}

SolveOptions ParseSolveOptions(int argc, char** argv, int start_index) {
  SolveOptions options;
  for (int i = start_index; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--tolerance" && i + 1 < argc) {
      options.tolerance = std::stod(argv[++i]);
    } else if (arg == "--screen-px" && i + 1 < argc) {
      options.screen_px = std::stod(argv[++i]);
    } else if (arg == "--jobs" && i + 1 < argc) {
      options.jobs = std::stoi(argv[++i]);
    } else if (arg == "--progress-fd" && i + 1 < argc) {
      options.progress_fd = std::stoi(argv[++i]);
    } else if (arg == "--diagnostics" && i + 1 < argc) {
      options.diagnostics_file = std::filesystem::path(argv[++i]);
    } else if (arg == "--cancel-file" && i + 1 < argc) {
      options.cancel_file = std::filesystem::path(argv[++i]);
    } else if (arg == "--decompose-paths") {
      options.decompose_paths = true;
    } else if (arg == "--fit-canonical-paths") {
      options.fit_canonical_paths = true;
    } else if (arg == "--fit-replacement-paths") {
      options.fit_replacement_paths = true;
    } else if (arg == "--emit-landmark-subpaths") {
      options.emit_landmark_subpaths = true;
    } else if ((arg == "--solve-mode" || arg == "--solve-optimization-mode") &&
               i + 1 < argc) {
      options.solve_optimization_mode = argv[++i];
    } else if (arg == "--verbose") {
      options.verbose = true;
    } else {
      throw std::runtime_error("Unknown or incomplete solve option: " + arg);
    }
  }
  return options;
}

}  // namespace bbsolver
