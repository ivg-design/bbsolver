#include "bbsolver/shape/shape_flat_topology.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
    std::abort();
  }
}

std::vector<double> ShapeFlatValues(double vertex_count, int actual_vertices) {
  std::vector<double> values;
  values.reserve(static_cast<std::size_t>(2 + actual_vertices * 6));
  values.push_back(1.0);
  values.push_back(vertex_count);
  for (int i = 0; i < actual_vertices * 6; ++i) {
    values.push_back(static_cast<double>(i));
  }
  return values;
}

void TestTooShortVectorsReturnMinusOne() {
  Require(bbsolver::ShapeFlatVertexCountFromValues({}) == -1,
          "empty vector must be invalid");
  Require(bbsolver::ShapeFlatVertexCountFromValues({1.0}) == -1,
          "single-value vector must be invalid");
}

void TestZeroAndNegativeCountsReturnMinusOne() {
  Require(bbsolver::ShapeFlatVertexCountFromValues({1.0, 0.0}) == -1,
          "zero vertex count must be invalid");
  Require(bbsolver::ShapeFlatVertexCountFromValues({1.0, -1.0}) == -1,
          "negative vertex count must be invalid");
}

void TestWrongExpectedSizeReturnsMinusOne() {
  Require(bbsolver::ShapeFlatVertexCountFromValues({1.0, 1.0, 0.0}) == -1,
          "one-vertex payload with wrong size must be invalid");
  Require(bbsolver::ShapeFlatVertexCountFromValues(
              ShapeFlatValues(2.0, 1)) == -1,
          "two-vertex count with one-vertex payload must be invalid");
}

void TestValidOneAndTwoVertexVectors() {
  Require(bbsolver::ShapeFlatVertexCountFromValues(
              ShapeFlatValues(1.0, 1)) == 1,
          "valid one-vertex payload must return one");
  Require(bbsolver::ShapeFlatVertexCountFromValues(
              ShapeFlatValues(2.0, 2)) == 2,
          "valid two-vertex payload must return two");
}

void TestRoundedCountBehavior() {
  Require(bbsolver::ShapeFlatVertexCountFromValues(
              ShapeFlatValues(1.49, 1)) == 1,
          "rounded count must drive expected size and return value");
  Require(bbsolver::ShapeFlatVertexCountFromValues(
              ShapeFlatValues(1.5, 2)) == 2,
          "half count must round before expected-size validation");
}

void TestShapeFlatVertexCountReturnsZeroForMalformedInput() {
  Require(bbsolver::ShapeFlatVertexCount({}) == 0,
          "empty shape-flat vector must return zero count");
  Require(bbsolver::ShapeFlatVertexCount({1.0}) == 0,
          "too-short shape-flat vector must return zero count");
  Require(bbsolver::ShapeFlatVertexCount({1.0, 0.0}) == 0,
          "zero vertex count must return zero count");
  Require(bbsolver::ShapeFlatVertexCount({1.0, -1.0}) == 0,
          "negative vertex count must return zero count");
  Require(bbsolver::ShapeFlatVertexCount({1.0, 1.0, 0.0}) == 0,
          "wrong payload size must return zero count");
}

void TestShapeFlatVertexCountReturnsValidCounts() {
  Require(bbsolver::ShapeFlatVertexCount(ShapeFlatValues(1.0, 1)) == 1,
          "valid one-vertex shape-flat vector must return one");
  Require(bbsolver::ShapeFlatVertexCount(ShapeFlatValues(2.0, 2)) == 2,
          "valid two-vertex shape-flat vector must return two");
  Require(bbsolver::ShapeFlatVertexCount(ShapeFlatValues(1.5, 2)) == 2,
          "vertex count must be rounded before validation");
}

void TestShapeFlatClosedBehavior() {
  Require(!bbsolver::ShapeFlatClosed({}),
          "empty shape-flat vector must not be closed");
  std::vector<double> open = ShapeFlatValues(1.0, 1);
  open[0] = 0.0;
  Require(!bbsolver::ShapeFlatClosed(open),
          "zero closed flag must not be closed");

  std::vector<double> closed = ShapeFlatValues(1.0, 1);
  closed[0] = 1.0;
  Require(bbsolver::ShapeFlatClosed(closed),
          "closed flag must be treated as closed");

  std::vector<double> rounded_open = ShapeFlatValues(1.0, 1);
  rounded_open[0] = 0.49;
  Require(!bbsolver::ShapeFlatClosed(rounded_open),
          "closed flag must use rounded integer value");

  std::vector<double> rounded_closed = ShapeFlatValues(1.0, 1);
  rounded_closed[0] = 0.5;
  Require(bbsolver::ShapeFlatClosed(rounded_closed),
          "rounded closed flag must be treated as closed");

  Require(bbsolver::ShapeFlatClosed({1.0, 99.0}),
          "closed flag must not depend on valid payload size");
}

void TestShapeFlatDistanceZero() {
  Require(bbsolver::ShapeFlatDistance({2.0, -3.0}, {2.0, -3.0}) == 0.0,
          "distance between identical points must be zero");
}

void TestShapeFlatDistanceThreeFourFive() {
  Require(bbsolver::ShapeFlatDistance({0.0, 0.0}, {3.0, 4.0}) == 5.0,
          "3-4-5 point distance must be five");
}

void TestShapeFlatDistanceSignInsensitive() {
  const bbsolver::ShapeFlatPoint a{3.0, 4.0};
  const bbsolver::ShapeFlatPoint b{-1.0, -2.0};
  Require(bbsolver::ShapeFlatDistance(a, b) ==
              bbsolver::ShapeFlatDistance(b, a),
          "distance must be sign-insensitive by point order");
}

void TestShapeFlatDistanceNegativeCoordinates() {
  Require(bbsolver::ShapeFlatDistance({-5.0, -6.0}, {-2.0, -2.0}) == 5.0,
          "negative coordinates must use euclidean distance");
}

void TestShapeFlatVertexOffsetMath() {
  Require(bbsolver::ShapeFlatVertexOffset(0) == 2,
          "first vertex offset must start after header");
  Require(bbsolver::ShapeFlatVertexOffset(1) == 8,
          "second vertex offset must advance by six values");
  Require(bbsolver::ShapeFlatVertexOffset(3) == 20,
          "later vertex offsets must use 2 + index * 6");
}

void TestShapeFlatComponentInRangeAndOutOfRange() {
  const std::vector<double> flat = {
      1.0, 2.0,
      10.0, 11.0, 12.0, 13.0, 14.0, 15.0,
      20.0, 21.0, 22.0, 23.0, 24.0, 25.0,
  };
  Require(bbsolver::ShapeFlatComponent(flat, 0, 0) == 10.0,
          "component lookup must read first vertex x");
  Require(bbsolver::ShapeFlatComponent(flat, 1, 5) == 25.0,
          "component lookup must read in-range later component");
  Require(bbsolver::ShapeFlatComponent(flat, 1, 6) == 0.0,
          "component lookup past vertex payload must return zero");
  Require(bbsolver::ShapeFlatComponent(flat, 4, 0) == 0.0,
          "component lookup past flat size must return zero");
}

void TestShapeFlatVerticesMalformedInputReturnsEmpty() {
  Require(bbsolver::ShapeFlatVertices({}).empty(),
          "empty flat vector must produce no vertices");
  Require(bbsolver::ShapeFlatVertices({1.0, 1.0, 0.0}).empty(),
          "malformed flat vector must produce no vertices");
}

void TestShapeFlatVerticesParsingOrder() {
  const std::vector<double> flat = {
      1.0, 2.0,
      10.0, 11.0, 12.0, 13.0, 14.0, 15.0,
      20.0, 21.0, 22.0, 23.0, 24.0, 25.0,
  };
  const std::vector<bbsolver::ShapeFlatVertex> vertices =
      bbsolver::ShapeFlatVertices(flat);
  Require(vertices.size() == 2, "valid flat vector must parse both vertices");
  Require(vertices[0].x == 10.0 && vertices[0].y == 11.0,
          "first vertex xy must parse in order");
  Require(vertices[0].in_x == 12.0 && vertices[0].in_y == 13.0,
          "first vertex in tangent must parse in order");
  Require(vertices[0].out_x == 14.0 && vertices[0].out_y == 15.0,
          "first vertex out tangent must parse in order");
  Require(vertices[1].x == 20.0 && vertices[1].y == 21.0,
          "second vertex xy must parse in order");
  Require(vertices[1].in_x == 22.0 && vertices[1].in_y == 23.0,
          "second vertex in tangent must parse in order");
  Require(vertices[1].out_x == 24.0 && vertices[1].out_y == 25.0,
          "second vertex out tangent must parse in order");
}

void TestShapeFlatFromVerticesSerialization() {
  const std::vector<bbsolver::ShapeFlatVertex> vertices = {
      {10.0, 11.0, 12.0, 13.0, 14.0, 15.0},
      {20.0, 21.0, 22.0, 23.0, 24.0, 25.0},
  };
  const std::vector<double> open =
      bbsolver::ShapeFlatFromVertices(vertices, false);
  const std::vector<double> closed =
      bbsolver::ShapeFlatFromVertices(vertices, true);
  Require(open == std::vector<double>({
                       0.0, 2.0,
                       10.0, 11.0, 12.0, 13.0, 14.0, 15.0,
                       20.0, 21.0, 22.0, 23.0, 24.0, 25.0,
                   }),
          "open serialization must write flag, count, then vertex fields");
  Require(closed == std::vector<double>({
                         1.0, 2.0,
                         10.0, 11.0, 12.0, 13.0, 14.0, 15.0,
                         20.0, 21.0, 22.0, 23.0, 24.0, 25.0,
                     }),
          "closed serialization must write closed flag and same vertex fields");
}

}  // namespace

int main() {
  TestTooShortVectorsReturnMinusOne();
  TestZeroAndNegativeCountsReturnMinusOne();
  TestWrongExpectedSizeReturnsMinusOne();
  TestValidOneAndTwoVertexVectors();
  TestRoundedCountBehavior();
  TestShapeFlatVertexCountReturnsZeroForMalformedInput();
  TestShapeFlatVertexCountReturnsValidCounts();
  TestShapeFlatClosedBehavior();
  TestShapeFlatDistanceZero();
  TestShapeFlatDistanceThreeFourFive();
  TestShapeFlatDistanceSignInsensitive();
  TestShapeFlatDistanceNegativeCoordinates();
  TestShapeFlatVertexOffsetMath();
  TestShapeFlatComponentInRangeAndOutOfRange();
  TestShapeFlatVerticesMalformedInputReturnsEmpty();
  TestShapeFlatVerticesParsingOrder();
  TestShapeFlatFromVerticesSerialization();
  std::cout << "[PASS] test_shape_flat_topology\n";
  return 0;
}
