#!/usr/bin/env python3
"""Source-level policy for the SampleBundle JSON refactor boundary.

IO3-IO6 moved SampleBundle parsing out of `io_json.cpp` into pure JSON-to-domain
modules. This policy keeps `io_json.cpp` as the file I/O facade and ensures
the extracted modules do not drift into diagnostics, progress, or filesystem
ownership.
"""

from __future__ import annotations

from pathlib import Path

from _solver_policy_paths import find_solver_layout


ROOT, SOLVER = find_solver_layout(__file__)
SOLVER_SRC = SOLVER / "src"
SOLVER_INCLUDE = SOLVER / "include" / "bbsolver"
SAMPLE_IO_DIR = SOLVER_SRC / "io"
SAMPLE_IO_INCLUDE_DIR = SOLVER_INCLUDE / "io"
IO_JSON_CPP = SAMPLE_IO_DIR / "io_json.cpp"
IO_JSON_HPP = SAMPLE_IO_INCLUDE_DIR / "io_json.hpp"
MAX_IO_JSON_LINES = 80
MAX_SAMPLE_MODULE_LINES = 200

SAMPLE_MODULES = (
    SAMPLE_IO_INCLUDE_DIR / "sample_json_value_io.hpp",
    SAMPLE_IO_DIR / "sample_json_value_io.cpp",
    SAMPLE_IO_INCLUDE_DIR / "sample_json_timing_io.hpp",
    SAMPLE_IO_DIR / "sample_json_timing_io.cpp",
    SAMPLE_IO_INCLUDE_DIR / "sample_property_io.hpp",
    SAMPLE_IO_DIR / "sample_property_io.cpp",
    SAMPLE_IO_INCLUDE_DIR / "sample_bundle_validation.hpp",
    SAMPLE_IO_DIR / "sample_bundle_validation.cpp",
    SAMPLE_IO_INCLUDE_DIR / "sample_bundle_io.hpp",
    SAMPLE_IO_DIR / "sample_bundle_io.cpp",
)

EXPECTED_EXPORTS = {
    "sample_json_value_io.hpp": (
        "ValueKind ParseSampleValueKindJson(const std::string& value);",
    ),
    "sample_json_timing_io.hpp": (
        "bool HasSampleKeyTimingJsonFields(const nlohmann::json& obj);",
        "KeyTiming ParseSampleKeyTimingJson(const nlohmann::json& obj);",
        "Sample ParseSampleJson(const nlohmann::json& obj);",
    ),
    "sample_property_io.hpp": (
        "CompInfo ParseCompInfoJson(const nlohmann::json& obj);",
        "LayerXform ParseLayerXformJson(const nlohmann::json& obj);",
        "PropertyInfo ParsePropertyInfoJson(const nlohmann::json& obj);",
        "PropertySamples ParsePropertySamplesJson(const nlohmann::json& obj);",
    ),
    "sample_bundle_io.hpp": (
        "void RequireSampleBundleJsonRoot(const nlohmann::json& root);",
        "SampleBundle ParseSampleBundleJson(const nlohmann::json& root);",
    ),
    "sample_bundle_validation.hpp": (
        "void RequirePropertySamplesJson(const nlohmann::json& property_json);",
    ),
}

IO_JSON_FORBIDDEN_SAMPLE_FIELDS = (
    '"schema_version"',
    '"request_id"',
    '"comp"',
    '"properties"',
    '"config"',
    '"property"',
    '"samples"',
    '"key_timing"',
    '"interp_in"',
    '"interp_out"',
    '"temporal_ease_in"',
    '"temporal_ease_out"',
    '"spatial_in"',
    '"spatial_out"',
    '"kind"',
    '"layer_xform_at_start"',
)

IO_JSON_FORBIDDEN_HELPERS = (
    "SampleBundle bundle",
    "PropertySamples property_samples",
    "PropertyInfo property",
    "CompInfo comp",
    "KeyTiming timing",
    "TemporalEase ease",
    "GetOr<",
    "GetDoubleVector(",
    "ParseCompInfo",
    "ParseLayerXform",
    "ParsePropertyInfo",
    "ParsePropertySamples",
    "ParseSampleJson",
    "ParseSampleKeyTimingJson",
    "ParseSampleValueKindJson",
)

PURE_MODULE_FORBIDDEN_TOKENS = (
    "#include <filesystem>",
    "#include <fstream>",
    "std::filesystem",
    "std::ifstream",
    "std::ofstream",
    "DiagnosticsWriter",
    "solver_diagnostics",
    "diagnostics.Emit",
    "ProgressWriter",
    "ProgressEvent",
    "WriteProgress",
    "EmitProgress",
)


def _text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def _line_count(path: Path) -> int:
    return len(_text(path).splitlines())


def test_io_json_stays_sample_file_facade_only() -> None:
    text = _text(IO_JSON_CPP)

    assert '#include "bbsolver/io/sample_bundle_io.hpp"' in text, (
        "io_json.cpp must delegate SampleBundle parsing through sample_bundle_io"
    )
    assert "return ParseSampleBundleJson(root);" in text, (
        "ReadSampleBundleJson must delegate parsed JSON to ParseSampleBundleJson"
    )
    assert "std::ifstream input(path);" in text, (
        "io_json.cpp should remain the file-open facade for sample/key JSON"
    )

    for token in IO_JSON_FORBIDDEN_SAMPLE_FIELDS:
        assert token not in text, (
            f"io_json.cpp must not parse SampleBundle field {token}; "
            "keep that in sample JSON modules"
        )

    for token in IO_JSON_FORBIDDEN_HELPERS:
        assert token not in text, (
            f"io_json.cpp must not own SampleBundle parse helper {token!r}"
        )


def test_io_json_stays_facade_sized() -> None:
    lines = _line_count(IO_JSON_CPP)
    assert lines <= MAX_IO_JSON_LINES, (
        f"{_rel(IO_JSON_CPP)} has {lines} lines; SampleBundle parsing must "
        f"stay extracted and the facade cap is {MAX_IO_JSON_LINES}"
    )


def test_public_io_json_header_stays_file_api_only() -> None:
    text = _text(IO_JSON_HPP)
    assert "SampleBundle ReadSampleBundleJson(const std::filesystem::path& path);" in text
    assert "KeyBundle ReadKeyBundleJson(const std::filesystem::path& path);" in text
    assert "void WriteKeyBundleJson(" in text
    assert "ParseSampleBundleJson" not in text
    assert "ParseSampleJson" not in text
    assert "ParsePropertySamplesJson" not in text


def test_sample_json_modules_export_expected_surface() -> None:
    for header_name, declarations in EXPECTED_EXPORTS.items():
        header_text = _text(SAMPLE_IO_INCLUDE_DIR / header_name)
        assert "#pragma once" in header_text
        for declaration in declarations:
            assert declaration in header_text, (
                f"{header_name} missing expected export {declaration!r}"
            )


def test_sample_json_modules_stay_compact() -> None:
    for path in SAMPLE_MODULES:
        lines = _line_count(path)
        assert lines <= MAX_SAMPLE_MODULE_LINES, (
            f"{_rel(path)} has {lines} lines; split the sample JSON boundary "
            f"before exceeding the compact-module cap {MAX_SAMPLE_MODULE_LINES}"
        )


def test_sample_json_modules_stay_pure() -> None:
    for path in SAMPLE_MODULES:
        text = _text(path)
        for token in PURE_MODULE_FORBIDDEN_TOKENS:
            assert token not in text, (
                f"{_rel(path)} must remain pure JSON/domain conversion; "
                f"found {token!r}"
            )


def test_sample_json_headers_use_hpp() -> None:
    legacy_headers = sorted(SAMPLE_IO_INCLUDE_DIR.glob("sample*_io.h"))
    assert not legacy_headers, (
        "sample JSON Phase 3 headers must use .hpp: "
        + ", ".join(_rel(path) for path in legacy_headers)
    )


def main() -> int:
    tests = [
        test_io_json_stays_sample_file_facade_only,
        test_io_json_stays_facade_sized,
        test_public_io_json_header_stays_file_api_only,
        test_sample_json_modules_export_expected_surface,
        test_sample_json_modules_stay_compact,
        test_sample_json_modules_stay_pure,
        test_sample_json_headers_use_hpp,
    ]
    for test in tests:
        test()
        print(f"[PASS] {test.__name__}")
    print(f"summary: {len(tests)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
