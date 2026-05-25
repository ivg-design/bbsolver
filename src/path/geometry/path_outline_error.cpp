// Implements the bbsolver outline-error trio declared in path_frame_fit.hpp:
//   ShapeFlatFrameOutlineError(source, fitted, options, cutoff)
//   BuildShapeFlatOutlinePolyline(shape_flat, options)
//   ShapeFlatFrameOutlineErrorFromPolylines(source, fitted, cutoff)
//
// The dense-polyline / outline-polyline pipeline lives in pff_dense. Behavior
// is stable with the previous anonymous-namespace definitions in path_frame_fit.cpp:
// `cutoff_error` short-circuits the second pass exactly as before, mismatched
// open/closed yields infinity, malformed shape_flat yields a not-ok
// polyline.
//
// Diagnostics decision: **none / pure layout**. Pure geometric helpers that
// return double / struct results. No DiagnosticsWriter, no progress, no
// cancellation, no operator state. Failure modes (malformed input, open/
// closed mismatch) are surfaced through sentinel return values (infinity or
// a not-ok ShapeFlatOutlinePolyline) just as before.

#include "bbsolver/path/frame_fit/path_frame_fit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "bbsolver/path/dense/path_dense_polyline.hpp"
#include "bbsolver/path/frame_fit/path_frame_fit_geometry.hpp"

namespace bbsolver {

ShapeFlatOutlinePolyline BuildShapeFlatOutlinePolyline(
    const std::vector<double>& shape_flat,
    const PathFrameFitOptions& options) {
  ShapeFlatOutlinePolyline out;
  const pff_geom::DecodedShape decoded = pff_geom::DecodeShapeFlat(shape_flat);
  if (!decoded.ok) {
    return out;
  }
  out.ok = true;
  out.closed = decoded.closed;
  out.points = pff_dense::DenseToOutlinePoints(
      pff_dense::ShapeFlatToDensePolyline(shape_flat, options, nullptr));
  return out;
}

double ShapeFlatFrameOutlineErrorFromPolylines(
    const ShapeFlatOutlinePolyline& source,
    const ShapeFlatOutlinePolyline& fitted,
    double cutoff_error) {
  if (!source.ok || !fitted.ok || source.closed != fitted.closed) {
    return std::numeric_limits<double>::infinity();
  }
  const double source_to_fitted = pff_dense::DirectedOutlinePolylineDistance(
      source.points, fitted.points, source.closed, cutoff_error);
  if (std::isfinite(cutoff_error) && source_to_fitted > cutoff_error) {
    return source_to_fitted;
  }
  return std::max(
      source_to_fitted,
      pff_dense::DirectedOutlinePolylineDistance(
          fitted.points, source.points, source.closed, cutoff_error));
}

double ShapeFlatFrameOutlineError(const std::vector<double>& source,
                                  const std::vector<double>& fitted,
                                  const PathFrameFitOptions& options,
                                  double cutoff_error) {
  return ShapeFlatFrameOutlineErrorFromPolylines(
      BuildShapeFlatOutlinePolyline(source, options),
      BuildShapeFlatOutlinePolyline(fitted, options),
      cutoff_error);
}

}  // namespace bbsolver
