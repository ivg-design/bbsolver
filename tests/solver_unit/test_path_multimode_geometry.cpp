#include "bbsolver/path/multimode/path_multimode_geometry.hpp"
#include "bbsolver/domain.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::vector<double> Flat(
    const std::vector<std::vector<double>>& vertices,
    bool closed = true) {
  std::vector<double> out;
  out.push_back(closed ? 1.0 : 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const std::vector<double>& vertex : vertices) {
    for (double value : vertex) {
      out.push_back(value);
    }
  }
  return out;
}

bbsolver::Sample Sample(double t, std::vector<double> value) {
  bbsolver::Sample sample;
  sample.t_sec = t;
  sample.v = std::move(value);
  return sample;
}

bbsolver::PropertySamples ShapeSamples(
    const std::vector<std::vector<double>>& values) {
  bbsolver::PropertySamples ps;
  ps.property.id = "unit/path_multimode_geometry";
  ps.property.kind = bbsolver::ValueKind::Custom;
  ps.property.units_label = "shape_flat";
  ps.property.dimensions =
      values.empty() ? 1 : static_cast<int>(values.front().size());
  ps.t_start_sec = 0.0;
  ps.t_end_sec = static_cast<double>(values.empty() ? 0 : values.size() - 1);
  for (std::size_t idx = 0; idx < values.size(); ++idx) {
    ps.samples.push_back(Sample(static_cast<double>(idx), values[idx]));
  }
  return ps;
}

std::vector<double> FourVertexFrame(double left_motion,
                                    double right_motion = 0.0) {
  return Flat({
      {0.0 + left_motion, 0.0, 1.0, 2.0, 3.0, 4.0},
      {10.0 + left_motion, 0.0, 5.0, 6.0, 7.0, 8.0},
      {20.0 + right_motion, 0.0, 9.0, 10.0, 11.0, 12.0},
      {30.0 + right_motion, 0.0, 13.0, 14.0, 15.0, 16.0},
  });
}

void TestShapeFlatClassificationAndTopology() {
  bbsolver::PropertySamples ps = ShapeSamples({FourVertexFrame(0.0)});
  Require(bbsolver::path_multimode::IsShapeFlatPath(ps),
          "shape_flat custom property must classify as shape path");
  ps.property.units_label = "other";
  Require(!bbsolver::path_multimode::IsShapeFlatPath(ps),
          "wrong units label must not classify as shape path");

  const std::vector<double> flat = FourVertexFrame(0.0);
  Require(bbsolver::path_multimode::ShapeFlatVertexCount(flat) == 4,
          "valid flat payload must report rounded vertex count");
  Require(bbsolver::path_multimode::ShapeFlatVertexCount({1.0, 4.0}) == 0,
          "malformed payload must report zero vertex count");
  Require(bbsolver::path_multimode::SameShapeFlatTopology(
              flat, FourVertexFrame(1.0)),
          "matching vertex count and closed flag must share topology");
  Require(!bbsolver::path_multimode::SameShapeFlatTopology(
              flat, Flat({{0, 0, 0, 0, 0, 0}}, true)),
          "different vertex counts must not share topology");
}

void TestSampleTimesAndMaxVertexCount() {
  bbsolver::PropertySamples a = ShapeSamples({
      Flat({{0, 0, 0, 0, 0, 0}}, true),
      FourVertexFrame(0.0),
  });
  bbsolver::PropertySamples b = a;
  Require(bbsolver::path_multimode::SameSampleTimes(a, b),
          "identical sample times must match");
  b.samples.back().t_sec += 2e-9;
  Require(!bbsolver::path_multimode::SameSampleTimes(a, b),
          "sample times outside epsilon must not match");
  Require(bbsolver::path_multimode::MaxShapeFlatVertexCount(a) == 4,
          "max vertex count must scan all samples");
}

void TestRegionPartitioning() {
  const std::vector<bbsolver::path_multimode::VertexRegion> uniform =
      bbsolver::path_multimode::BuildVertexRegions(5, 2);
  Require(uniform.size() == 2 &&
              uniform[0].first_vertex == 0 &&
              uniform[0].end_vertex == 2 &&
              uniform[1].first_vertex == 2 &&
              uniform[1].end_vertex == 5,
          "uniform region builder must split integer ranges deterministically");

  const bbsolver::PropertySamples motion = ShapeSamples({
      FourVertexFrame(0.0, 0.0),
      FourVertexFrame(10.0, 0.0),
  });
  const std::vector<bbsolver::path_multimode::VertexRegion> aware =
      bbsolver::path_multimode::BuildMotionAwareVertexRegions(motion, 4, 2);
  Require(aware.size() == 2 &&
              aware[0].first_vertex == 0 &&
              aware[0].end_vertex == 2 &&
              aware[1].first_vertex == 2 &&
              aware[1].end_vertex == 4,
          "motion-aware builder must choose the strongest moving boundary");
}

void TestRegionSliceInsertAndSampleBuild() {
  const std::vector<double> flat = FourVertexFrame(0.0);
  const bbsolver::path_multimode::VertexRegion middle{1, 3};
  const std::vector<double> region =
      bbsolver::path_multimode::ShapeFlatRegion(flat, middle);
  Require(bbsolver::path_multimode::ShapeFlatVertexCount(region) == 2,
          "region slice must reduce vertex count");
  Require(region[0] == 0.0,
          "partial region slice must be open");
  Require(region[4] == 0.0 && region[5] == 0.0,
          "partial region first incoming tangent must be zeroed");
  Require(region[12] == 0.0 && region[13] == 0.0,
          "partial region last outgoing tangent must be zeroed");

  std::vector<double> full = flat;
  std::vector<double> replacement = region;
  replacement[2] = 111.0;
  replacement[3] = 222.0;
  Require(bbsolver::path_multimode::InsertShapeFlatRegion(
              &full, middle, replacement),
          "matching replacement region must insert into full shape");
  Require(full[8] == 111.0 && full[9] == 222.0,
          "inserted region must write back to matching full vertex");

  const bbsolver::PropertySamples samples =
      ShapeSamples({flat, FourVertexFrame(1.0)});
  const bbsolver::PropertySamples region_samples =
      bbsolver::path_multimode::BuildLandmarkRegionSamples(samples, middle);
  Require(region_samples.samples.size() == 2,
          "region sample builder must preserve sample count");
  Require(region_samples.property.dimensions == 14,
          "region sample builder must update dimensions for two vertices");
  Require(region_samples.samples.front().v == region,
          "region sample builder must use the same region slicing rules");
}

}  // namespace

int main() {
  TestShapeFlatClassificationAndTopology();
  TestSampleTimesAndMaxVertexCount();
  TestRegionPartitioning();
  TestRegionSliceInsertAndSampleBuild();
  std::cout << "[PASS] test_path_multimode_geometry\n";
  return 0;
}
