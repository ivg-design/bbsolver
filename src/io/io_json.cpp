#include "bbsolver/io/io_json.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/domain.hpp"
#include "bbsolver/io/key_bundle_io.hpp"
#include "bbsolver/io/sample_bundle_io.hpp"

namespace bbsolver {

SampleBundle ReadSampleBundleJson(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Failed to open SampleBundle JSON: " +
                             path.string());
  }

  nlohmann::json root;
  input >> root;
  RequireSampleBundleJsonRoot(root);
  return ParseSampleBundleJson(root);
}

KeyBundle ReadKeyBundleJson(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Failed to open KeyBundle JSON: " +
                             path.string());
  }

  nlohmann::json root;
  input >> root;
  RequireKeyBundleJsonRoot(root);
  return ParseKeyBundleJson(root);
}

void WriteKeyBundleJson(const std::filesystem::path& path,
                        const KeyBundle& bundle) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("Failed to open KeyBundle JSON for writing: " +
                             path.string());
  }
  output << BuildKeyBundleJson(bundle).dump(2) << '\n';
}

}  // namespace bbsolver
