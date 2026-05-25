/*
  bbsolver-json-shim v1.0.0
  Copyright (c) 2026 IVG Design and Ilya Gusinski
  SPDX-License-Identifier: MIT

  Minimal JSON file helpers for After Effects / ExtendScript host examples.
  The stable bbsolver v1 process boundary is JSON SampleBundle input and JSON
  KeyBundle output. Include this file before host code that writes
  *.bbsm.json or reads *.bbky.json.
*/

var BBSOLVER_JSON_SHIM_VERSION = "1.0.0";
var BBSOLVER_SCHEMA_VERSION = 1;

function bbsolverHasJson() {
    return typeof JSON !== "undefined" &&
        JSON &&
        typeof JSON.stringify === "function" &&
        typeof JSON.parse === "function";
}

function bbsolverRequireJson() {
    if (!bbsolverHasJson()) {
        throw new Error("bbsolver JSON shim requires native JSON support in After Effects.");
    }
}

function bbsolverReadTextFile(path) {
    var file = new File(path);
    file.encoding = "UTF-8";
    if (!file.exists) {
        throw new Error("bbsolverReadTextFile: file not found: " + path);
    }
    if (!file.open("r")) {
        throw new Error("bbsolverReadTextFile: could not open for read: " + path);
    }
    var text = file.read();
    file.close();
    return text;
}

function bbsolverWriteTextFile(path, text) {
    var file = new File(path);
    file.encoding = "UTF-8";
    if (!file.open("w")) {
        throw new Error("bbsolverWriteTextFile: could not open for write: " + path);
    }
    file.write(text);
    file.close();
    return true;
}

function bbsolverStringifyJson(value) {
    bbsolverRequireJson();
    try {
        return JSON.stringify(value, null, 2);
    } catch (err) {
        throw new Error("bbsolverStringifyJson: JSON serialization failed: " + err.message);
    }
}

function bbsolverParseJson(text, label) {
    bbsolverRequireJson();
    if (!text || !text.length) {
        throw new Error("bbsolverParseJson: empty JSON text" + (label ? " in " + label : ""));
    }
    try {
        return JSON.parse(text);
    } catch (err) {
        throw new Error("bbsolverParseJson: JSON parse failed" +
            (label ? " in " + label : "") + ": " + err.message);
    }
}

function bbsolverRequireArray(value, label) {
    if (!value || !(value instanceof Array)) {
        throw new Error(label + " must be an array");
    }
}

function bbsolverRequireNonEmptyArray(value, label) {
    bbsolverRequireArray(value, label);
    if (value.length === 0) {
        throw new Error(label + " must not be empty");
    }
}

function bbsolverRequireSchemaVersion(bundle, label) {
    if (!bundle || typeof bundle !== "object") {
        throw new Error(label + " must be an object");
    }
    if (bundle.schema_version !== BBSOLVER_SCHEMA_VERSION) {
        throw new Error(label + " schema_version " + bundle.schema_version +
            " is unsupported; expected " + BBSOLVER_SCHEMA_VERSION);
    }
}

function bbsolverRequirePositiveInteger(value, label) {
    if (typeof value !== "number" || value <= 0 || Math.floor(value) !== value) {
        throw new Error(label + " must be a positive integer");
    }
    return value;
}

function bbsolverShapeFlatLength(values) {
    if (!values || values.length < 8) {
        return -1;
    }
    var vertexCount = Number(values[1]);
    var rounded = Math.round(vertexCount);
    if (Math.abs(vertexCount - rounded) > 0.000000001 || rounded <= 0) {
        return -1;
    }
    var expected = 2 + rounded * 6;
    return values.length === expected ? expected : -1;
}

function validateSampleBundleJson(bundle) {
    bbsolverRequireSchemaVersion(bundle, "SampleBundle");
    if (bundle._schema !== "samples") {
        throw new Error("SampleBundle _schema must be \"samples\"");
    }
    if (!bundle.request_id || typeof bundle.request_id !== "string") {
        throw new Error("SampleBundle request_id must be a string");
    }
    if (!bundle.comp || typeof bundle.comp !== "object") {
        throw new Error("SampleBundle comp must be an object");
    }
    bbsolverRequireNonEmptyArray(bundle.properties, "SampleBundle properties");
    if (!bundle.config || typeof bundle.config !== "object") {
        throw new Error("SampleBundle config must be an object");
    }

    for (var i = 0; i < bundle.properties.length; i++) {
        var propSamples = bundle.properties[i];
        if (!propSamples || typeof propSamples !== "object") {
            throw new Error("SampleBundle properties[" + i + "] must be an object");
        }
        if (!propSamples.property || typeof propSamples.property !== "object") {
            throw new Error("SampleBundle properties[" + i + "].property must be an object");
        }
        if (!propSamples.property.id || typeof propSamples.property.id !== "string") {
            throw new Error("SampleBundle properties[" + i + "].property.id must be a string");
        }
        var dimensions = bbsolverRequirePositiveInteger(
            propSamples.property.dimensions,
            "SampleBundle properties[" + i + "].property.dimensions");
        var samplesPerFrame =
            propSamples.samples_per_frame === undefined ||
            propSamples.samples_per_frame === null ? 1 : propSamples.samples_per_frame;
        bbsolverRequirePositiveInteger(
            samplesPerFrame,
            "SampleBundle properties[" + i + "].samples_per_frame");
        var expectedValueCount = dimensions * samplesPerFrame;
        var variableShapeFlat =
            propSamples.property.units_label === "shape_flat" &&
            propSamples.property.shape_variable_topology === true;
        bbsolverRequireNonEmptyArray(propSamples.samples, "SampleBundle properties[" + i + "].samples");
        for (var j = 0; j < propSamples.samples.length; j++) {
            var sample = propSamples.samples[j];
            if (typeof sample.t_sec !== "number") {
                throw new Error("SampleBundle properties[" + i + "].samples[" + j +
                    "].t_sec must be a number");
            }
            bbsolverRequireNonEmptyArray(
                sample.v, "SampleBundle properties[" + i + "].samples[" + j + "].v");
            if (variableShapeFlat) {
                var flatLength = bbsolverShapeFlatLength(sample.v);
                if (flatLength < 0 || flatLength > dimensions) {
                    throw new Error("SampleBundle properties[" + i + "].samples[" + j +
                        "].v must be a valid shape_flat vector not exceeding property dimensions");
                }
            } else if (sample.v.length !== expectedValueCount) {
                throw new Error("SampleBundle properties[" + i + "].samples[" + j +
                    "].v length must equal dimensions * samples_per_frame");
            }
        }
    }
    return true;
}

function validateKeyBundleJson(bundle) {
    bbsolverRequireSchemaVersion(bundle, "KeyBundle");
    if (bundle._schema !== "keys") {
        throw new Error("KeyBundle _schema must be \"keys\"");
    }
    bbsolverRequireNonEmptyArray(bundle.property_results, "KeyBundle property_results");
    for (var i = 0; i < bundle.property_results.length; i++) {
        var result = bundle.property_results[i];
        if (!result.property_id || typeof result.property_id !== "string") {
            throw new Error("KeyBundle property_results[" + i + "].property_id must be a string");
        }
        var dimensions = bbsolverRequirePositiveInteger(
            result.dimensions,
            "KeyBundle property_results[" + i + "].dimensions");
        bbsolverRequireArray(result.keys, "KeyBundle property_results[" + i + "].keys");
        if (result.keys.length === 0 && result.converged !== false) {
            throw new Error("KeyBundle property_results[" + i +
                "].keys must not be empty for converged results");
        }
        for (var j = 0; j < result.keys.length; j++) {
            var key = result.keys[j];
            if (typeof key.t_sec !== "number") {
                throw new Error("KeyBundle property_results[" + i + "].keys[" + j +
                    "].t_sec must be a number");
            }
            bbsolverRequireNonEmptyArray(
                key.v, "KeyBundle property_results[" + i + "].keys[" + j + "].v");
            if (key.v.length !== dimensions) {
                throw new Error("KeyBundle property_results[" + i + "].keys[" + j +
                    "].v length must equal property_results dimensions");
            }
        }
    }
    return true;
}

function writeSampleBundleJson(bundle, filepath) {
    validateSampleBundleJson(bundle);
    return bbsolverWriteTextFile(filepath, bbsolverStringifyJson(bundle));
}

function readSampleBundleJson(filepath) {
    var bundle = bbsolverParseJson(bbsolverReadTextFile(filepath), filepath);
    validateSampleBundleJson(bundle);
    return bundle;
}

function readKeyBundleJson(filepath) {
    var bundle = bbsolverParseJson(bbsolverReadTextFile(filepath), filepath);
    validateKeyBundleJson(bundle);
    return bundle;
}

// Short aliases kept for compact examples.
function hasJson() { return bbsolverHasJson(); }
function stringifyJson(value) { return bbsolverStringifyJson(value); }
function parseJson(text) { return bbsolverParseJson(text, "inline JSON"); }
function readTextFile(path) { return bbsolverReadTextFile(path); }
function writeTextFile(path, text) { return bbsolverWriteTextFile(path, text); }
