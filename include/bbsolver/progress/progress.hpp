#pragma once

#include "bbsolver/domain.hpp"

#include <cstddef>
#include <mutex>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>

#include "bbsolver/dp/dp_placer.hpp"

namespace bbsolver {

class ProgressWriter {
 public:
  explicit ProgressWriter(int fd);

  void Emit(const nlohmann::json& event) const;

 private:
  int fd_ = -1;
  mutable std::mutex mutex_;
};

double SolveProgressForPropertyStage(std::size_t property_idx,
                                     std::size_t property_count,
                                     double stage_fraction);

std::string ProgressPropertyLabel(const PropertySamples& ps);

nlohmann::json PropertyProgressEvent(const char* event,
                                     const std::string& phase,
                                     std::size_t property_idx,
                                     std::size_t property_count,
                                     double stage_fraction,
                                     const PropertySamples& ps);

nlohmann::json PlacementProgressEvent(const char* event,
                                      const std::string& phase_prefix,
                                      std::size_t property_idx,
                                      std::size_t property_count,
                                      double stage_start,
                                      double stage_end,
                                      const PropertySamples& ps,
                                      const PlacementProgress& placement);

}  // namespace bbsolver
