#target aftereffects
//@include "bbsolver-json-shim.jsx"

/*
  bbsolver-test-harness v1.0.0
  Copyright (c) 2026 IVG Design and Ilya Gusinski
  SPDX-License-Identifier: MIT

  Minimal After Effects ScriptUI integration example for bbsolver. It samples
  one selected property into a SampleBundle, runs the bbsolver CLI, parses the
  returned KeyBundle, and writes solved keys back to the selected property.
*/

(function bbsolverTestHarness(thisObj) {
    var HARNESS_NAME = "bbsolver-test-harness";
    var HARNESS_VERSION = "1.0.0";
    var SCHEMA_VERSION = 1;
    var lastSamplePath = "";
    var lastKeyPath = "";
    var lastLogPath = "";
    var lastSampleBundle = null;
    var lastSampleRecord = null;
    var lastPropertyId = "";
    var persistentLogText = "";

    function safeName(value) {
        var s = String(value || "property").replace(/[^A-Za-z0-9._-]+/g, "_");
        s = s.replace(/^_+|_+$/g, "");
        return s.length ? s : "property";
    }

    function pad2(value) {
        return value < 10 ? "0" + value : String(value);
    }

    function timestamp() {
        var d = new Date();
        return d.getFullYear() +
            pad2(d.getMonth() + 1) +
            pad2(d.getDate()) + "-" +
            pad2(d.getHours()) +
            pad2(d.getMinutes()) +
            pad2(d.getSeconds());
    }

    function isWindows() {
        try {
            return $.os.toLowerCase().indexOf("windows") >= 0;
        } catch (err) {
            return false;
        }
    }

    function scriptFolderPath() {
        try {
            return File($.fileName).parent.fsName;
        } catch (err) {
            return "";
        }
    }

    function existingFilePath(candidates) {
        for (var i = 0; i < candidates.length; i++) {
            if (!candidates[i]) {
                continue;
            }
            try {
                if ((new File(candidates[i])).exists) {
                    return candidates[i];
                }
            } catch (err) {
            }
        }
        return "";
    }

    function defaultSolverPath() {
        var envBin = "";
        try { envBin = $.getenv("BBSOLVER_BIN") || ""; } catch (errEnv) {}
        if (envBin) {
            return envBin;
        }

        var scriptDir = scriptFolderPath();
        if (isWindows()) {
            var appData = "";
            var programFiles = "";
            var programFilesX86 = "";
            try { appData = Folder.userData.fsName; } catch (errAppData) {}
            try { programFiles = $.getenv("ProgramFiles") || ""; } catch (errPf) {}
            try { programFilesX86 = $.getenv("ProgramFiles(x86)") || ""; } catch (errPfx86) {}
            var winFound = existingFilePath([
                appData + "/bbsolver/bin/bbsolver.exe",
                programFiles + "/bbsolver/bin/bbsolver.exe",
                programFilesX86 + "/bbsolver/bin/bbsolver.exe",
                scriptDir + "/bbsolver.exe",
                scriptDir + "/bin/bbsolver.exe"
            ]);
            return winFound || "bbsolver.exe";
        }

        var home = "";
        try { home = Folder("~").fsName; } catch (errHome) {}
        var macFound = existingFilePath([
            home + "/.bbsolver/bin/bbsolver",
            "/usr/local/bin/bbsolver",
            "/opt/homebrew/bin/bbsolver",
            scriptDir + "/bbsolver",
            scriptDir + "/bin/bbsolver"
        ]);
        return macFound || "bbsolver";
    }

    function defaultOutputFolder() {
        return Folder.desktop.fsName + "/" + HARNESS_NAME;
    }

    function quoteShell(value) {
        var s = String(value);
        if (isWindows()) {
            return "\"" + s.replace(/"/g, "\"\"") + "\"";
        }
        s = s.replace(/(["\\$`])/g, "\\$1");
        return "\"" + s + "\"";
    }

    function isPathLike(value) {
        var s = String(value || "");
        return s.indexOf("/") >= 0 || s.indexOf("\\") >= 0 || /^[A-Za-z]:/.test(s);
    }

    function resolveSolverPath(value) {
        var path = String(value || "");
        if (!path) {
            path = defaultSolverPath();
        }
        if (isPathLike(path) && !(new File(path)).exists) {
            throw new Error("bbsolver executable not found at:\n" + path +
                "\n\nSet the bbsolver field to a built solver binary, or set BBSOLVER_BIN.");
        }
        return path;
    }

    function activeComp() {
        if (!app.project || !(app.project.activeItem instanceof CompItem)) {
            throw new Error("Open a comp and select one animatable property.");
        }
        return app.project.activeItem;
    }

    function selectedPropertyRecord(comp) {
        for (var layerIndex = 1; layerIndex <= comp.numLayers; layerIndex++) {
            var layer = comp.layer(layerIndex);
            var selected = layer.selectedProperties;
            for (var i = 0; i < selected.length; i++) {
                var prop = selected[i];
                if (prop.propertyType === PropertyType.PROPERTY) {
                    return { layer: layer, layerIndex: layerIndex, prop: prop };
                }
            }
        }
        throw new Error("Select one supported property in the active comp.");
    }

    function log(ui, message) {
        var line = timestamp() + "  " + message;
        persistentLogText += line + "\n";
        if (lastLogPath) {
            try { writeTextFile(lastLogPath, persistentLogText); } catch (errWriteLog) {}
        }
        if (ui && ui.logBox) {
            var next = line + "\n" + ui.logBox.text;
            if (next.length > 60000) {
                next = next.substr(0, 60000);
            }
            ui.logBox.text = next;
        }
        if (ui && ui.status) {
            ui.status.text = message;
        }
    }

    function clearLog(ui) {
        if (ui && ui.logBox) {
            ui.logBox.text = "";
        }
        persistentLogText = "";
        if (lastLogPath) {
            try { writeTextFile(lastLogPath, ""); } catch (errWriteLog) {}
        }
        if (ui && ui.status) {
            ui.status.text = "Select one property in the active comp.";
        }
    }

    function startPersistentLog(folder, baseName) {
        persistentLogText = "";
        lastLogPath = folder.fsName + "/" + safeName(baseName) + ".log.txt";
        writeTextFile(lastLogPath, "");
    }

    function propertyPath(rec) {
        var parts = [];
        var p = rec.prop;
        while (p && p.parentProperty) {
            parts.unshift(p.name);
            p = p.parentProperty;
        }
        return rec.layer.containingComp.name + "/" + rec.layer.name + "/" + parts.join("/");
    }

    function propertyId(rec) {
        var parts = [];
        var p = rec.prop;
        while (p && p.parentProperty) {
            parts.unshift(p.matchName || p.name);
            p = p.parentProperty;
        }
        return "L" + rec.layerIndex + "/" + parts.join("/");
    }

    function kindFromProperty(prop) {
        var pvt = prop.propertyValueType;
        if (pvt === PropertyValueType.OneD) {
            return { kind: "Scalar", dimensions: 1, isSpatial: false, units: "scalar" };
        }
        if (pvt === PropertyValueType.TwoD) {
            return { kind: "TwoD", dimensions: 2, isSpatial: false, units: "num" };
        }
        if (pvt === PropertyValueType.ThreeD) {
            return { kind: "ThreeD", dimensions: 3, isSpatial: false, units: "num" };
        }
        if (pvt === PropertyValueType.TwoD_SPATIAL) {
            return { kind: "TwoD_Spatial", dimensions: 2, isSpatial: true, units: "px" };
        }
        if (pvt === PropertyValueType.ThreeD_SPATIAL) {
            return { kind: "ThreeD_Spatial", dimensions: 3, isSpatial: true, units: "px" };
        }
        if (pvt === PropertyValueType.COLOR) {
            return { kind: "Color", dimensions: 4, isSpatial: false, units: "rgba" };
        }
        if (pvt === PropertyValueType.SHAPE) {
            return { kind: "Custom", dimensions: 0, isSpatial: false, units: "shape_flat", isShape: true };
        }
        throw new Error("Unsupported property type: " + prop.name);
    }

    function shapeToFlat(shape) {
        var values = [shape.closed ? 1 : 0, shape.vertices.length];
        for (var i = 0; i < shape.vertices.length; i++) {
            values.push(shape.vertices[i][0], shape.vertices[i][1]);
            values.push(shape.inTangents[i][0], shape.inTangents[i][1]);
            values.push(shape.outTangents[i][0], shape.outTangents[i][1]);
        }
        return values;
    }

    function shapeTopologyFromFlat(flat) {
        if (!flat || flat.length < 2) {
            return { ok: false, warning: "shape_flat sample is missing its header" };
        }
        var closed = Number(flat[0] || 0) !== 0;
        var vertexCount = Math.round(Number(flat[1] || 0));
        var dimensions = 2 + vertexCount * 6;
        if (vertexCount <= 0 || flat.length !== dimensions) {
            return {
                ok: false,
                warning: "shape_flat sample has invalid vertex count " +
                    vertexCount + " for length " + flat.length
            };
        }
        return {
            ok: true,
            closed: closed,
            vertexCount: vertexCount,
            dimensions: dimensions,
            signature: (closed ? "closed" : "open") + ":" + vertexCount
        };
    }

    function analyzeShapeSamples(samples) {
        var sourceTopologies = [];
        var seenTopologies = {};
        var baselineClosed = null;
        var baselineVertexCount = null;
        var variableTopology = false;
        var maxVertexCount = 0;
        var maxDimensions = 0;

        for (var i = 0; i < samples.length; i++) {
            var topology = shapeTopologyFromFlat(samples[i].v);
            if (!topology.ok) {
                throw new Error("Shape sample " + i + " is invalid: " + topology.warning);
            }
            if (!seenTopologies[topology.signature]) {
                seenTopologies[topology.signature] = true;
                sourceTopologies.push(topology.signature);
            }
            if (baselineClosed === null) {
                baselineClosed = topology.closed;
                baselineVertexCount = topology.vertexCount;
            } else {
                if (baselineClosed !== topology.closed) {
                    throw new Error("Shape open/closed state changes during the sampled range. " +
                        "Variable-topology fitting requires a stable closed flag.");
                }
                if (baselineVertexCount !== topology.vertexCount) {
                    variableTopology = true;
                }
            }
            if (topology.vertexCount > maxVertexCount) {
                maxVertexCount = topology.vertexCount;
            }
            if (topology.dimensions > maxDimensions) {
                maxDimensions = topology.dimensions;
            }
        }

        return {
            variableTopology: variableTopology,
            maxVertexCount: maxVertexCount,
            maxDimensions: maxDimensions,
            sourceTopologies: sourceTopologies,
            method: variableTopology ? "shape_flat_raw_variable" : "shape_flat_exact"
        };
    }

    function flatToShape(values) {
        var shape = new Shape();
        var count = Math.max(0, Math.round(Number(values[1] || 0)));
        shape.closed = Number(values[0] || 0) !== 0;
        shape.vertices = [];
        shape.inTangents = [];
        shape.outTangents = [];
        for (var i = 0; i < count; i++) {
            var base = 2 + i * 6;
            shape.vertices.push([Number(values[base]), Number(values[base + 1])]);
            shape.inTangents.push([Number(values[base + 2]), Number(values[base + 3])]);
            shape.outTangents.push([Number(values[base + 4]), Number(values[base + 5])]);
        }
        return shape;
    }

    function valueToArray(value, kind) {
        if (kind.isShape) {
            return shapeToFlat(value);
        }
        if (typeof value === "number") {
            return [Number(value)];
        }
        var out = [];
        for (var i = 0; i < value.length; i++) {
            out.push(Number(value[i]));
        }
        return out;
    }

    function arrayToValue(values, prop) {
        if (prop.propertyValueType === PropertyValueType.OneD) {
            return Number(values[0]);
        }
        if (prop.propertyValueType === PropertyValueType.SHAPE) {
            return flatToShape(values);
        }
        var out = [];
        for (var i = 0; i < values.length; i++) {
            out.push(Number(values[i]));
        }
        return out;
    }

    function interpolationName(value) {
        if (value === KeyframeInterpolationType.HOLD) {
            return "Hold";
        }
        if (value === KeyframeInterpolationType.LINEAR) {
            return "Linear";
        }
        return "Bezier";
    }

    function interpolationType(value) {
        if (String(value) === "Hold") {
            return KeyframeInterpolationType.HOLD;
        }
        if (String(value) === "Linear") {
            return KeyframeInterpolationType.LINEAR;
        }
        return KeyframeInterpolationType.BEZIER;
    }

    function easeToJson(eases) {
        var out = [];
        if (!eases) {
            return out;
        }
        for (var i = 0; i < eases.length; i++) {
            out.push({
                speed: Number(eases[i].speed || 0),
                influence: Number(eases[i].influence || 33.3)
            });
        }
        return out;
    }

    function easeFromJson(items, dimensions) {
        var out = [];
        var count = items && items.length ? items.length : Math.max(1, dimensions || 1);
        for (var i = 0; i < count; i++) {
            var item = items && items[i] ? items[i] : { speed: 0, influence: 33.3 };
            out.push(new KeyframeEase(Number(item.speed || 0), Number(item.influence || 33.3)));
        }
        return out;
    }

    function keyIndexAtTime(prop, time, epsilon) {
        for (var i = 1; i <= prop.numKeys; i++) {
            if (Math.abs(prop.keyTime(i) - time) <= epsilon) {
                return i;
            }
        }
        return 0;
    }

    function keyTimingForSample(prop, time, kind, frameDuration) {
        var index = keyIndexAtTime(prop, time, frameDuration * 0.25);
        if (!index) {
            return null;
        }
        var timing = {};
        try { timing.interp_in = interpolationName(prop.keyInInterpolationType(index)); } catch (err1) {}
        try { timing.interp_out = interpolationName(prop.keyOutInterpolationType(index)); } catch (err2) {}
        try { timing.temporal_ease_in = easeToJson(prop.keyInTemporalEase(index)); } catch (err3) {}
        try { timing.temporal_ease_out = easeToJson(prop.keyOutTemporalEase(index)); } catch (err4) {}
        try { timing.temporal_continuous = !!prop.keyTemporalContinuous(index); } catch (err5) {}
        try { timing.temporal_auto_bezier = !!prop.keyTemporalAutoBezier(index); } catch (err6) {}
        if (kind.isSpatial) {
            try { timing.spatial_in = valueToArray(prop.keyInSpatialTangent(index), kind); } catch (err7) {}
            try { timing.spatial_out = valueToArray(prop.keyOutSpatialTangent(index), kind); } catch (err8) {}
            try { timing.spatial_continuous = !!prop.keySpatialContinuous(index); } catch (err9) {}
            try { timing.spatial_auto_bezier = !!prop.keySpatialAutoBezier(index); } catch (err10) {}
            try { timing.roving = !!prop.keyRoving(index); } catch (err11) {}
        }
        return timing;
    }

    function sourceKeyTimes(prop, t0, t1) {
        var times = [];
        for (var i = 1; i <= prop.numKeys; i++) {
            var t = prop.keyTime(i);
            if (t >= t0 - 1e-6 && t <= t1 + 1e-6) {
                times.push(t);
            }
        }
        return times;
    }

    function buildLayerXform(layer) {
        function readArray(prop) {
            try {
                if (!prop) {
                    return [];
                }
                var value = prop.value;
                if (typeof value === "number") {
                    return [Number(value)];
                }
                var out = [];
                for (var i = 0; i < value.length; i++) {
                    out.push(Number(value[i]));
                }
                return out;
            } catch (err) {
                return [];
            }
        }
        var xform = layer.property("ADBE Transform Group");
        if (!xform) {
            return null;
        }
        return {
            anchor_point: readArray(xform.property("ADBE Anchor Point")),
            position: readArray(xform.property("ADBE Position")),
            scale: readArray(xform.property("ADBE Scale")),
            rotation: readArray(xform.property("ADBE Rotate Z")),
            opacity: (function () {
                var opacity = xform.property("ADBE Opacity");
                try { return opacity ? Number(opacity.value) : 100; } catch (err) { return 100; }
            })()
        };
    }

    function layerHasParent(layer) {
        try {
            return layer && layer.parent !== null;
        } catch (err) {
            return false;
        }
    }

    function canFlattenParentedPosition(rec, kind) {
        return parentFlattenBlocker(rec, kind) === "";
    }

    function parentFlattenBlocker(rec, kind) {
        if (!rec || !rec.layer || !rec.prop || !kind) {
            return "selected property record is incomplete";
        }
        if (!kind.isSpatial || kind.dimensions !== 2) {
            return "selected property is not a 2D spatial property";
        }
        if (!isLayerTransformPosition(rec.prop)) {
            return "selected property is not the layer Transform > Position property";
        }
        try {
            if (rec.layer.threeDLayer === true) {
                return "selected layer is 3D; this example only flattens parented 2D layers";
            }
        } catch (err3d) {
        }
        if (!layerHasParent(rec.layer)) {
            return "selected layer has no parent";
        }
        if (typeof rec.layer.sourcePointToComp !== "function") {
            return "selected layer cannot be sampled with sourcePointToComp";
        }
        return "";
    }

    function isLayerTransformPosition(prop) {
        if (!prop) {
            return false;
        }
        if (prop.matchName !== "ADBE Position" && prop.name !== "Position") {
            return false;
        }
        var parent = null;
        try { parent = prop.parentProperty; } catch (err) {}
        if (!parent) {
            return false;
        }
        return parent.matchName === "ADBE Transform Group";
    }

    function sourcePointToCompAtTime(layer, comp, point, tSec) {
        var oldTime = comp.time;
        var out = null;
        var errText = "";
        try {
            comp.time = tSec;
            out = layer.sourcePointToComp(point);
        } catch (err) {
            errText = err.toString();
        }
        try { comp.time = oldTime; } catch (restoreErr) {}
        if (!out || out.length < 2) {
            throw new Error("sourcePointToComp failed while sampling parented Position in comp space" +
                (errText ? ": " + errText : ""));
        }
        return [Number(out[0]), Number(out[1])];
    }

    function sampleLayerAnchorInComp(layer, comp, tSec) {
        var anchor = [0, 0];
        try {
            var xform = layer.property("ADBE Transform Group");
            var anchorProp = xform.property("ADBE Anchor Point");
            var value = anchorProp.valueAtTime(tSec, false);
            anchor = [Number(value[0]), Number(value[1])];
        } catch (err) {
        }
        return sourcePointToCompAtTime(layer, comp, anchor, tSec);
    }

    function unparentLayerForFlatten(layer) {
        if (!layerHasParent(layer)) {
            return true;
        }
        try {
            layer.parent = null;
            return true;
        } catch (errParent) {
        }
        try {
            layer.setParentWithJump(null);
            return true;
        } catch (errJump) {
        }
        return false;
    }

    function propertyInfoFromLastBundle(propertyId) {
        if (!lastSampleBundle || !lastSampleBundle.properties) {
            return null;
        }
        for (var i = 0; i < lastSampleBundle.properties.length; i++) {
            var info = lastSampleBundle.properties[i].property || {};
            if (info.id === propertyId) {
                return info;
            }
        }
        return null;
    }

    function expressionEnabled(prop) {
        try { return prop.expressionEnabled === true; } catch (err) {}
        return false;
    }

    function expressionText(prop) {
        try { return prop.expression || ""; } catch (err) {}
        return "";
    }

    function disableExpressionForBake(prop, ui) {
        var text = expressionText(prop);
        var enabled = expressionEnabled(prop);
        if (!text && !enabled) {
            log(ui, "expression: none detected on selected property.");
            return;
        }
        log(ui, "expression: detected " + (enabled ? "enabled" : "disabled") +
            " expression; length=" + text.length + ".");
        if (!enabled) {
            return;
        }
        try {
            prop.expressionEnabled = false;
        } catch (errDisable) {
            throw new Error("Could not disable expression after bake: " + errDisable.toString());
        }
        if (expressionEnabled(prop)) {
            throw new Error("Expression is still enabled after writeback attempted to disable it.");
        }
        log(ui, "expression: disabled after bake; expression text preserved on property.");
    }

    function readOptions(ui) {
        return {
            solverPath: ui.solverPath.text,
            outFolder: ui.outFolder.text,
            tolerance: Number(ui.tolerance.text) || 0.5,
            screenPx: Number(ui.screenPx.text) || 0,
            jobs: Math.max(0, parseInt(ui.jobs.text, 10) || 0),
            solveMode: ui.solveMode.selection ? ui.solveMode.selection.text : "full",
            motionPathSmoothingTolerance: Number(ui.motionPathSmoothingTolerance.text) || 3.0,
            motionPathAccuracyTolerance: Number(ui.motionPathAccuracyTolerance.text) || 1.5,
            motionPathPreserveBounds: !!ui.motionPathPreserveBounds.value,
            motionPathBoundsTolerance: Math.max(0, Number(ui.motionPathBoundsTolerance.text) || 0),
            motionPathPreserveSharpPoints: !!ui.motionPathPreserveSharpPoints.value,
            motionPathRespectKeyedFrames: !!ui.motionPathRespectKeyedFrames.value,
            fitVariableTopologyPaths: !!ui.fitVariableTopologyPaths.value,
            flattenParentedPosition: !!ui.flattenParentedPosition.value,
            disableExpressionAfterApply: !!ui.disableExpressionAfterApply.value
        };
    }

    function buildSampleBundle(options) {
        var comp = activeComp();
        var rec = selectedPropertyRecord(comp);
        var prop = rec.prop;
        var kind = kindFromProperty(prop);
        var frameDuration = comp.frameDuration;
        var t0 = comp.workAreaStart;
        var t1 = comp.workAreaStart + comp.workAreaDuration;
        var samples = [];
        var shapeInfo = null;
        var parentFlattenReason = parentFlattenBlocker(rec, kind);
        var flattenThisPosition = options.flattenParentedPosition && !parentFlattenReason;
        if (options.flattenParentedPosition && parentFlattenReason) {
            throw new Error("Parent-flatten option is enabled but cannot run: " +
                parentFlattenReason + ".");
        }
        lastSampleRecord = rec;
        lastPropertyId = propertyId(rec);

        for (var t = t0; t <= t1 + frameDuration * 0.5; t += frameDuration) {
            var sampleValue = flattenThisPosition
                ? sampleLayerAnchorInComp(rec.layer, comp, t)
                : valueToArray(prop.valueAtTime(t, false), kind);
            var sample = {
                t_sec: Number(t.toFixed(9)),
                v: sampleValue
            };
            var timing = flattenThisPosition ? null : keyTimingForSample(prop, t, kind, frameDuration);
            if (timing) {
                sample.key_timing = timing;
            }
            samples.push(sample);
        }

        if (kind.isShape && samples.length) {
            shapeInfo = analyzeShapeSamples(samples);
            kind.dimensions = shapeInfo.maxDimensions;
        }
        var fitVariableTopology = !!(shapeInfo &&
            shapeInfo.variableTopology &&
            options.fitVariableTopologyPaths);

        return {
            _schema: "samples",
            schema_version: SCHEMA_VERSION,
            request_id: HARNESS_NAME + "-" + timestamp() + "-" + safeName(prop.name),
            comp: {
                fps: 1.0 / comp.frameDuration,
                duration_sec: comp.duration,
                width: comp.width,
                height: comp.height,
                pixel_aspect: comp.pixelAspect,
                shutter_angle_deg: comp.shutterAngle,
                shutter_phase_deg: comp.shutterPhase,
                motion_blur_enabled: !!comp.motionBlur,
                work_area_start_sec: t0,
                work_area_end_sec: t1
            },
            properties: [{
                property: {
                    id: propertyId(rec),
                    match_name: prop.matchName || "",
                    display_name: prop.name || "",
                    layer_path: propertyPath(rec),
                    kind: kind.kind,
                    dimensions: kind.dimensions,
                    is_spatial: kind.isSpatial,
                    is_separated: false,
                    units_label: kind.units,
                    source_key_times: sourceKeyTimes(prop, t0, t1),
                    flatten_parented_position: flattenThisPosition,
                    shape_canonicalized: false,
                    shape_variable_topology: shapeInfo ? shapeInfo.variableTopology : false,
                    shape_canonical_method: shapeInfo ? shapeInfo.method : "",
                    shape_canonical_vertex_count: shapeInfo && !shapeInfo.variableTopology ?
                        shapeInfo.maxVertexCount : 0,
                    shape_max_vertex_count: shapeInfo ? shapeInfo.maxVertexCount : 0,
                    shape_source_topologies: shapeInfo ? shapeInfo.sourceTopologies : [],
                    source_expression_enabled: expressionEnabled(prop),
                    source_expression_length: expressionText(prop).length,
                    sample_space: flattenThisPosition ? "comp" : "property",
                    writeback_mode: flattenThisPosition ? "unparent_position" : "normal"
                },
                t_start_sec: t0,
                t_end_sec: t1,
                samples_per_frame: 1,
                samples: samples,
                layer_xform_at_start: buildLayerXform(rec.layer)
            }],
            config: {
                tolerance: options.tolerance,
                tolerance_screen_px: options.screenPx,
                solve_optimization_mode: options.solveMode,
                parallel_jobs: options.jobs,
                motion_path_smoothing_tolerance: options.motionPathSmoothingTolerance,
                motion_path_accuracy_tolerance: options.motionPathAccuracyTolerance,
                motion_path_preserve_bounds: options.motionPathPreserveBounds,
                motion_path_bounds_tolerance: options.motionPathBoundsTolerance,
                motion_path_preserve_sharp_points: options.motionPathPreserveSharpPoints,
                motion_path_respect_keyed_frames: options.motionPathRespectKeyedFrames,
                allow_path_replacement_fit: fitVariableTopology,
                path_replacement_prefer_vertices: false
            }
        };
    }

    function ensureFolder(path) {
        var folder = new Folder(path);
        if (!folder.exists && !folder.create()) {
            throw new Error("Could not create output folder: " + path);
        }
        return folder;
    }

    function sampleToFile(ui) {
        var options = readOptions(ui);
        var folder = ensureFolder(options.outFolder);
        startPersistentLog(folder, HARNESS_NAME + "-" + timestamp());
        log(ui, "log file: " + lastLogPath);
        var bundle = buildSampleBundle(options);
        var name = safeName(bundle.properties[0].property.display_name) + "-" + timestamp();
        lastSamplePath = folder.fsName + "/" + name + ".bbsm.json";
        lastKeyPath = folder.fsName + "/" + name + ".bbky.json";
        lastSampleBundle = bundle;
        writeSampleBundleJson(bundle, lastSamplePath);
        log(ui, "SampleBundle: " + lastSamplePath);
        log(ui, "KeyBundle target: " + lastKeyPath);
        log(ui, "sampled " + bundle.properties[0].samples.length + " frame sample(s) from " +
            bundle.properties[0].property.layer_path);
        log(ui, "expression source: enabled=" +
            bundle.properties[0].property.source_expression_enabled +
            ", length=" + bundle.properties[0].property.source_expression_length);
        if (bundle.properties[0].property.shape_variable_topology === true) {
            log(ui, "shape topology: raw variable topology detected; max vertices=" +
                bundle.properties[0].property.shape_max_vertex_count +
                "; solver-side replacement fitting=" +
                (bundle.config.allow_path_replacement_fit === true ? "enabled" : "disabled") + ".");
        } else if (bundle.properties[0].property.units_label === "shape_flat") {
            log(ui, "shape topology: stable shape_flat topology; vertices=" +
                bundle.properties[0].property.shape_max_vertex_count + ".");
        }
        if (bundle.properties[0].property.flatten_parented_position === true) {
            log(ui, "parent-flatten: sampled 2D Position in comp space; apply will unparent the layer.");
        } else if (options.flattenParentedPosition === true) {
            log(ui, "parent-flatten: option enabled but selected property is not an eligible parented 2D Position.");
        }
        return lastSamplePath;
    }

    function runSolver(ui) {
        var options = readOptions(ui);
        var solverPath = resolveSolverPath(options.solverPath);
        if (!lastSamplePath || !(new File(lastSamplePath)).exists) {
            sampleToFile(ui);
        }
        var command = quoteShell(solverPath) +
            " solve " + quoteShell(lastSamplePath) +
            " " + quoteShell(lastKeyPath) +
            " --tolerance " + options.tolerance +
            " --screen-px " + options.screenPx +
            " --jobs " + options.jobs +
            " --solve-mode " + options.solveMode;
        if (options.fitVariableTopologyPaths &&
                lastSampleBundle &&
                lastSampleBundle.properties &&
                lastSampleBundle.properties.length &&
                lastSampleBundle.properties[0].property &&
                lastSampleBundle.properties[0].property.shape_variable_topology === true) {
            command += " --fit-replacement-paths";
        }
        ui.solverPath.text = solverPath;
        log(ui, "running bbsolver: " + command);
        var output = system.callSystem(command);
        if (output) {
            log(ui, "bbsolver output: " + output);
        }
        if (!(new File(lastKeyPath)).exists) {
            throw new Error("bbsolver did not write a KeyBundle.\n\n" + output);
        }
        log(ui, "KeyBundle: " + lastKeyPath);
        return lastKeyPath;
    }

    function applyKeyTiming(prop, index, key, dimensions, isSpatial) {
        try { prop.setInterpolationTypeAtKey(index, interpolationType(key.interp_in), interpolationType(key.interp_out)); } catch (err1) {}
        if (prop.propertyValueType !== PropertyValueType.SHAPE) {
            try { prop.setTemporalEaseAtKey(index, easeFromJson(key.temporal_ease_in, dimensions), easeFromJson(key.temporal_ease_out, dimensions)); } catch (err2) {}
        }
        try { prop.setTemporalContinuousAtKey(index, !!key.temporal_continuous); } catch (err3) {}
        try { prop.setTemporalAutoBezierAtKey(index, !!key.temporal_auto_bezier); } catch (err4) {}
        if (isSpatial) {
            try { prop.setSpatialContinuousAtKey(index, !!key.spatial_continuous); } catch (err5) {}
            try { prop.setSpatialAutoBezierAtKey(index, !!key.spatial_auto_bezier); } catch (err6) {}
            try { prop.setSpatialTangentsAtKey(index, key.spatial_in || [], key.spatial_out || []); } catch (err7) {}
            try {
                if (index > 1 && index < prop.numKeys) {
                    prop.setRovingAtKey(index, !!key.roving);
                }
            } catch (err8) {}
        }
    }

    function applyKeyBundle(ui) {
        if (!lastKeyPath || !(new File(lastKeyPath)).exists) {
            throw new Error("No KeyBundle path yet. Run Solve first.");
        }
        var bundle = readKeyBundleJson(lastKeyPath);
        if (!bundle.property_results || !bundle.property_results.length) {
            throw new Error("KeyBundle has no property_results.");
        }
        var comp = activeComp();
        var rec = lastSampleRecord || selectedPropertyRecord(comp);
        var prop = rec.prop;
        var kind = kindFromProperty(prop);
        var wantedId = lastPropertyId || propertyId(rec);
        var sampleInfo = propertyInfoFromLastBundle(wantedId);
        var result = null;
        for (var i = 0; i < bundle.property_results.length; i++) {
            if (bundle.property_results[i].property_id === wantedId) {
                result = bundle.property_results[i];
                break;
            }
        }
        if (!result) {
            throw new Error("KeyBundle does not contain selected property id: " + wantedId);
        }
        if (result.converged === false) {
            throw new Error("Refusing to apply non-converged result: " + (result.notes || ""));
        }

        app.beginUndoGroup(HARNESS_NAME + " apply");
        try {
            if (sampleInfo && sampleInfo.flatten_parented_position === true) {
                log(ui, "parent-flatten: unparenting layer before comp-space Position writeback.");
                if (!unparentLayerForFlatten(rec.layer)) {
                    throw new Error("Could not unparent layer for parent-flatten writeback.");
                }
                log(ui, "parent-flatten: layer parent after unparent attempt = " +
                    (layerHasParent(rec.layer) ? "still parented" : "none") + ".");
            }
            if (readOptions(ui).disableExpressionAfterApply) {
                disableExpressionForBake(prop, ui);
            }
            while (prop.numKeys > 0) {
                prop.removeKey(prop.numKeys);
            }
            for (var k = 0; k < result.keys.length; k++) {
                prop.setValueAtTime(Number(result.keys[k].t_sec), arrayToValue(result.keys[k].v || [], prop));
            }
            for (var j = 0; j < result.keys.length; j++) {
                var index = keyIndexAtTime(prop, Number(result.keys[j].t_sec), comp.frameDuration * 0.25);
                if (index) {
                    applyKeyTiming(prop, index, result.keys[j], kind.dimensions, kind.isSpatial);
                }
            }
        } finally {
            app.endUndoGroup();
        }
        log(ui, "applied " + result.keys.length + " key(s) to " + prop.name + ".");
    }

    function buildUi(target) {
        var title = HARNESS_NAME + " v" + HARNESS_VERSION;
        var win = target instanceof Panel ? target : new Window("palette", title, undefined, { resizeable: true });
        win.text = title;
        win.orientation = "column";
        win.alignChildren = "fill";
        win.margins = 12;
        win.spacing = 8;

        var header = win.add("statictext", undefined, title);
        try {
            header.graphics.font = ScriptUI.newFont(header.graphics.font.name, "Bold", header.graphics.font.size + 1);
        } catch (err) {}

        var paths = win.add("panel", undefined, "Paths");
        paths.orientation = "column";
        paths.alignChildren = "fill";
        paths.margins = 10;

        var solverRow = paths.add("group");
        solverRow.add("statictext", undefined, "bbsolver:");
        var solverPath = solverRow.add("edittext", undefined, defaultSolverPath());
        solverPath.characters = 34;
        var solverBrowse = solverRow.add("button", undefined, "...");

        var folderRow = paths.add("group");
        folderRow.add("statictext", undefined, "Output:");
        var outFolder = folderRow.add("edittext", undefined, defaultOutputFolder());
        outFolder.characters = 35;
        var browse = folderRow.add("button", undefined, "...");

        var options = win.add("panel", undefined, "Solve");
        options.orientation = "column";
        options.alignChildren = "left";
        options.margins = 10;

        var modeRow = options.add("group");
        modeRow.add("statictext", undefined, "Mode:");
        var solveMode = modeRow.add("dropdownlist", undefined, ["full", "temporal_only", "vertex_only", "motion_smooth", "motion_path_smooth"]);
        solveMode.selection = 0;

        var toleranceRow = options.add("group");
        toleranceRow.add("statictext", undefined, "Tolerance:");
        var tolerance = toleranceRow.add("edittext", undefined, "0.5");
        tolerance.characters = 7;
        toleranceRow.add("statictext", undefined, "Screen px:");
        var screenPx = toleranceRow.add("edittext", undefined, "0");
        screenPx.characters = 7;
        toleranceRow.add("statictext", undefined, "Jobs:");
        var jobs = toleranceRow.add("edittext", undefined, "0");
        jobs.characters = 4;

        var motionRow = options.add("group");
        motionRow.add("statictext", undefined, "\u03b1 Smooth:");
        var motionPathSmoothingTolerance = motionRow.add("edittext", undefined, "3.0");
        motionPathSmoothingTolerance.characters = 6;
        motionPathSmoothingTolerance.helpTip = "Motion Path Smooth only. Smoothing strength is unitless. Default 3.0; min 1.0; max 32.0. Lower values keep the target path closer to the original; higher values smooth more aggressively.";
        motionRow.add("statictext", undefined, "\u03b5 Fit:");
        var motionPathAccuracyTolerance = motionRow.add("edittext", undefined, "1.5");
        motionPathAccuracyTolerance.characters = 6;
        motionPathAccuracyTolerance.helpTip = "Motion Path Smooth only. Spatiotemporal fit tolerance. Units: comp px for Position. Default 1.5; min 0.1; max 200. Lower values keep more keys closer to the smoothed path; higher values allow fewer keys.";
        var motionBoundsRow = options.add("group");
        var motionPathPreserveBounds = motionBoundsRow.add("checkbox", undefined, "preserve bounds");
        motionPathPreserveBounds.value = false;
        motionPathPreserveBounds.helpTip = "Motion Path Smooth only. Keeps the smoothed target within the original sampled motion-path bounds.";
        motionBoundsRow.add("statictext", undefined, "\u03b5 Bounds:");
        var motionPathBoundsTolerance = motionBoundsRow.add("edittext", undefined, "0");
        motionPathBoundsTolerance.characters = 6;
        motionPathBoundsTolerance.helpTip = "Motion Path Smooth only. Global motion-path bounds tolerance. Units: comp px for Position. Default 0; min 0; max 500. At 0 the target keeps original outer bounds; higher values allow each side to deviate farther.";

        var flagsRow = options.add("group");
        var motionPathPreserveSharpPoints = flagsRow.add("checkbox", undefined, "preserve sharp points");
        motionPathPreserveSharpPoints.value = true;
        var motionPathRespectKeyedFrames = flagsRow.add("checkbox", undefined, "respect keyed frames");
        motionPathRespectKeyedFrames.value = false;

        var shapeRow = options.add("group");
        var fitVariableTopologyPaths = shapeRow.add("checkbox", undefined, "fit variable-topology shape paths");
        fitVariableTopologyPaths.value = true;
        fitVariableTopologyPaths.helpTip = "For Shape Path samples whose vertex count changes, export raw frames and enable bbsolver replacement-path fitting. Requires full solve mode.";

        var parentRow = options.add("group");
        var flattenParentedPosition = parentRow.add("checkbox", undefined, "flatten parented 2D Position and unparent on apply");
        flattenParentedPosition.value = false;
        flattenParentedPosition.helpTip = "Samples parented 2D Position in comp space and unparents before writing comp-space keys. Use this when parent transforms are part of the motion you want baked.";

        var expressionRow = options.add("group");
        var disableExpressionAfterApply = expressionRow.add("checkbox", undefined, "disable expression after apply");
        disableExpressionAfterApply.value = true;
        disableExpressionAfterApply.helpTip = "Turns off the selected property's expression after writing solved keys, while preserving the expression text.";

        var buttons = win.add("group");
        buttons.alignment = "right";
        var sampleButton = buttons.add("button", undefined, "Sample");
        var solveButton = buttons.add("button", undefined, "Solve");
        var applyButton = buttons.add("button", undefined, "Apply");
        var allButton = buttons.add("button", undefined, "Sample + Solve + Apply");
        var clearButton = buttons.add("button", undefined, "Clear Log");

        var status = win.add("statictext", undefined, "Select one property in the active comp.");
        status.characters = 64;

        var logBox = win.add("edittext", undefined, "", { multiline: true, scrolling: true });
        logBox.preferredSize = [560, 140];
        logBox.alignment = ["fill", "fill"];

        var ui = {
            solverPath: solverPath,
            outFolder: outFolder,
            solveMode: solveMode,
            tolerance: tolerance,
            screenPx: screenPx,
            jobs: jobs,
            motionPathSmoothingTolerance: motionPathSmoothingTolerance,
            motionPathAccuracyTolerance: motionPathAccuracyTolerance,
            motionPathPreserveBounds: motionPathPreserveBounds,
            motionPathBoundsTolerance: motionPathBoundsTolerance,
            motionPathPreserveSharpPoints: motionPathPreserveSharpPoints,
            motionPathRespectKeyedFrames: motionPathRespectKeyedFrames,
            fitVariableTopologyPaths: fitVariableTopologyPaths,
            flattenParentedPosition: flattenParentedPosition,
            disableExpressionAfterApply: disableExpressionAfterApply,
            status: status,
            logBox: logBox
        };
        log(ui, "ready; bbsolver path: " + solverPath.text);
        log(ui, "output folder: " + outFolder.text);

        function reportError(err) {
            log(ui, "ERROR: " + err.toString());
            alert(HARNESS_NAME + "\n\n" + err.toString());
        }

        solverBrowse.onClick = function () {
            var chosen = File.openDialog("Choose bbsolver executable");
            if (chosen) {
                solverPath.text = chosen.fsName;
                log(ui, "bbsolver path set to: " + chosen.fsName);
            }
        };
        browse.onClick = function () {
            var folder = Folder.selectDialog("Choose " + HARNESS_NAME + " output folder", new Folder(outFolder.text));
            if (folder) {
                outFolder.text = folder.fsName;
                log(ui, "output folder set to: " + folder.fsName);
            }
        };
        sampleButton.onClick = function () { try { sampleToFile(ui); } catch (err) { reportError(err); } };
        solveButton.onClick = function () { try { runSolver(ui); } catch (err) { reportError(err); } };
        applyButton.onClick = function () { try { applyKeyBundle(ui); } catch (err) { reportError(err); } };
        clearButton.onClick = function () { clearLog(ui); };
        allButton.onClick = function () {
            try {
                sampleToFile(ui);
                runSolver(ui);
                applyKeyBundle(ui);
            } catch (err) {
                reportError(err);
            }
        };

        win.onResizing = win.onResize = function () { this.layout.resize(); };
        win.layout.layout(true);
        return win;
    }

    var win = buildUi(thisObj);
    if (win instanceof Window) {
        win.center();
        win.show();
    }
})(this);
