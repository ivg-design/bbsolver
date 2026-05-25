#pragma once

#include "bbsolver/domain.hpp"

#include <filesystem>

#include "bbsolver/app/cli_options.hpp"

namespace bbsolver {

struct SolveCommandConfig {
  std::filesystem::path input_path;
  std::filesystem::path output_path;
  SolveOptions options;
  SampleBundle samples;
  KeyBundle keys;
  int resolved_parallel_jobs = 1;
};

void ApplySolveEnvironmentDefaults(SolveOptions& options);

void ApplySolveOptionsToSampleBundle(SampleBundle& samples,
                                     const SolveOptions& options,
                                     int resolved_parallel_jobs);

KeyBundle MakeInitialKeyBundle(const SampleBundle& samples);

SolveCommandConfig ParseSolveCommandConfig(int argc, char** argv);

void LoadSolveCommandSamples(SolveCommandConfig& command);

SolveCommandConfig PrepareSolveCommand(int argc, char** argv);

}  // namespace bbsolver
