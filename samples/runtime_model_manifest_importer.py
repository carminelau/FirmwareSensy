"""Import deployable Multigas equations from modelling manifest into firmware JSON."""

from __future__ import annotations

import json
import math
import re
from pathlib import Path
from typing import Any

from runtime_equation_parser import (
    FEATURE_TO_COEFFICIENT,
    REQUIRED_COEFFICIENTS,
    build_runtime_model_payload_from_coefficients,
)


DEFAULT_TRAINING_ORIGIN_EPOCH = 1785916800  # 2026-08-05 08:00:00 UTC
DEFAULT_OUTPUT_UNITS = {
    "NO2": "ug/m3",
    "CO": "mg/m3",
    "C6H6": "ug/m3",
}
TEMPERATURE_HINGE = re.compile(
    r"^max\(0, Temperature \[°C\] - (?P<threshold>\d+(?:\.\d+)?)\)$"
)

MANIFEST_FEATURE_TO_COEFFICIENT = {
    feature: coefficient
    for feature, coefficient in FEATURE_TO_COEFFICIENT.items()
    if feature != "__temperature_above_threshold__"
}


def read_model_manifest(manifest_path: str | Path) -> dict[str, Any]:
    manifest = json.loads(Path(manifest_path).read_text(encoding="utf-8"))
    if not isinstance(manifest.get("models"), list):
        raise ValueError("manifest.models must be a list")
    return manifest


def select_manifest_model(
    manifest: dict[str, Any],
    sensy_id: str,
    pollutant: str,
    *,
    sensor_family: str = "Multigas",
) -> dict[str, Any]:
    """Select exactly one standalone, best-deployable equation."""

    pollutant = pollutant.upper()
    matches = [
        model
        for model in manifest["models"]
        if model.get("device") == sensy_id
        and str(model.get("pollutant", "")).upper() == pollutant
        and model.get("sensor_family") == sensor_family
        and model.get("mode") == "Standalone sensor-only"
        and model.get("selection") == "Best deployable equation"
    ]
    if len(matches) != 1:
        raise ValueError(
            f"expected one deployable model for ID={sensy_id} Pollutant={pollutant}; "
            f"found {len(matches)}"
        )
    return matches[0]


def manifest_model_to_runtime_payload(
    model: dict[str, Any],
    *,
    allow_rejected: bool = False,
    elapsed_days_origin_epoch: int = DEFAULT_TRAINING_ORIGIN_EPOCH,
) -> dict[str, Any]:
    """Convert exact manifest coefficients; reject unsupported firmware features."""

    status = str(model.get("status", ""))
    if status == "Rejected" and not allow_rejected:
        raise ValueError("refusing Rejected model; pass allow_rejected=True explicitly")
    if model.get("prediction_transform") != "direct":
        raise ValueError("only prediction_transform=direct is supported")

    manifest_coefficients = model.get("coefficients")
    if not isinstance(manifest_coefficients, dict):
        raise ValueError("model.coefficients must be an object")

    internal_coefficients = {name: 0.0 for name in REQUIRED_COEFFICIENTS}
    intercept = model.get("intercept")
    if not isinstance(intercept, (int, float)) or not math.isfinite(intercept):
        raise ValueError("model.intercept must be finite")
    internal_coefficients["intercept"] = float(intercept)

    temperature_threshold_c = 30.0
    unsupported_features: list[str] = []
    for feature, value in manifest_coefficients.items():
        coefficient_name = MANIFEST_FEATURE_TO_COEFFICIENT.get(feature)
        if coefficient_name is None:
            hinge_match = TEMPERATURE_HINGE.fullmatch(feature)
            if hinge_match:
                coefficient_name = "temperature_above_threshold"
                temperature_threshold_c = float(hinge_match.group("threshold"))
            else:
                unsupported_features.append(feature)
                continue
        if not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError(f"non-finite coefficient for {feature}")
        internal_coefficients[coefficient_name] = float(value)

    if unsupported_features:
        raise ValueError(
            "features unsupported by current firmware: "
            + ", ".join(sorted(unsupported_features))
        )

    pollutant = str(model.get("pollutant", "")).upper()
    output_unit = DEFAULT_OUTPUT_UNITS.get(pollutant)
    if output_unit is None:
        raise ValueError(f"unknown output unit for pollutant {pollutant}")

    payload = build_runtime_model_payload_from_coefficients(
        internal_coefficients,
        temperature_threshold_c=temperature_threshold_c,
        output_field=pollutant.lower(),
        output_unit=output_unit,
        elapsed_days_origin_epoch=elapsed_days_origin_epoch,
        no2_raw_scale=1.0,
        voc_raw_scale=1.0,
        sample_period_seconds=3600,
    )
    payload["source_model"] = {
        "device": model.get("device"),
        "pollutant": pollutant,
        "sensor_family": model.get("sensor_family"),
        "mode": model.get("mode"),
        "selection": model.get("selection"),
        "status": status,
        "algorithm": model.get("algorithm"),
        "model_file": model.get("model_file"),
        "equation": model.get("equation"),
    }
    return payload


def runtime_payload_from_manifest(
    manifest_path: str | Path,
    sensy_id: str,
    pollutant: str,
    *,
    allow_rejected: bool = False,
) -> dict[str, Any]:
    manifest = read_model_manifest(manifest_path)
    model = select_manifest_model(manifest, sensy_id, pollutant)
    return manifest_model_to_runtime_payload(model, allow_rejected=allow_rejected)
