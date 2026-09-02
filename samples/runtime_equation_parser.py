"""Strict converter from the human-readable regression equation to model JSON."""

from __future__ import annotations

import html
import math
import re
from typing import Any


NUMBER = r"(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"

FEATURE_TO_COEFFICIENT = {
    "Multigas NO2 [raw] [t-1h]": "no2_raw_lag_1",
    "Multigas NO2 [raw] [t-2h]": "no2_raw_lag_2",
    "Multigas NO2 [raw] [rolling3h]": "no2_raw_rolling",
    "Multigas VOC [raw] [t-1h]": "voc_raw_lag_1",
    "Multigas VOC [raw] [t-2h]": "voc_raw_lag_2",
    "Multigas VOC [raw] [rolling3h]": "voc_raw_rolling",
    "(Multigas NO2 [raw])²": "no2_raw_squared",
    "(Multigas NO2 [raw])×(Temperature [°C])": "no2_raw_temperature",
    "(Multigas NO2 [raw])×(Humidity [%])": "no2_raw_humidity",
    "(Multigas VOC [raw])²": "voc_raw_squared",
    "(Multigas VOC [raw])×(Temperature [°C])": "voc_raw_temperature",
    "(Multigas VOC [raw])×(Humidity [%])": "voc_raw_humidity",
    "__temperature_above_threshold__": "temperature_above_threshold",
    "Multigas NO2 [raw]": "no2_raw",
    "Multigas VOC [raw]": "voc_raw",
    "Temperature [°C]": "temperature",
    "Humidity [%]": "humidity",
    "Elapsed days": "elapsed_days",
}

REQUIRED_COEFFICIENTS = frozenset({"intercept", *FEATURE_TO_COEFFICIENT.values()})
SAFE_OUTPUT_FIELD = re.compile(r"^[A-Za-z][A-Za-z0-9_]{0,31}$")


def _normalize_equation(equation: str) -> str:
    normalized = html.unescape(equation).strip()
    normalized = normalized.replace("−", "-").replace("–", "-")
    normalized = normalized.replace("·", "×").replace("*", "×")
    normalized = normalized.replace("^2", "²")
    return re.sub(r"\s+", " ", normalized)


def parse_equation_string(equation: str) -> dict[str, Any]:
    """Parse only the supported fixed feature vocabulary; never execute input."""

    if not isinstance(equation, str) or not equation.strip():
        raise ValueError("equation must be a non-empty string")

    normalized = _normalize_equation(equation)
    if "=" not in normalized:
        raise ValueError("equation must contain =")
    _, right_hand_side = normalized.split("=", 1)
    right_hand_side = right_hand_side.strip()

    thresholds: list[float] = []
    hinge_pattern = re.compile(
        rf"max\(\s*0\s*,\s*Temperature\s*\[°C\]\s*-\s*(?P<value>{NUMBER})\s*\)",
        re.IGNORECASE,
    )

    def replace_hinge(match: re.Match[str]) -> str:
        thresholds.append(float(match.group("value")))
        return "__temperature_above_threshold__"

    right_hand_side = hinge_pattern.sub(replace_hinge, right_hand_side)
    if len(thresholds) != 1 or not math.isfinite(thresholds[0]):
        raise ValueError("equation must contain one temperature max() threshold")

    intercept_match = re.match(rf"(?P<value>[+-]?{NUMBER})", right_hand_side)
    if intercept_match is None:
        raise ValueError("equation intercept is missing or invalid")

    coefficients: dict[str, float] = {
        "intercept": float(intercept_match.group("value"))
    }
    position = intercept_match.end()

    feature_pattern = "|".join(
        re.escape(feature)
        for feature in sorted(FEATURE_TO_COEFFICIENT, key=len, reverse=True)
    )
    term_pattern = re.compile(
        rf"\s*(?P<sign>[+-])\s*(?P<value>{NUMBER})\s*×\s*"
        rf"(?P<feature>{feature_pattern})"
    )

    while position < len(right_hand_side):
        term_match = term_pattern.match(right_hand_side, position)
        if term_match is None:
            fragment = right_hand_side[position : position + 80]
            raise ValueError(f"unsupported equation term near: {fragment!r}")

        coefficient_name = FEATURE_TO_COEFFICIENT[term_match.group("feature")]
        if coefficient_name in coefficients:
            raise ValueError(f"duplicate equation term: {coefficient_name}")

        coefficient = float(term_match.group("value"))
        if term_match.group("sign") == "-":
            coefficient = -coefficient
        if not math.isfinite(coefficient):
            raise ValueError(f"non-finite coefficient: {coefficient_name}")

        coefficients[coefficient_name] = coefficient
        position = term_match.end()

    missing = sorted(REQUIRED_COEFFICIENTS - coefficients.keys())
    extra = sorted(coefficients.keys() - REQUIRED_COEFFICIENTS)
    if missing or extra:
        raise ValueError(f"coefficient mismatch; missing={missing}, extra={extra}")

    return {
        "temperature_threshold_c": thresholds[0],
        "coefficients": coefficients,
    }


def build_runtime_model_payload(
    equation: str,
    *,
    output_field: str,
    output_unit: str,
    elapsed_days_origin_epoch: int,
    no2_raw_scale: float,
    voc_raw_scale: float,
    no2_raw_offset: float = 0.0,
    voc_raw_offset: float = 0.0,
    sample_period_seconds: int = 300,
    refresh_after_seconds: int = 21600,
    enabled: bool = True,
) -> dict[str, Any]:
    """Combine parsed coefficients with metadata absent from equation text."""

    parsed = parse_equation_string(equation)
    return build_runtime_model_payload_from_coefficients(
        parsed["coefficients"],
        temperature_threshold_c=parsed["temperature_threshold_c"],
        output_field=output_field,
        output_unit=output_unit,
        elapsed_days_origin_epoch=elapsed_days_origin_epoch,
        no2_raw_scale=no2_raw_scale,
        voc_raw_scale=voc_raw_scale,
        no2_raw_offset=no2_raw_offset,
        voc_raw_offset=voc_raw_offset,
        sample_period_seconds=sample_period_seconds,
        refresh_after_seconds=refresh_after_seconds,
        enabled=enabled,
    )


def build_runtime_model_payload_from_coefficients(
    coefficients: dict[str, float],
    *,
    temperature_threshold_c: float,
    output_field: str,
    output_unit: str,
    elapsed_days_origin_epoch: int,
    no2_raw_scale: float,
    voc_raw_scale: float,
    no2_raw_offset: float = 0.0,
    voc_raw_offset: float = 0.0,
    sample_period_seconds: int = 300,
    refresh_after_seconds: int = 21600,
    enabled: bool = True,
) -> dict[str, Any]:
    """Build firmware JSON from already structured internal coefficients."""

    if not SAFE_OUTPUT_FIELD.fullmatch(output_field):
        raise ValueError("invalid output_field")
    numeric_values = (
        no2_raw_scale,
        voc_raw_scale,
        no2_raw_offset,
        voc_raw_offset,
    )
    if not all(isinstance(value, (int, float)) and math.isfinite(value) for value in numeric_values):
        raise ValueError("raw scales and offsets must be finite numbers")
    if no2_raw_scale == 0 or voc_raw_scale == 0:
        raise ValueError("raw scales must be non-zero")
    if elapsed_days_origin_epoch <= 0:
        raise ValueError("elapsed_days_origin_epoch must be a positive epoch")
    if sample_period_seconds <= 0:
        raise ValueError("sample_period_seconds must be positive")
    if sample_period_seconds > 10800 or 10800 % sample_period_seconds != 0:
        raise ValueError("sample_period_seconds must divide the 3-hour window")
    if not isinstance(temperature_threshold_c, (int, float)) or not math.isfinite(
        temperature_threshold_c
    ):
        raise ValueError("temperature_threshold_c must be finite")

    coefficient_names = set(coefficients)
    if coefficient_names != REQUIRED_COEFFICIENTS:
        missing = sorted(REQUIRED_COEFFICIENTS - coefficient_names)
        extra = sorted(coefficient_names - REQUIRED_COEFFICIENTS)
        raise ValueError(f"coefficient mismatch; missing={missing}, extra={extra}")
    if not all(
        isinstance(value, (int, float)) and math.isfinite(value)
        for value in coefficients.values()
    ):
        raise ValueError("coefficients must be finite numbers")

    return {
        "schema_version": 1,
        "enabled": bool(enabled),
        "refresh_after_seconds": int(refresh_after_seconds),
        "output": {
            "field": output_field,
            "unit": output_unit,
            "clamp_min": None,
            "clamp_max": None,
        },
        "inputs": {
            "multigas_no2_raw": {
                "source": "GM102B",
                "scale": float(no2_raw_scale),
                "offset": float(no2_raw_offset),
            },
            "multigas_voc_raw": {
                "source": "GM502B",
                "scale": float(voc_raw_scale),
                "offset": float(voc_raw_offset),
            },
            "temperature_source": "published_temperatura",
            "humidity_source": "published_umidita",
            "elapsed_days_origin_epoch": int(elapsed_days_origin_epoch),
        },
        "history": {
            "sample_period_seconds": int(sample_period_seconds),
            "lag_1_seconds": 3600,
            "lag_2_seconds": 7200,
            "rolling_window_seconds": max(7200, 10800 - int(sample_period_seconds)),
            "lag_tolerance_seconds": min(
                3599, max(450, int(sample_period_seconds // 4))
            ),
            "minimum_rolling_samples": max(1, 10800 // sample_period_seconds),
            "require_full_rolling_window": True,
        },
        "temperature_threshold_c": float(temperature_threshold_c),
        "coefficients": {name: float(value) for name, value in coefficients.items()},
    }
