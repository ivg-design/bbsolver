// Integration test: every bbky.json in benchmarks/
// corpus/ must reach the canonical bbsolver verifier without being
// rejected at the strict per-key dimension gate.
//
// Six rows in this corpus are variable-topology shape_flat bundles whose per-key
// v[] length is shorter than the bbsm-declared dimensions (noodle ε=0.5/1.0/3.0,
// noodle experimental, blob v1, blob v6). Before v1.0.1 the canonical verify CLI
// rejected those rows with key_value_dimension_mismatch. This test pins the new
// behaviour so a future regression cannot ship: schema rejection is forbidden
// here, but legitimate ε-budget overshoot (e.g. the experimental noodle subpath
// row) is allowed — it is real measurement data, not a schema bug.
//
// Runs under ctest as a regular unit-test target. Working directory is the
// repo root and BBSOLVER_TEST_SOURCE_DIR is set by the CMake plumbing.

#include "bbsolver/verify/verify_dump_commands.hpp"
#include "bbsolver/io/schema_contract.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    std::abort();
  }
}

class CoutCapture {
 public:
  CoutCapture() { old_ = std::cout.rdbuf(buffer_.rdbuf()); }
  ~CoutCapture() { std::cout.rdbuf(old_); }
  std::string str() const { return buffer_.str(); }

 private:
  std::streambuf* old_ = nullptr;
  std::ostringstream buffer_;
};

std::filesystem::path PaperCorpusRoot() {
  const char* env = std::getenv("BBSOLVER_TEST_SOURCE_DIR");
  std::filesystem::path source_dir;
  if (env != nullptr && *env != '\0') {
    source_dir = env;
  } else {
    source_dir = std::filesystem::current_path();
  }
  return source_dir / "benchmarks" / "corpus";
}

// Reasons the verifier MUST NOT emit on this corpus. Both indicate that
// the verifier rejected the input at the per-key dim gate; the relaxed
// gate is supposed to keep these rows on the sample-aware verification
// path.
bool IsForbiddenReason(const std::string& reason) {
  return reason == "key_value_dimension_mismatch" ||
         reason == "invalid_shape_flat_key_dimensions" ||
         reason == "invalid_shape_flat_sample_metadata";
}

void RunVerifyAndAssertNoSchemaRejection(
    const std::filesystem::path& bbky,
    const std::filesystem::path& bbsm) {
  std::vector<std::string> args = {
      "bbsolver", "verify", bbky.string(), bbsm.string()};
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (std::string& arg: args) {
    argv.push_back(arg.data());
  }

  CoutCapture capture;
  // RunVerifyCommand returns kVerifyMismatchExitCode when any property
  // does not satisfy the ε budget, which is allowed here (the
  // experimental noodle subpath row legitimately exceeds budget).
  // What we forbid is *schema* rejection.
  const int rc = bbsolver::RunVerifyCommand(
      static_cast<int>(argv.size()), argv.data());
  Require(rc == 0 || rc == bbsolver::kVerifyMismatchExitCode,
          "verify exited with unexpected rc on " + bbky.string());

  nlohmann::json output;
  try {
    output = nlohmann::json::parse(capture.str());
  } catch (const std::exception& e) {
    Require(false,
            std::string("verify output is not valid JSON for ") +
                bbky.string() + ": " + e.what());
  }

  Require(output.contains("property_results"),
          "verify output missing property_results for " + bbky.string());
  Require(output.at("verified_properties").get<int>() >= 1,
          "verify did not reach sample-aware path for " + bbky.string());

  for (const auto& entry: output.at("property_results")) {
    if (!entry.contains("reason")) {
      continue;
    }
    const std::string reason = entry.at("reason").get<std::string>();
    Require(!IsForbiddenReason(reason),
            "canonical verifier rejected " + bbky.string() +
                " at schema gate with reason: " + reason);
  }
}

}  // namespace

int main() {
  const std::filesystem::path corpus = PaperCorpusRoot();
  Require(std::filesystem::is_directory(corpus),
          "benchmark corpus directory not found: " + corpus.string());

  int bbky_count = 0;
  for (const auto& request_entry:
       std::filesystem::directory_iterator(corpus)) {
    if (!request_entry.is_directory()) {
      continue;
    }
    const std::string request_name = request_entry.path().filename().string();
    if (request_name.rfind("req-", 0) != 0) {
      continue;
    }
    for (const auto& file_entry:
         std::filesystem::directory_iterator(request_entry.path())) {
      const std::filesystem::path& path = file_entry.path();
      const std::string name = path.filename().string();
      if (name.size() < 10 ||
          name.find(".bbky.json") != name.size() - 10) {
        continue;
      }
      std::filesystem::path bbsm = path;
      bbsm.replace_extension();   // drops.json
      bbsm.replace_extension();   // drops.bbky
      bbsm += ".bbsm.json";
      if (!std::filesystem::exists(bbsm)) {
        continue;
      }
      RunVerifyAndAssertNoSchemaRejection(path, bbsm);
      ++bbky_count;
    }
  }

  Require(bbky_count >= 11,
          "expected at least 11 benchmark-corpus bbky bundles, got " +
              std::to_string(bbky_count));

  std::cout << "benchmark corpus integration: " << bbky_count
            << " bbky bundles verified without schema rejection\n";
  return 0;
}
