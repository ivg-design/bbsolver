#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
#include <cstddef>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Point {
  double x = 0.0;
  double y = 0.0;
};

void PushUnique(std::vector<Point>& points, Point p) {
  if (!points.empty()) {
    const double dx = points.back().x - p.x;
    const double dy = points.back().y - p.y;
    if (std::sqrt(dx * dx + dy * dy) < 1e-9) {
      return;
    }
  }
  points.push_back(p);
}

std::vector<double> ShapeFlat(bool closed, const std::vector<Point>& vertices) {
  std::vector<double> out;
  out.push_back(closed ? 1.0: 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (const Point& vertex: vertices) {
    out.push_back(vertex.x);
    out.push_back(vertex.y);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
    out.push_back(0.0);
  }
  return out;
}

std::vector<double> ShapeFlatWithSmoothTangents(bool closed,
                                                const std::vector<Point>& vertices) {
  std::vector<double> out;
  out.push_back(closed ? 1.0: 0.0);
  out.push_back(static_cast<double>(vertices.size()));
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    Point in_tangent{0.0, 0.0};
    Point out_tangent{0.0, 0.0};
    if (vertices.size() > 1) {
      const bool has_prev = closed || i > 0;
      const bool has_next = closed || i + 1 < vertices.size();
      if (has_prev && has_next) {
        const Point prev = vertices[(i + vertices.size() - 1) % vertices.size()];
        const Point next = vertices[(i + 1) % vertices.size()];
        in_tangent = {(prev.x - next.x) / 6.0, (prev.y - next.y) / 6.0};
        out_tangent = {(next.x - prev.x) / 6.0, (next.y - prev.y) / 6.0};
      }
    }
    out.push_back(vertices[i].x);
    out.push_back(vertices[i].y);
    out.push_back(in_tangent.x);
    out.push_back(in_tangent.y);
    out.push_back(out_tangent.x);
    out.push_back(out_tangent.y);
  }
  return out;
}

Point VertexAt(const std::vector<double>& flat, int index) {
  const std::size_t offset = 2 + static_cast<std::size_t>(index) * 6;
  return {flat[offset], flat[offset + 1]};
}

bool HasNonZeroTangent(const std::vector<double>& flat) {
  const int n = static_cast<int>(std::llround(flat[1]));
  for (int i = 0; i < n; ++i) {
    const std::size_t offset = 2 + static_cast<std::size_t>(i) * 6;
    const double in_x = flat[offset + 2];
    const double in_y = flat[offset + 3];
    const double out_x = flat[offset + 4];
    const double out_y = flat[offset + 5];
    if (std::hypot(in_x, in_y) > 1e-6 || std::hypot(out_x, out_y) > 1e-6) {
      return true;
    }
  }
  return false;
}

bool AllTangentsNearZero(const std::vector<double>& flat, double eps = 1e-6) {
  const int n = static_cast<int>(std::llround(flat[1]));
  for (int i = 0; i < n; ++i) {
    const std::size_t offset = 2 + static_cast<std::size_t>(i) * 6;
    if (std::hypot(flat[offset + 2], flat[offset + 3]) > eps ||
        std::hypot(flat[offset + 4], flat[offset + 5]) > eps) {
      return false;
    }
  }
  return true;
}

bool TangentsNearZeroAt(const std::vector<double>& flat, int index, double eps = 1e-6) {
  const std::size_t offset = 2 + static_cast<std::size_t>(index) * 6;
  return std::hypot(flat[offset + 2], flat[offset + 3]) <= eps &&
         std::hypot(flat[offset + 4], flat[offset + 5]) <= eps;
}

bool Near(Point a, Point b, double eps = 1e-6) {
  return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps;
}

bool NearDouble(double a, double b, double eps = 1e-9) {
  return std::abs(a - b) <= eps;
}

bool FractionsInSeamOrder(const std::vector<double>& fractions, bool closed) {
  if (fractions.empty()) {
    return false;
  }
  double previous = -1.0;
  for (double fraction: fractions) {
    if (!std::isfinite(fraction)) {
      return false;
    }
    if (closed) {
      if (fraction < 0.0 || fraction >= 1.0) {
        return false;
      }
    } else if (fraction < 0.0 || fraction > 1.0) {
      return false;
    }
    if (fraction <= previous) {
      return false;
    }
    previous = fraction;
  }
  return true;
}

std::vector<Point> RoundedRectPolyline() {
  const double w = 160.0;
  const double h = 90.0;
  const double r = 22.0;
  const int arc_steps = 10;
  std::vector<Point> points;

  auto add_arc = [&](double cx, double cy, double a0, double a1) {
    for (int i = 0; i <= arc_steps; ++i) {
      const double u = static_cast<double>(i) / arc_steps;
      const double a = a0 + (a1 - a0) * u;
      PushUnique(points, {cx + std::cos(a) * r, cy + std::sin(a) * r});
    }
  };

  PushUnique(points, {r, 0.0});
  PushUnique(points, {w * 0.5, 0.0});
  PushUnique(points, {w - r, 0.0});
  add_arc(w - r, r, -0.5 * kPi, 0.0);
  PushUnique(points, {w, h * 0.5});
  PushUnique(points, {w, h - r});
  add_arc(w - r, h - r, 0.0, 0.5 * kPi);
  PushUnique(points, {w * 0.5, h});
  PushUnique(points, {r, h});
  add_arc(r, h - r, 0.5 * kPi, kPi);
  PushUnique(points, {0.0, h * 0.5});
  PushUnique(points, {0.0, r});
  add_arc(r, r, kPi, 1.5 * kPi);
  return points;
}

std::vector<Point> OversampledRectangle() {
  return {
      {0.0, 0.0},   {25.0, 0.0},  {50.0, 0.0},  {75.0, 0.0},
      {100.0, 0.0}, {100.0, 25.0}, {100.0, 50.0}, {100.0, 75.0},
      {100.0, 100.0}, {75.0, 100.0}, {50.0, 100.0}, {25.0, 100.0},
      {0.0, 100.0}, {0.0, 75.0}, {0.0, 50.0}, {0.0, 25.0},
  };
}

std::vector<Point> SmoothOvalPolyline() {
  std::vector<Point> points;
  const int steps = 72;
  const double cx = 100.0;
  const double cy = 80.0;
  const double rx = 70.0;
  const double ry = 34.0;
  for (int i = 0; i < steps; ++i) {
    const double a = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(steps);
    points.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
  }
  return points;
}

std::vector<Point> DeformedSmoothOvalPolyline(double rx, double ry, double wobble) {
  std::vector<Point> points;
  const int steps = 96;
  const double cx = 120.0;
  const double cy = 90.0;
  for (int i = 0; i < steps; ++i) {
    const double a = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(steps);
    const double r = 1.0 + wobble * std::sin(3.0 * a);
    points.push_back({cx + std::cos(a) * rx * r,
                      cy + std::sin(a) * ry * (1.0 - 0.5 * wobble * std::cos(2.0 * a))});
  }
  return points;
}

std::vector<Point> OpenCurvedWristPolyline(int wrist_step = 20) {
  std::vector<Point> points;
  const int steps = 40;
  for (int i = 0; i <= steps; ++i) {
    const double x = 200.0 * static_cast<double>(i) / static_cast<double>(steps);
    double y = 25.0 * std::sin(kPi * x / 200.0);
    if (i == wrist_step) {
      y += 1.7;  // about 0.6 rad turn: subtle wrist-like geometric feature.
    }
    points.push_back({x, y});
  }
  return points;
}

std::vector<Point> OpenCurvedWristPolylineWithBumps(
    int persistent_wrist_step,
    const std::vector<int>& transient_steps) {
  std::vector<Point> points;
  const int steps = 40;
  for (int i = 0; i <= steps; ++i) {
    const double x = 200.0 * static_cast<double>(i) / static_cast<double>(steps);
    double y = 25.0 * std::sin(kPi * x / 200.0);
    if (i == persistent_wrist_step ||
        std::find(transient_steps.begin(), transient_steps.end(), i) !=
            transient_steps.end()) {
      y += 1.7;
    }
    points.push_back({x, y});
  }
  return points;
}

std::vector<Point> OpenZeroTangentVPolyline() {
  std::vector<Point> points;
  for (int i = 0; i <= 10; ++i) {
    const double x = 20.0 * static_cast<double>(i);
    const double y = i <= 5
        ? 7.0 * static_cast<double>(i)
: 7.0 * static_cast<double>(10 - i);
    points.push_back({x, y});
  }
  return points;
}

std::vector<Point> RegularLimbSelfOverlapPolyline() {
  return {
      {-39.9317779541016, 30.0907440185547},
      {-48.4073791503906, 12.5189971923828},
      {-49.5134124755859, -6.95866394042969},
      {-43.0814666748047, -25.3769226074219},
      {-30.0907592773438, -39.9317779541016},
      {-12.5189971923828, -48.4073791503906},
      {6.95866394042969, -49.5134124755859},
      {25.3769073486328, -43.0814666748047},
      {39.9317779541016, -30.0907592773438},
      {67.5268096923828, 11.7216949462891},
      {95.1218109130859, 53.5341339111328},
      {122.716827392578, 95.3465728759766},
      {150.311859130859, 137.159057617188},
      {152.506774902344, 140.413177490234},
      {154.349548339844, 143.878936767578},
      {155.819946289062, 147.518310546875},
      {156.901885986328, 151.291442871094},
      {157.58349609375, 155.157012939453},
      {157.857299804688, 159.072631835938},
      {153.105773925781, 209.163208007812},
      {148.354248046875, 259.253753662109},
      {143.602722167969, 309.344268798828},
      {138.851196289062, 359.434844970703},
      {135.101776123047, 359.500305175781},
      {131.352355957031, 359.565734863281},
      {127.602920532227, 359.631195068359},
      {123.853485107422, 359.696655273438},
      {120.104049682617, 359.762084960938},
      {116.354629516602, 359.827545166016},
      {112.605209350586, 359.892944335938},
      {108.855773925781, 359.958435058594},
      {102.359008789062, 310.064208984375},
      {95.8622436523438, 260.169982910156},
      {89.365478515625, 210.275787353516},
      {82.8687133789062, 160.381561279297},
      {83.1425323486328, 164.297210693359},
      {83.8241119384766, 168.162780761719},
      {84.9060668945312, 171.935913085938},
      {86.37646484375, 175.575286865234},
      {88.21923828125, 179.041046142578},
      {90.4141845703125, 182.295166015625},
      {57.8276824951172, 144.244079589844},
      {25.2411956787109, 106.192962646484},
      {-7.34529113769531, 68.1418609619141},
      {-39.9317779541016, 30.0907440185547},
  };
}

std::vector<double> UniformClosedFractions(int count) {
  std::vector<double> fractions;
  fractions.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    fractions.push_back(static_cast<double>(i) / static_cast<double>(count));
  }
  return fractions;
}

std::vector<double> UniformOpenFractions(int count) {
  std::vector<double> fractions;
  fractions.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    fractions.push_back(static_cast<double>(i) / static_cast<double>(count - 1));
  }
  return fractions;
}

std::vector<double> MergeFeatureFractions(
    std::vector<double> fractions,
    const std::vector<bbsolver::PathFeatureAnchor>& anchors) {
  for (const bbsolver::PathFeatureAnchor& anchor: anchors) {
    const auto existing = std::find_if(
        fractions.begin(), fractions.end(), [&](double fraction) {
          return std::abs(fraction - anchor.outline_fraction) <= 1e-6;
        });
    if (existing == fractions.end()) {
      fractions.push_back(anchor.outline_fraction);
    }
  }
  std::sort(fractions.begin(), fractions.end());
  fractions.erase(std::unique(fractions.begin(), fractions.end(), [](double a, double b) {
                    return std::abs(a - b) <= 1e-6;
                  }),
                  fractions.end());
  return fractions;
}

}  // namespace

int main() {
  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 2.5;

    const std::vector<double> source = ShapeFlat(true, RoundedRectPolyline());
    const bbsolver::PathFrameFitResult result = bbsolver::FitShapeFlatFrame(source, options);

    assert(result.ok);
    assert(result.applied);
    assert(result.closed);
    assert(result.source_vertex_count > 30);
    assert(result.fitted_vertex_count < result.source_vertex_count);
    assert(result.fitted_vertex_count <= 18);
    assert(result.max_outline_error <= options.outline_tolerance);
    assert(std::llround(result.fitted[0]) == 1);
    assert(std::llround(result.fitted[1]) == result.fitted_vertex_count);
    assert(result.outline_fractions.size() == static_cast<std::size_t>(result.fitted_vertex_count));
    assert(FractionsInSeamOrder(result.outline_fractions, true));
    assert(Near(VertexAt(result.fitted, 0), VertexAt(source, 0)));
  }

  {
    bbsolver::PathFrameFitOptions auto_options;
    auto_options.outline_tolerance = 2.5;

    const std::vector<double> source = ShapeFlat(true, RoundedRectPolyline());
    const bbsolver::PathFrameFitResult automatic = bbsolver::FitShapeFlatFrame(source, auto_options);
    assert(automatic.ok);
    assert(automatic.applied);
    assert(automatic.max_outline_error <= auto_options.outline_tolerance);

    bbsolver::PathFrameFitOptions target_options = auto_options;
    target_options.target_vertex_count = automatic.fitted_vertex_count + 6;
    const bbsolver::PathFrameFitResult targeted = bbsolver::FitShapeFlatFrame(source, target_options);

    assert(targeted.ok);
    assert(targeted.applied);
    assert(targeted.target_met);
    assert(targeted.fitted_vertex_count == target_options.target_vertex_count);
    assert(targeted.max_outline_error <= auto_options.outline_tolerance);
    assert(targeted.max_outline_error <= automatic.max_outline_error + 1e-6);
    assert(targeted.outline_fractions.size() == static_cast<std::size_t>(targeted.fitted_vertex_count));
    assert(FractionsInSeamOrder(targeted.outline_fractions, true));
    assert(Near(VertexAt(targeted.fitted, 0), VertexAt(source, 0)));

    const bbsolver::PathFrameFitResult replay =
        bbsolver::FitShapeFlatFrameAtFractions(source, targeted.outline_fractions, auto_options);
    assert(replay.ok);
    assert(replay.applied);
    assert(replay.fitted_vertex_count == targeted.fitted_vertex_count);
    assert(replay.max_outline_error <= auto_options.outline_tolerance);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.01;

    const std::vector<double> source = ShapeFlat(true, OversampledRectangle());
    const bbsolver::PathFrameFitResult result = bbsolver::FitShapeFlatFrame(source, options);

    assert(result.ok);
    assert(result.applied);
    assert(result.closed);
    assert(result.source_vertex_count == 16);
    assert(result.fitted_vertex_count == 4);
    assert(result.max_outline_error <= options.outline_tolerance);
    assert(result.outline_fractions.size() == 4);
    assert(NearDouble(result.outline_fractions[0], 0.0));
    assert(NearDouble(result.outline_fractions[1], 0.25));
    assert(NearDouble(result.outline_fractions[2], 0.5));
    assert(NearDouble(result.outline_fractions[3], 0.75));
    assert(Near(VertexAt(result.fitted, 0), {0.0, 0.0}));
    assert(Near(VertexAt(result.fitted, 1), {100.0, 0.0}));
    assert(Near(VertexAt(result.fitted, 2), {100.0, 100.0}));
    assert(Near(VertexAt(result.fitted, 3), {0.0, 100.0}));

    const bbsolver::PathFrameFitResult replay =
        bbsolver::FitShapeFlatFrameAtFractions(source, result.outline_fractions, options);
    assert(replay.ok);
    assert(replay.applied);
    assert(replay.fitted_vertex_count == 4);
    assert(replay.max_outline_error <= options.outline_tolerance);
    assert(AllTangentsNearZero(replay.fitted));
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.01;
    options.target_vertex_count = 8;

    const std::vector<double> source = ShapeFlat(true, OversampledRectangle());
    const bbsolver::PathFrameFitResult result = bbsolver::FitShapeFlatFrame(source, options);

    assert(result.ok);
    assert(result.applied);
    assert(result.target_vertex_count == 8);
    assert(result.target_met);
    assert(result.source_vertex_count == 16);
    assert(result.fitted_vertex_count == 8);
    assert(result.max_outline_error <= options.outline_tolerance);
    assert(Near(VertexAt(result.fitted, 0), {0.0, 0.0}));
    assert(result.outline_fractions.size() == static_cast<std::size_t>(result.fitted_vertex_count));
    assert(FractionsInSeamOrder(result.outline_fractions, true));

    bool has_top_right = false;
    bool has_bottom_right = false;
    bool has_bottom_left = false;
    for (int i = 0; i < result.fitted_vertex_count; ++i) {
      const Point v = VertexAt(result.fitted, i);
      has_top_right = has_top_right || Near(v, {100.0, 0.0});
      has_bottom_right = has_bottom_right || Near(v, {100.0, 100.0});
      has_bottom_left = has_bottom_left || Near(v, {0.0, 100.0});
    }
    assert(has_top_right);
    assert(has_bottom_right);
    assert(has_bottom_left);

    const bbsolver::PathFrameFitResult replay =
        bbsolver::FitShapeFlatFrameAtFractions(source, result.outline_fractions, options);
    assert(replay.ok);
    assert(replay.applied);
    assert(replay.fitted_vertex_count == result.fitted_vertex_count);
    assert(replay.max_outline_error <= options.outline_tolerance);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.01;
    options.target_vertex_count = 3;

    const std::vector<double> source = ShapeFlat(true, OversampledRectangle());
    const bbsolver::PathFrameFitResult result = bbsolver::FitShapeFlatFrame(source, options);

    assert(result.ok);
    assert(result.applied);
    assert(!result.target_met);
    assert(result.fitted_vertex_count == 4);
    assert(result.max_outline_error <= options.outline_tolerance);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.5;

    const std::vector<double> source = ShapeFlat(true, SmoothOvalPolyline());
    const bbsolver::PathFrameFitResult result = bbsolver::FitShapeFlatFrame(source, options);

    assert(result.ok);
    assert(result.applied);
    assert(result.closed);
    assert(result.source_vertex_count == 72);
    assert(result.fitted_vertex_count <= 12);
    assert(result.max_outline_error <= options.outline_tolerance);
    assert(HasNonZeroTangent(result.fitted));
    assert(result.outline_fractions.size() == static_cast<std::size_t>(result.fitted_vertex_count));
    assert(FractionsInSeamOrder(result.outline_fractions, true));
    assert(Near(VertexAt(result.fitted, 0), VertexAt(source, 0)));
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.01;

    const std::vector<double> source = ShapeFlat(true, SmoothOvalPolyline());
    const bbsolver::PathFrameFitResult result =
        bbsolver::FitShapeFlatFrameAtFractions(source, UniformClosedFractions(8), options);

    assert(result.ok);
    assert(result.applied);
    assert(result.closed);
    assert(result.target_met);
    assert(result.fitted_vertex_count == 8);
    assert(result.max_outline_error <= options.outline_tolerance);
    assert(HasNonZeroTangent(result.fitted));
    assert(result.outline_fractions.size() == 8);
    assert(FractionsInSeamOrder(result.outline_fractions, true));
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.5;

    const std::vector<double> source = ShapeFlat(true, SmoothOvalPolyline());
    const bbsolver::PathFrameFitResult automatic = bbsolver::FitShapeFlatFrame(source, options);
    assert(automatic.ok);
    assert(automatic.applied);

    bbsolver::PathFrameFitOptions target_options = options;
    target_options.target_vertex_count = automatic.fitted_vertex_count + 6;
    const bbsolver::PathFrameFitResult targeted =
        bbsolver::FitShapeFlatFrame(source, target_options);

    assert(targeted.ok);
    assert(targeted.applied);
    assert(targeted.target_met);
    assert(targeted.fitted_vertex_count == target_options.target_vertex_count);
    assert(targeted.max_outline_error <= options.outline_tolerance);
    assert(targeted.max_outline_error <= automatic.max_outline_error + 1e-6);
    assert(HasNonZeroTangent(targeted.fitted));
    assert(targeted.outline_fractions.size() == static_cast<std::size_t>(targeted.fitted_vertex_count));
    assert(FractionsInSeamOrder(targeted.outline_fractions, true));

    const bbsolver::PathFrameFitResult replay =
        bbsolver::FitShapeFlatFrameAtFractions(source, targeted.outline_fractions, options);
    assert(replay.ok);
    assert(replay.applied);
    assert(replay.fitted_vertex_count == targeted.fitted_vertex_count);
    assert(replay.max_outline_error <= options.outline_tolerance);
    assert(HasNonZeroTangent(replay.fitted));
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.75;

    const std::vector<double> frame_a = ShapeFlat(true, DeformedSmoothOvalPolyline(76.0, 36.0, 0.05));
    const std::vector<double> frame_b = ShapeFlat(true, DeformedSmoothOvalPolyline(74.0, 38.0, 0.06));

    const bbsolver::PathFrameFitResult seed = bbsolver::FitShapeFlatFrame(frame_a, options);
    assert(seed.ok);
    assert(seed.applied);
    assert(seed.outline_fractions.size() == static_cast<std::size_t>(seed.fitted_vertex_count));
    assert(FractionsInSeamOrder(seed.outline_fractions, true));

    const bbsolver::PathFrameFitResult follow =
        bbsolver::FitShapeFlatFrameAtFractions(frame_b, seed.outline_fractions, options);
    assert(follow.ok);
    assert(follow.applied);
    assert(follow.target_met);
    assert(follow.fitted_vertex_count == seed.fitted_vertex_count);
    assert(follow.outline_fractions.size() == seed.outline_fractions.size());
    assert(follow.max_outline_error <= options.outline_tolerance);
    assert(HasNonZeroTangent(follow.fitted));
    for (std::size_t i = 0; i < seed.outline_fractions.size(); ++i) {
      assert(NearDouble(follow.outline_fractions[i], seed.outline_fractions[i]));
    }
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.5;

    const std::vector<double> fractions = UniformClosedFractions(12);
    const std::vector<double> frame_a = ShapeFlat(true, DeformedSmoothOvalPolyline(76.0, 36.0, 0.05));
    const std::vector<double> frame_b = ShapeFlat(true, DeformedSmoothOvalPolyline(68.0, 44.0, 0.08));

    const bbsolver::PathFrameFitResult fit_a =
        bbsolver::FitShapeFlatFrameAtFractions(frame_a, fractions, options);
    const bbsolver::PathFrameFitResult fit_b =
        bbsolver::FitShapeFlatFrameAtFractions(frame_b, fractions, options);

    assert(fit_a.ok);
    assert(fit_b.ok);
    assert(fit_a.applied);
    assert(fit_b.applied);
    assert(fit_a.closed);
    assert(fit_b.closed);
    assert(fit_a.target_met);
    assert(fit_b.target_met);
    assert(fit_a.fitted_vertex_count == static_cast<int>(fractions.size()));
    assert(fit_b.fitted_vertex_count == static_cast<int>(fractions.size()));
    assert(fit_a.fitted_vertex_count == fit_b.fitted_vertex_count);
    assert(fit_a.max_outline_error <= options.outline_tolerance);
    assert(fit_b.max_outline_error <= options.outline_tolerance);
    assert(HasNonZeroTangent(fit_a.fitted));
    assert(HasNonZeroTangent(fit_b.fitted));
    assert(fit_a.outline_fractions.size() == fractions.size());
    assert(fit_b.outline_fractions.size() == fractions.size());
    for (std::size_t i = 0; i < fractions.size(); ++i) {
      assert(NearDouble(fit_a.outline_fractions[i], fractions[i]));
      assert(NearDouble(fit_b.outline_fractions[i], fractions[i]));
    }
    assert(Near(VertexAt(fit_a.fitted, 0), VertexAt(frame_a, 0)));
    assert(Near(VertexAt(fit_b.fitted, 0), VertexAt(frame_b, 0)));
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.35;

    const std::vector<std::vector<double>> frames = {
        ShapeFlat(true, DeformedSmoothOvalPolyline(76.0, 36.0, 0.05)),
        ShapeFlat(true, DeformedSmoothOvalPolyline(70.0, 42.0, 0.10)),
        ShapeFlat(true, DeformedSmoothOvalPolyline(82.0, 32.0, 0.08)),
    };
    const std::vector<double> seed = UniformClosedFractions(6);

    double initial_max_error = 0.0;
    for (const std::vector<double>& frame: frames) {
      const bbsolver::PathFrameFitResult fit =
          bbsolver::FitShapeFlatFrameAtFractions(frame, seed, options);
      assert(fit.ok);
      assert(fit.target_met);
      initial_max_error = std::max(initial_max_error, fit.max_outline_error);
    }
    assert(initial_max_error > options.outline_tolerance);

    bbsolver::PathFractionExpansionOptions expansion_options;
    expansion_options.max_fraction_count = 14;
    expansion_options.max_insertions = 8;
    const bbsolver::PathFractionExpansionResult expanded =
        bbsolver::ExpandShapeFlatOutlineFractions(frames, seed, options, expansion_options);

    assert(expanded.ok);
    assert(expanded.applied);
    assert(expanded.tolerance_met);
    assert(expanded.initial_fraction_count == static_cast<int>(seed.size()));
    assert(expanded.final_fraction_count > expanded.initial_fraction_count);
    assert(expanded.final_fraction_count <= expansion_options.max_fraction_count);
    assert(expanded.final_max_outline_error <= options.outline_tolerance);
    assert(expanded.final_max_outline_error < expanded.initial_max_outline_error);
    assert(expanded.candidate_evaluations > 0);
    assert(expanded.outline_fractions.size() == static_cast<std::size_t>(expanded.final_fraction_count));
    assert(FractionsInSeamOrder(expanded.outline_fractions, true));

    for (const std::vector<double>& frame: frames) {
      const bbsolver::PathFrameFitResult replay =
          bbsolver::FitShapeFlatFrameAtFractions(frame, expanded.outline_fractions, options);
      assert(replay.ok);
      assert(replay.applied);
      assert(replay.target_met);
      assert(replay.fitted_vertex_count == expanded.final_fraction_count);
      assert(replay.max_outline_error <= options.outline_tolerance);
      assert(HasNonZeroTangent(replay.fitted));
    }
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.1;

    const std::vector<std::vector<double>> frames = {
        ShapeFlat(true, SmoothOvalPolyline()),
    };
    bbsolver::PathFractionExpansionOptions expansion_options;
    expansion_options.max_fraction_count = 8;
    const bbsolver::PathFractionExpansionResult expanded =
        bbsolver::ExpandShapeFlatOutlineFractions(
            frames, {0.0, 0.5, 0.5}, options, expansion_options);

    assert(!expanded.ok);
    assert(!expanded.applied);
    assert(expanded.warning.find("invalid outline fractions") != std::string::npos);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.01;

    const std::vector<double> source = ShapeFlat(true, OversampledRectangle());
    const std::vector<double> fractions = {0.0, 0.25, 0.5, 0.75};
    const bbsolver::PathFrameFitResult result =
        bbsolver::FitShapeFlatFrameAtFractions(source, fractions, options);

    assert(result.ok);
    assert(result.applied);
    assert(result.closed);
    assert(result.target_met);
    assert(result.fitted_vertex_count == 4);
    assert(result.max_outline_error <= options.outline_tolerance);
    assert(AllTangentsNearZero(result.fitted));
    assert(Near(VertexAt(result.fitted, 0), {0.0, 0.0}));
    assert(Near(VertexAt(result.fitted, 1), {100.0, 0.0}));
    assert(Near(VertexAt(result.fitted, 2), {100.0, 100.0}));
    assert(Near(VertexAt(result.fitted, 3), {0.0, 100.0}));
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.01;

    const std::vector<double> source = ShapeFlat(false, {
        {0.0, 0.0}, {25.0, 0.0}, {50.0, 0.0}, {75.0, 0.0}, {100.0, 0.0},
    });
    const bbsolver::PathFrameFitResult result =
        bbsolver::FitShapeFlatFrameAtFractions(source, {0.0, 1.0}, options);

    assert(result.ok);
    assert(result.applied);
    assert(!result.closed);
    assert(result.target_met);
    assert(result.fitted_vertex_count == 2);
    assert(result.max_outline_error <= options.outline_tolerance);
    assert(Near(VertexAt(result.fitted, 0), {0.0, 0.0}));
    assert(Near(VertexAt(result.fitted, 1), {100.0, 0.0}));
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.5;

    const std::vector<double> closed_source = ShapeFlat(true, SmoothOvalPolyline());
    assert(!bbsolver::FitShapeFlatFrameAtFractions(
                closed_source, {}, options).applied);
    assert(!bbsolver::FitShapeFlatFrameAtFractions(
                closed_source, {0.0}, options).applied);
    assert(!bbsolver::FitShapeFlatFrameAtFractions(
                closed_source, {0.0, 0.5, 0.25}, options).applied);

    const std::vector<double> open_source = ShapeFlat(false, {
        {0.0, 0.0}, {25.0, 0.0}, {50.0, 0.0}, {75.0, 0.0}, {100.0, 0.0},
    });
    assert(!bbsolver::FitShapeFlatFrameAtFractions(
                open_source, {-0.1, 1.0}, options).applied);
    assert(!bbsolver::FitShapeFlatFrameAtFractions(
                open_source, {0.0, 1.1}, options).applied);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.01;

    const std::vector<double> source = ShapeFlat(true, OversampledRectangle());
    const bbsolver::PathFrameFitResult result =
        bbsolver::FitShapeFlatFrameAtFractions(source, UniformClosedFractions(16), options);

    assert(result.ok);
    assert(!result.applied);
    assert(result.target_met);
    assert(result.fitted_vertex_count == result.source_vertex_count);
    assert(result.warning.find("unchanged") != std::string::npos);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.75;

    const std::vector<Point> points = OpenCurvedWristPolyline();
    const int wrist_index = static_cast<int>(points.size()) / 2;
    const std::vector<double> source = ShapeFlatWithSmoothTangents(false, points);

    const std::vector<bbsolver::PathFeatureAnchor> anchors =
        bbsolver::ExtractShapeFlatFeatureAnchors(source, options);
    assert(anchors.size() == 1);
    assert(anchors.front().source_vertex_index == wrist_index);
    assert(anchors.front().turn_radians >= 0.50);
    assert(anchors.front().turn_radians < 0.80);
    assert(!anchors.front().zero_tangent_cue);

    const std::vector<double> straight_zero_tangent_source = ShapeFlat(false, {
        {0.0, 0.0}, {25.0, 0.0}, {50.0, 0.0}, {75.0, 0.0}, {100.0, 0.0},
    });
    assert(bbsolver::ExtractShapeFlatFeatureAnchors(
               straight_zero_tangent_source, options).empty());

    const bbsolver::PathFrameFitResult automatic =
        bbsolver::FitShapeFlatFrame(source, options);
    assert(automatic.ok);
    assert(automatic.applied);
    assert(!automatic.closed);
    assert(automatic.fitted_vertex_count < automatic.source_vertex_count);
    assert(automatic.max_outline_error <= options.outline_tolerance);

    bool automatic_has_wrist = false;
    for (int i = 0; i < automatic.fitted_vertex_count; ++i) {
      if (i < static_cast<int>(automatic.source_vertex_indices.size()) &&
          automatic.source_vertex_indices[static_cast<std::size_t>(i)] == wrist_index) {
        automatic_has_wrist = true;
        assert(Near(VertexAt(automatic.fitted, i), points[static_cast<std::size_t>(wrist_index)]));
        assert(TangentsNearZeroAt(automatic.fitted, i, 1e-5));
      }
    }
    assert(automatic_has_wrist);

    const std::vector<double> fractions =
        MergeFeatureFractions(UniformOpenFractions(7), anchors);
    assert(FractionsInSeamOrder(fractions, false));
    const bbsolver::PathFrameFitResult fixed_layout =
        bbsolver::FitShapeFlatFrameAtFractions(source, fractions, options);
    assert(fixed_layout.ok);
    assert(fixed_layout.applied);
    assert(fixed_layout.target_met);
    assert(fixed_layout.fitted_vertex_count == static_cast<int>(fractions.size()));
    assert(fixed_layout.max_outline_error <= options.outline_tolerance);

    bool fixed_has_wrist = false;
    for (int i = 0; i < fixed_layout.fitted_vertex_count; ++i) {
      if (i < static_cast<int>(fixed_layout.source_vertex_indices.size()) &&
          fixed_layout.source_vertex_indices[static_cast<std::size_t>(i)] == wrist_index) {
        fixed_has_wrist = true;
        assert(Near(VertexAt(fixed_layout.fitted, i), points[static_cast<std::size_t>(wrist_index)]));
        assert(TangentsNearZeroAt(fixed_layout.fitted, i, 1e-5));
      }
    }
    assert(fixed_has_wrist);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.75;

    const std::vector<int> wrist_steps = {19, 20, 21};
    std::vector<std::vector<double>> frames;
    std::vector<double> anchor_fractions;
    for (int wrist_step: wrist_steps) {
      const std::vector<double> frame =
          ShapeFlatWithSmoothTangents(false, OpenCurvedWristPolyline(wrist_step));
      const std::vector<bbsolver::PathFeatureAnchor> anchors =
          bbsolver::ExtractShapeFlatFeatureAnchors(frame, options);
      assert(anchors.size() == 1);
      assert(anchors.front().source_vertex_index == wrist_step);
      anchor_fractions.push_back(anchors.front().outline_fraction);
      frames.push_back(frame);
    }

    const bbsolver::PathFeatureFractionLayoutResult layout =
        bbsolver::BuildShapeFlatFeatureFractionLayout(frames, 9, options);
    assert(layout.ok);
    assert(!layout.closed);
    assert(layout.target_count == 9);
    assert(layout.frame_count == static_cast<int>(frames.size()));
    assert(layout.feature_count == 1);
    assert(layout.outline_fractions.size() == 9);
    assert(FractionsInSeamOrder(layout.outline_fractions, false));
    assert(NearDouble(layout.outline_fractions.front(), 0.0));
    assert(NearDouble(layout.outline_fractions.back(), 1.0));

    const auto minmax_anchor =
        std::minmax_element(anchor_fractions.begin(), anchor_fractions.end());
    int clustered_wrist_slots = 0;
    for (double fraction: layout.outline_fractions) {
      if (fraction >= *minmax_anchor.first - 1e-9 &&
          fraction <= *minmax_anchor.second + 1e-9) {
        ++clustered_wrist_slots;
      }
    }
    assert(clustered_wrist_slots == 1);

    for (std::size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
      const bbsolver::PathFrameFitResult fit =
          bbsolver::FitShapeFlatFrameAtFractions(
              frames[frame_index], layout.outline_fractions, options);
      assert(fit.ok);
      assert(fit.applied);
      assert(fit.target_met);
      assert(fit.fitted_vertex_count == static_cast<int>(layout.outline_fractions.size()));
      assert(fit.max_outline_error <= options.outline_tolerance);

      bool has_frame_local_wrist = false;
      for (int i = 0; i < fit.fitted_vertex_count; ++i) {
        if (i < static_cast<int>(fit.source_vertex_indices.size()) &&
            fit.source_vertex_indices[static_cast<std::size_t>(i)] ==
                wrist_steps[frame_index]) {
          has_frame_local_wrist = true;
          assert(TangentsNearZeroAt(fit.fitted, i, 1e-5));
        }
      }
      assert(has_frame_local_wrist);
    }
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 1.25;

    const int persistent_wrist_step = 20;
    const std::vector<int> transient_steps = {4, 9, 14, 27, 33, 38};
    std::vector<std::vector<double>> frames;
    std::vector<double> persistent_fractions;
    for (int transient_step: transient_steps) {
      const std::vector<double> frame = ShapeFlatWithSmoothTangents(
          false,
          OpenCurvedWristPolylineWithBumps(
              persistent_wrist_step, {transient_step}));
      const std::vector<bbsolver::PathFeatureAnchor> anchors =
          bbsolver::ExtractShapeFlatFeatureAnchors(frame, options);
      bool saw_persistent = false;
      bool saw_transient = false;
      for (const bbsolver::PathFeatureAnchor& anchor: anchors) {
        if (anchor.source_vertex_index == persistent_wrist_step) {
          saw_persistent = true;
          persistent_fractions.push_back(anchor.outline_fraction);
        }
        if (anchor.source_vertex_index == transient_step) {
          saw_transient = true;
        }
      }
      assert(saw_persistent);
      assert(saw_transient);
      frames.push_back(frame);
    }

    const bbsolver::PathFeatureFractionLayoutResult layout =
        bbsolver::BuildShapeFlatFeatureFractionLayout(frames, 10, options);
    assert(layout.ok);
    assert(!layout.closed);
    assert(layout.target_count == 10);
    assert(layout.feature_count == 1);
    assert(layout.notes.find("optional_features_skipped=") != std::string::npos);
    assert(layout.outline_fractions.size() == 10);
    assert(FractionsInSeamOrder(layout.outline_fractions, false));

    const auto persistent_minmax =
        std::minmax_element(persistent_fractions.begin(), persistent_fractions.end());
    int persistent_slots = 0;
    for (double fraction: layout.outline_fractions) {
      if (fraction >= *persistent_minmax.first - 1e-9 &&
          fraction <= *persistent_minmax.second + 1e-9) {
        ++persistent_slots;
      }
    }
    assert(persistent_slots == 1);

    for (const std::vector<double>& frame: frames) {
      const bbsolver::PathFrameFitResult fit =
          bbsolver::FitShapeFlatFrameAtFractions(
              frame, layout.outline_fractions, options);
      assert(fit.ok);
      assert(fit.applied);
      assert(fit.target_met);
      assert(fit.fitted_vertex_count == 10);
      assert(fit.max_outline_error <= options.outline_tolerance);

      bool has_persistent_wrist = false;
      for (int i = 0; i < fit.fitted_vertex_count; ++i) {
        if (i < static_cast<int>(fit.source_vertex_indices.size()) &&
            fit.source_vertex_indices[static_cast<std::size_t>(i)] ==
                persistent_wrist_step) {
          has_persistent_wrist = true;
        }
      }
      assert(has_persistent_wrist);
    }
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 2.0;

    std::vector<std::vector<double>> frames = {
        ShapeFlatWithSmoothTangents(false, OpenCurvedWristPolyline(-1)),
        ShapeFlat(false, OpenZeroTangentVPolyline()),
        ShapeFlatWithSmoothTangents(false, OpenCurvedWristPolyline(-1)),
        ShapeFlatWithSmoothTangents(false, OpenCurvedWristPolyline(-1)),
    };

    const std::vector<bbsolver::PathFeatureAnchor> corner_anchors =
        bbsolver::ExtractShapeFlatFeatureAnchors(frames[1], options);
    bool saw_zero_tangent_corner = false;
    for (const bbsolver::PathFeatureAnchor& anchor: corner_anchors) {
      if (anchor.source_vertex_index == 5 && anchor.zero_tangent_cue) {
        saw_zero_tangent_corner = true;
      }
    }
    assert(saw_zero_tangent_corner);

    const bbsolver::PathFeatureFractionLayoutResult layout =
        bbsolver::BuildShapeFlatFeatureFractionLayout(frames, 8, options);
    assert(layout.ok);
    assert(layout.feature_count == 1);
    assert(layout.outline_fractions.size() == 8);
    assert(FractionsInSeamOrder(layout.outline_fractions, false));

    const bbsolver::PathFrameFitResult fit =
        bbsolver::FitShapeFlatFrameAtFractions(
            frames[1], layout.outline_fractions, options);
    assert(fit.ok);
    assert(fit.applied);
    assert(fit.target_met);
    assert(fit.max_outline_error <= options.outline_tolerance);

    bool preserved_zero_corner = false;
    for (int i = 0; i < fit.fitted_vertex_count; ++i) {
      if (i < static_cast<int>(fit.source_vertex_indices.size()) &&
          fit.source_vertex_indices[static_cast<std::size_t>(i)] == 5) {
        preserved_zero_corner = true;
        assert(TangentsNearZeroAt(fit.fitted, i, 1e-5));
      }
    }
    assert(preserved_zero_corner);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.75;

    const std::vector<double> feature_rich = ShapeFlatWithSmoothTangents(false, {
        {0.0, 0.0}, {20.0, 0.0}, {40.0, 16.0}, {60.0, 0.0},
        {80.0, 16.0}, {100.0, 0.0}, {120.0, 0.0},
    });
    const bbsolver::PathFeatureFractionLayoutResult rejected =
        bbsolver::BuildShapeFlatFeatureFractionLayout({feature_rich}, 2, options);

    assert(!rejected.ok);
    assert(rejected.feature_count > rejected.target_count);
    assert(rejected.warning.find("feature anchor count") != std::string::npos);
  }

  // Large-deformation moving wrist: amplitude varies 9× across 8 frames.
  // Pure geometric gap splitting fails at tolerance=0.5 because it allocates
  // slots by arc-length gap, not by where the fitting error is largest. The
  // error-guided fill discovers the high-curvature high-deformation region and
  // directs slots there instead.
  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.5;

    const int n_frames = 8;
    const int path_steps = 48;
    std::vector<std::vector<double>> frames;
    for (int fi = 0; fi < n_frames; ++fi) {
      const double amplitude =
          10.0 + 80.0 * static_cast<double>(fi) / (n_frames - 1);
      std::vector<Point> points;
      for (int i = 0; i <= path_steps; ++i) {
        const double x = 300.0 * static_cast<double>(i) / path_steps;
        const double y = amplitude * std::sin(kPi * x / 300.0);
        points.push_back({x, y});
      }
      // Insert a wrist bump at step 24 (midpoint) in every frame.
      const int wrist_step = 24;
      points[static_cast<std::size_t>(wrist_step)].y += 2.5;
      frames.push_back(ShapeFlatWithSmoothTangents(false, points));
    }

    // Confirm the wrist is detectable in every frame.
    for (const std::vector<double>& frame: frames) {
      const std::vector<bbsolver::PathFeatureAnchor> anchors =
          bbsolver::ExtractShapeFlatFeatureAnchors(frame, options);
      assert(!anchors.empty());
    }

    const bbsolver::PathFeatureFractionLayoutResult layout =
        bbsolver::BuildShapeFlatFeatureFractionLayout(frames, 14, options);
    assert(layout.ok);
    assert(layout.feature_count >= 1);
    assert(static_cast<int>(layout.outline_fractions.size()) == 14);
    assert(FractionsInSeamOrder(layout.outline_fractions, false));
    assert(NearDouble(layout.outline_fractions.front(), 0.0));
    assert(NearDouble(layout.outline_fractions.back(), 1.0));

    // Error-guided fill must achieve tolerance on every frame.
    for (const std::vector<double>& frame: frames) {
      const bbsolver::PathFrameFitResult fit =
          bbsolver::FitShapeFlatFrameAtFractions(
              frame, layout.outline_fractions, options);
      assert(fit.ok);
      assert(fit.applied);
      assert(fit.target_met);
      assert(fit.max_outline_error <= options.outline_tolerance);
    }
  }

  // Asymmetric-arc test: short high-curvature half + long low-curvature half.
  // With only target_count = 5 (seam + endpoint + 3 free slots), geometric
  // gap splitting puts all free slots in the long low-error half. Error-guided
  // fill detects that the tight half needs them and achieves tolerance.
  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 1.0;

    // Open path: first half is a gentle wide arc (low curvature, long arc);
    // second half is a tight compressed arc (high curvature, short arc).
    // Deformation: 5 frames scale the tight half's amplitude by 2-5×.
    std::vector<std::vector<double>> frames;
    const int wide_steps = 30;   // long gentle half
    const int tight_steps = 10;  // short tight half
    for (int fi = 0; fi < 5; ++fi) {
      const double tight_amp =
          40.0 + 30.0 * static_cast<double>(fi) / 4.0;
      std::vector<Point> points;
      for (int i = 0; i <= wide_steps; ++i) {
        const double t = static_cast<double>(i) / wide_steps;
        points.push_back({t * 300.0, 8.0 * std::sin(kPi * t)});
      }
      for (int i = 1; i <= tight_steps; ++i) {
        const double t = static_cast<double>(i) / tight_steps;
        // Tight half: x from 300 to 360 (short), y oscillates with tight_amp.
        points.push_back({300.0 + t * 60.0,
                          tight_amp * std::sin(3.0 * kPi * t)});
      }
      frames.push_back(ShapeFlatWithSmoothTangents(false, points));
    }

    const bbsolver::PathFeatureFractionLayoutResult layout =
        bbsolver::BuildShapeFlatFeatureFractionLayout(frames, 9, options);
    assert(layout.ok);
    assert(static_cast<int>(layout.outline_fractions.size()) == 9);
    assert(FractionsInSeamOrder(layout.outline_fractions, false));
    assert(NearDouble(layout.outline_fractions.front(), 0.0));
    assert(NearDouble(layout.outline_fractions.back(), 1.0));

    // At least 2 of the 9 fractions must fall in the tight high-curvature half
    // (arc-fraction > ~0.82 which is roughly where x=300 falls on the path).
    int tight_slots = 0;
    for (double f: layout.outline_fractions) {
      if (f > 0.78) {
        ++tight_slots;
      }
    }
    assert(tight_slots >= 2);

    for (const std::vector<double>& frame: frames) {
      const bbsolver::PathFrameFitResult fit =
          bbsolver::FitShapeFlatFrameAtFractions(
              frame, layout.outline_fractions, options);
      assert(fit.ok);
      assert(fit.applied);
      assert(fit.target_met);
      assert(fit.max_outline_error <= options.outline_tolerance);
    }
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.5;

    const bbsolver::VisibleShapeFlatOutlineResult visible =
        bbsolver::ExtractVisibleShapeFlatOutline(
            ShapeFlat(true, OversampledRectangle()), options);
    assert(visible.ok);
    assert(!visible.applied);
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.outline_tolerance = 0.5;

    const std::vector<Point> regular = RegularLimbSelfOverlapPolyline();
    const bbsolver::VisibleShapeFlatOutlineResult visible =
        bbsolver::ExtractVisibleShapeFlatOutline(
            ShapeFlat(true, regular), options);
    assert(visible.ok);
    assert(visible.applied);
    assert(visible.outline_vertex_count > 3);
    assert(visible.outline_vertex_count < static_cast<int>(regular.size()));

    const bbsolver::PathFrameFitResult fit =
        bbsolver::FitShapeFlatFrame(visible.outline, options);
    assert(fit.ok);
    assert(fit.applied);
    assert(fit.fitted_vertex_count < static_cast<int>(regular.size()));
    assert(fit.max_outline_error <= options.outline_tolerance + 1e-9);
  }

  {
    bbsolver::PathFrameFitOptions authored_options;
    authored_options.outline_tolerance = 0.5;

    const std::vector<Point> regular = RegularLimbSelfOverlapPolyline();
    const bbsolver::VisibleShapeFlatOutlineResult visible =
        bbsolver::ExtractVisibleShapeFlatOutline(
            ShapeFlat(true, regular), authored_options);
    assert(visible.ok);
    assert(visible.applied);

    const std::vector<bbsolver::PathFeatureAnchor> authored_anchors =
        bbsolver::ExtractShapeFlatFeatureAnchors(visible.outline, authored_options);
    assert(!authored_anchors.empty());

    bbsolver::PathFrameFitOptions visible_options = authored_options;
    visible_options.source_vertices_are_semantic_anchors = false;
    const std::vector<bbsolver::PathFeatureAnchor> visible_anchors =
        bbsolver::ExtractShapeFlatFeatureAnchors(visible.outline, visible_options);
    assert(visible_anchors.empty());

    const bbsolver::PathFeatureFractionLayoutResult layout =
        bbsolver::BuildShapeFlatFeatureFractionLayout(
            {visible.outline}, 6, visible_options);
    assert(layout.ok);
    assert(layout.feature_count == 0);
    assert(static_cast<int>(layout.outline_fractions.size()) == 6);
    assert(FractionsInSeamOrder(layout.outline_fractions, true));
  }

  {
    const std::vector<int> ladder =
        bbsolver::BuildShapeFlatReplacementTargetLadder(
            /*initial_target_vertices=*/22,
            /*source_min_vertices=*/28);
    const std::vector<int> expected = {22, 24, 26, 27};
    assert(ladder == expected);
    for (int target: ladder) {
      assert(target < 28);
    }
  }

  {
    bbsolver::PathReplacementTargetLadderOptions options;
    options.max_candidate_targets = 3;
    const std::vector<int> ladder =
        bbsolver::BuildShapeFlatReplacementTargetLadder(
            /*initial_target_vertices=*/22,
            /*source_min_vertices=*/28,
            options);
    const std::vector<int> expected = {22, 24, 27};
    assert(ladder == expected);
  }

  {
    bbsolver::PathReplacementTargetLadderOptions options;
    options.max_target_vertices = 26;
    const std::vector<int> ladder =
        bbsolver::BuildShapeFlatReplacementTargetLadder(
            /*initial_target_vertices=*/22,
            /*source_min_vertices=*/28,
            options);
    const std::vector<int> expected = {22, 24, 26};
    assert(ladder == expected);
  }

  {
    bbsolver::PathReplacementTargetLadderOptions options;
    options.min_target_vertices = 24;
    const std::vector<int> ladder =
        bbsolver::BuildShapeFlatReplacementTargetLadder(
            /*initial_target_vertices=*/22,
            /*source_min_vertices=*/28,
            options);
    const std::vector<int> expected = {24, 26, 27};
    assert(ladder == expected);
  }

  {
    assert(bbsolver::BuildShapeFlatReplacementTargetLadder(
               /*initial_target_vertices=*/28,
               /*source_min_vertices=*/28).empty());
    assert(bbsolver::BuildShapeFlatReplacementTargetLadder(
               /*initial_target_vertices=*/22,
               /*source_min_vertices=*/22).empty());

    bbsolver::PathReplacementTargetLadderOptions options;
    options.max_target_vertices = 20;
    assert(bbsolver::BuildShapeFlatReplacementTargetLadder(
               /*initial_target_vertices=*/22,
               /*source_min_vertices=*/28,
               options).empty());
  }

  {
    bbsolver::PathFrameFitOptions options;
    options.max_subdivisions_per_segment = 32;
    const std::vector<double> source =
        ShapeFlatWithSmoothTangents(true, SmoothOvalPolyline());
    const std::vector<double> fitted =
        ShapeFlatWithSmoothTangents(true, DeformedSmoothOvalPolyline(68.0, 33.0, 0.04));
    const double direct =
        bbsolver::ShapeFlatFrameOutlineError(source, fitted, options);
    const bbsolver::ShapeFlatOutlinePolyline source_outline =
        bbsolver::BuildShapeFlatOutlinePolyline(source, options);
    const bbsolver::ShapeFlatOutlinePolyline fitted_outline =
        bbsolver::BuildShapeFlatOutlinePolyline(fitted, options);
    const double cached =
        bbsolver::ShapeFlatFrameOutlineErrorFromPolylines(
            source_outline, fitted_outline);
    assert(source_outline.ok);
    assert(fitted_outline.ok);
    assert(NearDouble(direct, cached));
  }

  {
    bbsolver::PathFrameFitOptions options;
    const std::vector<double> source =
        ShapeFlatWithSmoothTangents(false, OpenCurvedWristPolyline());
    std::vector<Point> shifted_points = OpenCurvedWristPolyline();
    for (Point& point: shifted_points) {
      point.y += 2.0;
    }
    const std::vector<double> fitted =
        ShapeFlatWithSmoothTangents(false, shifted_points);
    const double direct =
        bbsolver::ShapeFlatFrameOutlineError(source, fitted, options);
    const double cached =
        bbsolver::ShapeFlatFrameOutlineErrorFromPolylines(
            bbsolver::BuildShapeFlatOutlinePolyline(source, options),
            bbsolver::BuildShapeFlatOutlinePolyline(fitted, options));
    assert(NearDouble(direct, cached));
  }

  {
    bbsolver::PathFrameFitOptions options;
    const std::vector<double> closed =
        ShapeFlat(true, {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}});
    const std::vector<double> open =
        ShapeFlat(false, {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}});
    const double cached =
        bbsolver::ShapeFlatFrameOutlineErrorFromPolylines(
            bbsolver::BuildShapeFlatOutlinePolyline(closed, options),
            bbsolver::BuildShapeFlatOutlinePolyline(open, options));
    assert(!std::isfinite(cached));
    assert(!bbsolver::BuildShapeFlatOutlinePolyline({}, options).ok);
  }

  return 0;
}
