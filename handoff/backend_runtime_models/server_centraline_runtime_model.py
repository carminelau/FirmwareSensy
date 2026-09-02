"""Flask route for per-Sensy, per-pollutant runtime models.

Integration target: server_centraline/app.py
Database: SSDB
Historical collection: centraline_minute_avg

The names of the historical MongoDB fields are isolated in HISTORY_FIELDS.
Confirm them against one real centraline_minute_avg document before deployment.
"""

from __future__ import annotations

import hashlib
import json
import math
import re
import time
from copy import deepcopy
from datetime import datetime, timezone
from typing import Any

from flask import current_app, jsonify, request
from pymongo import ASCENDING
from pymongo.database import Database

from runtime_equation_parser import build_runtime_model_payload
from runtime_model_manifest_importer import runtime_payload_from_manifest


MODEL_COLLECTION = "sensy_runtime_models"
HISTORY_COLLECTION = "centraline_minute_avg"
MAX_BOOTSTRAP_SAMPLES = 64
MAX_HISTORY_DOCUMENTS = 1000

# Change only this mapping if centraline_minute_avg uses different field names.
HISTORY_FIELDS = {
    "id": "ID",
    "timestamp": "timestamp",
    "timestamp_is_datetime": False,
    "no2_raw_candidates": ("multigas_no2_raw", "Multigas NO2 [raw]"),
    "voc_raw_candidates": ("multigas_voc_raw", "Multigas VOC [raw]"),
}

SAFE_ID = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
SAFE_POLLUTANT = re.compile(r"^[A-Za-z0-9_]{1,32}$")


def _model_fingerprint(model_payload: dict[str, Any]) -> str:
    """Return a stable cache fingerprint; it is not a stored model revision."""

    canonical_model = deepcopy(model_payload)
    for metadata_field in ("_id", "ID", "pollutant", "model_id", "updated_at"):
        canonical_model.pop(metadata_field, None)

    serialized = json.dumps(
        canonical_model,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=True,
    )
    digest = hashlib.sha256(serialized.encode("utf-8")).hexdigest()[:16]
    return f"sha256-{digest}"


def _first_present(document: dict[str, Any], field_names: tuple[str, ...]) -> Any:
    for field_name in field_names:
        if field_name in document and document[field_name] is not None:
            return document[field_name]
    return None


def _to_epoch(value: Any) -> int | None:
    if isinstance(value, datetime):
        if value.tzinfo is None:
            value = value.replace(tzinfo=timezone.utc)
        return int(value.timestamp())
    if isinstance(value, (int, float)) and math.isfinite(value):
        return int(value)
    return None


def _finite_number(value: Any) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    numeric = float(value)
    return numeric if math.isfinite(numeric) else None


def _history_cutoff_value(epoch: int) -> int | datetime:
    if HISTORY_FIELDS["timestamp_is_datetime"]:
        return datetime.fromtimestamp(epoch, tz=timezone.utc)
    return epoch


def build_history_bootstrap(
    ssdb: Database,
    sensy_id: str,
    model: dict[str, Any],
    *,
    now_epoch: int | None = None,
) -> dict[str, Any]:
    """Return downsampled raw history compatible with the firmware ring buffer."""

    history_config = model.get("history") or {}
    sample_period = int(history_config.get("sample_period_seconds", 300))
    lag_2 = int(history_config.get("lag_2_seconds", 7200))
    rolling_window = int(history_config.get("rolling_window_seconds", 10800))
    tolerance = int(history_config.get("lag_tolerance_seconds", 450))

    if sample_period <= 0:
        raise ValueError("history.sample_period_seconds must be positive")

    now_epoch = int(now_epoch or time.time())
    required_history_seconds = max(lag_2, rolling_window) + tolerance
    cutoff_epoch = now_epoch - required_history_seconds

    timestamp_field = str(HISTORY_FIELDS["timestamp"])
    id_field = str(HISTORY_FIELDS["id"])
    no2_candidates = tuple(HISTORY_FIELDS["no2_raw_candidates"])
    voc_candidates = tuple(HISTORY_FIELDS["voc_raw_candidates"])

    projection = {"_id": 0, timestamp_field: 1}
    projection.update({field: 1 for field in no2_candidates})
    projection.update({field: 1 for field in voc_candidates})

    cursor = (
        ssdb[HISTORY_COLLECTION]
        .find(
            {
                id_field: sensy_id,
                timestamp_field: {"$gte": _history_cutoff_value(cutoff_epoch)},
            },
            projection,
        )
        .sort(timestamp_field, ASCENDING)
        .limit(MAX_HISTORY_DOCUMENTS)
    )

    # Keep the latest measurement in each configured time bucket.
    buckets: dict[int, dict[str, Any]] = {}
    for document in cursor:
        timestamp = _to_epoch(document.get(timestamp_field))
        no2_raw = _finite_number(_first_present(document, no2_candidates))
        voc_raw = _finite_number(_first_present(document, voc_candidates))
        if timestamp is None or no2_raw is None or voc_raw is None:
            continue
        if timestamp < cutoff_epoch or timestamp > now_epoch:
            continue

        bucket = timestamp // sample_period
        buckets[bucket] = {
            "timestamp": timestamp,
            "multigas_no2_raw": no2_raw,
            "multigas_voc_raw": voc_raw,
        }

    samples = sorted(buckets.values(), key=lambda sample: sample["timestamp"])
    samples = samples[-MAX_BOOTSTRAP_SAMPLES:]

    return {
        "source": "SSDB.centraline_minute_avg",
        "sample_period_seconds": sample_period,
        "samples": samples,
    }


def _public_runtime_model(model: dict[str, Any]) -> dict[str, Any]:
    model = deepcopy(model)
    model.pop("ID", None)
    model.pop("updated_at", None)
    model.pop("source_model", None)
    return model


def get_runtime_model_document(
    ssdb: Database,
    sensy_id: str,
    pollutant: str,
) -> dict[str, Any] | None:
    """Load one equation selected by Sensy ID and target pollutant."""

    model = ssdb[MODEL_COLLECTION].find_one(
        {"ID": sensy_id, "pollutant": pollutant},
        {"_id": 0},
    )
    return None if model is None else _public_runtime_model(model)


def get_runtime_model_documents(
    ssdb: Database,
    sensy_id: str,
) -> list[dict[str, Any]]:
    """Load authoritative current model set for one Sensy."""

    cursor = ssdb[MODEL_COLLECTION].find({"ID": sensy_id}, {"_id": 0}).sort(
        "pollutant", ASCENDING
    )
    return [_public_runtime_model(model) for model in cursor]


def upsert_runtime_model(
    ssdb: Database,
    sensy_id: str,
    pollutant: str,
    model_payload: dict[str, Any],
) -> str:
    """Create or overwrite the single current model for (ID, pollutant)."""

    if not SAFE_ID.fullmatch(sensy_id):
        raise ValueError("invalid Sensy ID")
    if not SAFE_POLLUTANT.fullmatch(pollutant):
        raise ValueError("invalid pollutant")
    if model_payload.get("schema_version") != 1:
        raise ValueError("schema_version must be 1")

    document = deepcopy(model_payload)
    document["ID"] = sensy_id
    document["pollutant"] = pollutant
    document["model_id"] = _model_fingerprint(document)
    document["updated_at"] = datetime.now(tz=timezone.utc)

    ssdb[MODEL_COLLECTION].replace_one(
        {"ID": sensy_id, "pollutant": pollutant},
        document,
        upsert=True,
    )
    return document["model_id"]


def load_runtime_equation(
    ssdb: Database,
    sensy_id: str,
    pollutant: str,
    equation: str,
    *,
    output_unit: str,
    elapsed_days_origin_epoch: int,
    no2_raw_scale: float,
    voc_raw_scale: float,
    sample_period_seconds: int = 300,
) -> str:
    """Convert one equation string and overwrite its current MongoDB model."""

    model_payload = build_runtime_model_payload(
        equation,
        output_field=pollutant.lower(),
        output_unit=output_unit,
        elapsed_days_origin_epoch=elapsed_days_origin_epoch,
        no2_raw_scale=no2_raw_scale,
        voc_raw_scale=voc_raw_scale,
        sample_period_seconds=sample_period_seconds,
    )
    return upsert_runtime_model(ssdb, sensy_id, pollutant.lower(), model_payload)


def load_runtime_model_from_manifest(
    ssdb: Database,
    manifest_path: str,
    sensy_id: str,
    pollutant: str,
    *,
    allow_rejected: bool = False,
) -> str:
    """Select from modelling manifest and overwrite one current MongoDB model."""

    model_payload = runtime_payload_from_manifest(
        manifest_path,
        sensy_id,
        pollutant,
        allow_rejected=allow_rejected,
    )
    return upsert_runtime_model(ssdb, sensy_id, pollutant.lower(), model_payload)


def ensure_runtime_model_indexes(ssdb: Database) -> None:
    """Run once during a controlled deployment, not inside every HTTP request."""

    ssdb[MODEL_COLLECTION].create_index(
        [("ID", ASCENDING), ("pollutant", ASCENDING)],
        unique=True,
        name="runtime_model_by_sensy_pollutant",
    )
    ssdb[HISTORY_COLLECTION].create_index(
        [
            (str(HISTORY_FIELDS["id"]), ASCENDING),
            (str(HISTORY_FIELDS["timestamp"]), ASCENDING),
        ],
        name="runtime_history_by_sensy_time",
    )


def register_runtime_model_route(app: Any, ssdb: Database) -> None:
    """Register bulk route plus backward-compatible single-model route."""

    @app.route("/get_runtime_models", methods=["GET"])
    def get_runtime_models():
        sensy_id = (request.args.get("ID") or "").strip()
        if not SAFE_ID.fullmatch(sensy_id):
            return jsonify(error="invalid_ID"), 400

        try:
            models = get_runtime_model_documents(ssdb, sensy_id)
            if len(models) > 16:
                current_app.logger.error(
                    "Too many runtime models ID=%s count=%d", sensy_id, len(models)
                )
                return jsonify(error="too_many_runtime_models"), 500
            for model in models:
                pollutant = str(model.get("pollutant") or "").lower()
                if (
                    not SAFE_POLLUTANT.fullmatch(pollutant)
                    or model.get("schema_version") != 1
                    or not model.get("model_id")
                ):
                    current_app.logger.error(
                        "Invalid runtime model in bulk response ID=%s pollutant=%s",
                        sensy_id,
                        pollutant,
                    )
                    return jsonify(error="invalid_runtime_model"), 500
                model["pollutant"] = pollutant

            models_by_pollutant = {}
            for model in models:
                pollutant = model.pop("pollutant")
                if pollutant in models_by_pollutant:
                    current_app.logger.error(
                        "Duplicate runtime model ID=%s pollutant=%s",
                        sensy_id,
                        pollutant,
                    )
                    return jsonify(error="duplicate_runtime_model"), 500
                models_by_pollutant[pollutant] = model

            response = jsonify(models_by_pollutant)
            response.headers["Cache-Control"] = "no-store"
            return response, 200
        except (KeyError, TypeError, ValueError):
            current_app.logger.exception(
                "Runtime models contract error ID=%s", sensy_id
            )
            return jsonify(error="runtime_models_contract_error"), 500
        except Exception:
            current_app.logger.exception(
                "Runtime models database error ID=%s", sensy_id
            )
            return jsonify(error="runtime_models_unavailable"), 503

    @app.route("/get_runtime_model", methods=["GET"])
    def get_runtime_model():
        sensy_id = (request.args.get("ID") or "").strip()
        pollutant = (request.args.get("Pollutant") or "").strip().lower()

        if not SAFE_ID.fullmatch(sensy_id):
            return jsonify(error="invalid_ID"), 400
        if not SAFE_POLLUTANT.fullmatch(pollutant):
            return jsonify(error="invalid_Pollutant"), 400

        try:
            model = get_runtime_model_document(ssdb, sensy_id, pollutant)
            if model is None:
                return jsonify(error="runtime_model_not_found"), 404
            if model.get("schema_version") != 1 or not model.get("model_id"):
                current_app.logger.error(
                    "Invalid runtime model ID=%s pollutant=%s", sensy_id, pollutant
                )
                return jsonify(error="invalid_runtime_model"), 500

            model["pollutant"] = pollutant
            if model.get("enabled", False):
                model["history_bootstrap"] = build_history_bootstrap(
                    ssdb, sensy_id, model
                )

            response = jsonify(model)
            response.headers["Cache-Control"] = "no-store"
            return response, 200
        except (KeyError, TypeError, ValueError):
            current_app.logger.exception(
                "Runtime model contract error ID=%s pollutant=%s",
                sensy_id,
                pollutant,
            )
            return jsonify(error="runtime_model_contract_error"), 500
        except Exception:
            current_app.logger.exception(
                "Runtime model database error ID=%s pollutant=%s",
                sensy_id,
                pollutant,
            )
            return jsonify(error="runtime_model_unavailable"), 503
