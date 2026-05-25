#pragma once

#include "bbsolver/domain.hpp"

#include "bbsolver/path/multimode/path_multimode_landmark_partition.hpp"
#include "bbsolver/path/multimode/path_multimode_solver.hpp"

#include <string>

namespace bbsolver {
namespace path_multimode {

std::string DiagnoseNonContiguousMaskChannels(
    const PropertySamples& reduced,
    const LandmarkPartitionResult& selected,
    const ShapeFlatLandmarkSubpathOptions& options,
    int max_gap);

}  // namespace path_multimode
}  // namespace bbsolver
