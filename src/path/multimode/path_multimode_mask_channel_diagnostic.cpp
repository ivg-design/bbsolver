#include "bbsolver/path/multimode/path_multimode_mask_channel_diagnostic.hpp"
#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_landmark_temporal_solve.hpp"
#include "bbsolver/path/multimode/path_multimode_notes.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace bbsolver {
namespace path_multimode {
namespace {

constexpr int kMaskChannelDiagnosticSlots = 4;

}  // namespace

std::string DiagnoseNonContiguousMaskChannels(
    const PropertySamples& reduced,
    const LandmarkPartitionResult& selected,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap) {
  if (!selected.ok || selected.emissions.empty() ||
      selected.emissions.front().outlier_slots.empty()) {
    return "";
  }
  const int candidate_count =
      std::min(kMaskChannelDiagnosticSlots,
               static_cast<int>(selected.emissions.front().outlier_slots.size()));
  if (candidate_count < 2) {
    return "";
  }

  const int per_slot_budget =
      options.max_region_segment_checks > 0
          ? std::max(1, options.max_region_segment_checks /
                            std::max(1, candidate_count))
: options.max_region_segment_checks;
  std::vector<MaskChannelSlotPlan> slots;
  slots.reserve(static_cast<std::size_t>(candidate_count));
  int total_checks = 0;
  int singleton_keys = 0;
  for (int idx = 0; idx < candidate_count; ++idx) {
    const OutlierSlotScore& outlier =
        selected.emissions.front().outlier_slots[static_cast<std::size_t>(idx)];
    MaskChannelSlotPlan plan;
    plan.vertex = outlier.vertex;
    plan.score = outlier.score;

    const PropertySamples region_samples =
        BuildLandmarkRegionSamples(reduced, {plan.vertex, plan.vertex + 1});
    if (region_samples.samples.size() != reduced.samples.size()) {
      plan.status = "region_sample_failed";
      slots.push_back(std::move(plan));
      continue;
    }

    const LandmarkSubpathTemporalResult temporal =
        SolveLandmarkRegionTemporal(region_samples,
                                    options.region_tolerance,
                                    max_gap,
                                    per_slot_budget,
                                    options.cancel_fn);
    total_checks += temporal.segment_checks;
    if (temporal.notes == "cancelled") {
      return "cancelled";
    }
    if (!temporal.ok || temporal.keys.keys.empty()) {
      plan.status = temporal.notes.empty() ? "unknown": temporal.notes;
      slots.push_back(std::move(plan));
      continue;
    }

    plan.ok = true;
    plan.key_count = static_cast<int>(temporal.keys.keys.size());
    plan.signature = MaskChannelTemporalSignature(temporal.keys);
    if (plan.signature.empty()) {
      plan.ok = false;
      plan.status = "empty_signature";
      slots.push_back(std::move(plan));
      continue;
    }
    singleton_keys += plan.key_count;
    slots.push_back(std::move(plan));
  }

  std::vector<MaskChannelGroupPlan> groups;
  for (const MaskChannelSlotPlan& slot: slots) {
    if (!slot.ok) {
      continue;
    }
    auto existing =
        std::find_if(groups.begin(), groups.end(),
                     [&slot](const MaskChannelGroupPlan& group) {
                       return group.signature == slot.signature;
                     });
    if (existing == groups.end()) {
      MaskChannelGroupPlan group;
      group.signature = slot.signature;
      group.vertices.push_back(slot.vertex);
      group.key_count = slot.key_count;
      group.score = slot.score;
      groups.push_back(std::move(group));
    } else {
      existing->vertices.push_back(slot.vertex);
      existing->key_count = std::max(existing->key_count, slot.key_count);
      existing->score += slot.score;
    }
  }
  if (groups.size() < 2) {
    return "subpath_mask_channel_diagnostic=not_selected; reason=not_enough_temporal_groups" +
           std::string("; subpath_mask_channel_slots=") +
           FormatMaskChannelSlots(slots) +
           "; subpath_mask_channel_segment_checks=" +
           std::to_string(total_checks);
  }

  for (MaskChannelGroupPlan& group: groups) {
    std::sort(group.vertices.begin(), group.vertices.end());
  }
  std::sort(groups.begin(),
            groups.end(),
            [](const MaskChannelGroupPlan& a,
               const MaskChannelGroupPlan& b) {
              const int a_first =
                  a.vertices.empty() ? std::numeric_limits<int>::max()
: a.vertices.front();
              const int b_first =
                  b.vertices.empty() ? std::numeric_limits<int>::max()
: b.vertices.front();
              return a_first < b_first;
            });

  int grouped_keys = 0;
  for (const MaskChannelGroupPlan& group: groups) {
    grouped_keys += group.key_count;
  }
  const int full_key_count = TotalEmissionKeyCount(selected.emissions);
  const bool grouped_benefit =
      grouped_keys > 0 &&
      singleton_keys > 0 &&
      grouped_keys < singleton_keys;

  return "subpath_mask_channel_diagnostic=" +
         std::string(grouped_benefit ? "candidate": "not_selected") +
         (grouped_benefit ? ""
: "; reason=no_grouped_key_benefit") +
         "; subpath_mask_channel_slots=" +
         FormatMaskChannelSlots(slots) +
         "; subpath_mask_channel_groups=" +
         FormatMaskChannelGroups(groups) +
         "; subpath_mask_channel_grouped_keys=" +
         std::to_string(grouped_keys) +
         "; subpath_mask_channel_singleton_keys=" +
         std::to_string(singleton_keys) +
         "; subpath_mask_channel_full_key_count=" +
         std::to_string(full_key_count) +
         "; subpath_mask_channel_segment_checks=" +
         std::to_string(total_checks) +
         "; subpath_mask_channel_protocol=requires_extra_shape_channel_masks" +
         "; subpath_mask_channel_acceptance=requires_full_source_outline_validation";
}

}  // namespace path_multimode
}  // namespace bbsolver
