#include "bbsolver/solve/solve_command_config.hpp"
#include "bbsolver/domain.hpp"
#include "bbsolver/app/cli_options.hpp"

#include <stdexcept>

#include "bbsolver/io/io_json.hpp"
#include "bbsolver/io/schema_contract.hpp"
#include "bbsolver/runtime/runtime_env.hpp"
#include "bbsolver/routing/solve_mode_policy.hpp"

namespace bbsolver {

void ApplySolveEnvironmentDefaults(SolveOptions& options) {
  options.decompose_paths =
      options.decompose_paths || EnvFlagEnabled("BBSOLVER_DECOMPOSE_PATHS");
  options.fit_canonical_paths =
      options.fit_canonical_paths ||
      EnvFlagEnabled("BBSOLVER_FIT_CANONICAL_PATHS");
  options.fit_replacement_paths =
      options.fit_replacement_paths ||
      EnvFlagEnabled("BBSOLVER_FIT_REPLACEMENT_PATHS");
  options.emit_landmark_subpaths =
      options.emit_landmark_subpaths ||
      EnvFlagEnabled("BBSOLVER_EMIT_LANDMARK_SUBPATHS");
}

void ApplySolveOptionsToSampleBundle(SampleBundle& samples,
                                     const SolveOptions& options,
                                     int resolved_parallel_jobs) {
  samples.config.tolerance = options.tolerance;
  samples.config.tolerance_screen_px = options.screen_px;
  samples.config.parallel_jobs = resolved_parallel_jobs;
  samples.config.verbose = options.verbose;
  if (options.solve_optimization_mode.has_value()) {
    samples.config.solve_optimization_mode = *options.solve_optimization_mode;
  }
  samples.config.solve_optimization_mode =
      NormalizeSolveOptimizationMode(samples.config.solve_optimization_mode);
  if (options.fit_canonical_paths) {
    samples.config.allow_path_spatial_fit = true;
  }
  if (options.fit_replacement_paths) {
    samples.config.allow_path_replacement_fit = true;
  }
}

KeyBundle MakeInitialKeyBundle(const SampleBundle& samples) {
  KeyBundle keys;
  keys.schema_version = samples.schema_version;
  keys.request_id = samples.request_id;
  keys.solver_version = kBbsolverVersion;
  keys.solver_build = "dev";
  return keys;
}

SolveCommandConfig ParseSolveCommandConfig(int argc, char** argv) {
  SolveCommandConfig command;
  command.input_path = argv[2];
  command.output_path = argv[3];
  command.options = ParseSolveOptions(argc, argv, 4);
  ApplySolveEnvironmentDefaults(command.options);

  if (!HasJsonSuffix(command.input_path)) {
    throw std::runtime_error(
        "bbsolver solve accepts SampleBundle JSON input only");
  }
  if (!HasJsonSuffix(command.output_path)) {
    throw std::runtime_error(
        "bbsolver solve writes KeyBundle JSON output only");
  }

  return command;
}

void LoadSolveCommandSamples(SolveCommandConfig& command) {
  command.samples = ReadSampleBundleJson(command.input_path);
  RequireSupportedBundleSchemaVersion("SampleBundle",
                                      command.samples.schema_version);
  if (command.samples.properties.empty()) {
    throw std::runtime_error("SampleBundle properties must not be empty");
  }
  command.resolved_parallel_jobs = ResolveParallelJobs(command.options.jobs);
  ApplySolveOptionsToSampleBundle(command.samples,
                                  command.options,
                                  command.resolved_parallel_jobs);
  command.keys = MakeInitialKeyBundle(command.samples);
}

SolveCommandConfig PrepareSolveCommand(int argc, char** argv) {
  SolveCommandConfig command = ParseSolveCommandConfig(argc, argv);
  LoadSolveCommandSamples(command);
  return command;
}

}  // namespace bbsolver
