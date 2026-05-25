#include "bbsolver/path/multimode/path_multimode_landmark_options.hpp"
#include "env_test_support.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
  if (condition) {
    return;
  }
  std::cerr << "test_path_multimode_landmark_options: " << message << "\n";
  std::exit(1);
}

struct LandmarkEnvGuard {
  bbsolver::test_support::ScopedEnv diagnostics{
      "BBSOLVER_LANDMARK_DIAGNOSTICS"};
  bbsolver::test_support::ScopedEnv protocol{"BBSOLVER_LANDMARK_PROTOCOL"};
  bbsolver::test_support::ScopedEnv probe{"BBSOLVER_VISIBLE_CHANNEL_PROBE"};
  bbsolver::test_support::ScopedEnv baseline{
      "BBSOLVER_VISIBLE_CHANNEL_BASELINE"};
};

void TestDiagnosticsEnvDeepAndFast() {
  {
    LandmarkEnvGuard env;
    env.diagnostics.Set("deep");
    bbsolver::ShapeFlatLandmarkSubpathOptions deep =
        bbsolver::path_multimode::NormalizeLandmarkSubpathOptions({});
    Require(deep.diagnose_dense_runs, "deep enables dense-run diagnostics");
    Require(deep.diagnose_segment_gaps, "deep enables segment-gap diagnostics");
    Require(deep.diagnose_outlier_slots, "deep enables outlier diagnostics");
    Require(deep.diagnose_mask_channels,
            "deep enables mask-channel diagnostics");
    Require(!deep.fast_summary_only, "deep disables fast summary");
  }

  {
    LandmarkEnvGuard env;
    env.diagnostics.Set("fast");
    bbsolver::ShapeFlatLandmarkSubpathOptions fast =
        bbsolver::path_multimode::NormalizeLandmarkSubpathOptions({});
    Require(!fast.diagnose_dense_runs, "fast disables dense-run diagnostics");
    Require(!fast.diagnose_segment_gaps,
            "fast disables segment-gap diagnostics");
    Require(!fast.diagnose_outlier_slots, "fast disables outlier diagnostics");
    Require(!fast.diagnose_mask_channels,
            "fast disables mask-channel diagnostics");
    Require(fast.fast_summary_only, "fast enables summary mode");
  }
}

void TestExplicitDeepIsNotOverriddenByFastEnv() {
  LandmarkEnvGuard env;
  env.diagnostics.Set("fast");
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.diagnose_segment_gaps = true;
  bbsolver::ShapeFlatLandmarkSubpathOptions normalized =
      bbsolver::path_multimode::NormalizeLandmarkSubpathOptions(options);
  Require(normalized.diagnose_segment_gaps,
          "explicit deep diagnostic survives fast env");
  Require(!normalized.fast_summary_only,
          "fast summary not forced when explicit deep diagnostics are set");
}

void TestMaskDiagnosticsEnableOutlierSlots() {
  LandmarkEnvGuard env;
  bbsolver::ShapeFlatLandmarkSubpathOptions options;
  options.diagnose_mask_channels = true;
  bbsolver::ShapeFlatLandmarkSubpathOptions normalized =
      bbsolver::path_multimode::NormalizeLandmarkSubpathOptions(options);
  Require(normalized.diagnose_mask_channels, "mask diagnostics preserved");
  Require(normalized.diagnose_outlier_slots,
          "mask diagnostics require outlier slots");
}

void TestProtocolAndVisibleProbeEnv() {
  LandmarkEnvGuard env;
  env.protocol.Set("shape_channel");
  env.probe.Set("yes");
  env.baseline.Set("42");
  bbsolver::ShapeFlatLandmarkSubpathOptions visible =
      bbsolver::path_multimode::NormalizeLandmarkSubpathOptions({});
  Require(visible.emit_visible_shape_channels,
          "shape_channel protocol enables visible channels");
  Require(visible.probe_visible_channels,
          "visible probe env enables probing");
  Require(visible.visible_baseline_keys == 42,
          "visible baseline parsed from env");

  env.protocol.Set("landmark");
  env.probe.Set("off");
  env.baseline.Set("not-a-number");
  bbsolver::ShapeFlatLandmarkSubpathOptions landmark =
      bbsolver::path_multimode::NormalizeLandmarkSubpathOptions(visible);
  Require(!landmark.emit_visible_shape_channels,
          "landmark protocol disables visible channels");
  Require(!landmark.probe_visible_channels, "off disables visible probing");
  Require(landmark.visible_baseline_keys == 0,
          "invalid visible baseline falls back to zero");
}

}  // namespace

int main() {
  TestDiagnosticsEnvDeepAndFast();
  TestExplicitDeepIsNotOverriddenByFastEnv();
  TestMaskDiagnosticsEnableOutlierSlots();
  TestProtocolAndVisibleProbeEnv();
  return 0;
}
