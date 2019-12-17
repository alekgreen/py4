#!/usr/bin/env python3

from __future__ import annotations

import json
from dataclasses import dataclass


@dataclass
class Owner:
    name: str
    team: str
    email: str | None


@dataclass
class Metric:
    name: str
    value: float
    active: bool
    tags: list[str]
    note: str | None


@dataclass
class Report:
    id: int
    title: str
    revision: int
    owner: Owner
    flags: list[bool]
    metrics: list[Metric]


def decode_owner(data: dict[str, object]) -> Owner:
    return Owner(
        name=str(data["name"]),
        team=str(data["team"]),
        email=None if data["email"] is None else str(data["email"]),
    )


def decode_metric(data: dict[str, object]) -> Metric:
    return Metric(
        name=str(data["name"]),
        value=float(data["value"]),
        active=bool(data["active"]),
        tags=[str(tag) for tag in data["tags"]],
        note=None if data["note"] is None else str(data["note"]),
    )


def decode_report(text: str) -> Report:
    data = json.loads(text)
    return Report(
        id=int(data["id"]),
        title=str(data["title"]),
        revision=int(data["revision"]),
        owner=decode_owner(data["owner"]),
        flags=[bool(flag) for flag in data["flags"]],
        metrics=[decode_metric(metric) for metric in data["metrics"]],
    )


def encode_report(report: Report) -> str:
    return json.dumps(
        {
            "id": report.id,
            "title": report.title,
            "revision": report.revision,
            "owner": {
                "name": report.owner.name,
                "team": report.owner.team,
                "email": report.owner.email,
            },
            "flags": report.flags,
            "metrics": [
                {
                    "name": metric.name,
                    "value": metric.value,
                    "active": metric.active,
                    "tags": metric.tags,
                    "note": metric.note,
                }
                for metric in report.metrics
            ],
        },
        separators=(",", ":"),
    )


def main() -> None:
    with open("benchmarks/json_roundtrip_input.json", "r", encoding="utf-8") as handle:
        text = handle.read()

    total = 0
    for _ in range(2000):
        report = decode_report(text)
        total += report.id
        total += report.revision
        total += len(report.owner.team)
        total += len(report.flags)
        total += len(report.metrics)
        total += len(report.metrics[0].name)
        total += len(report.metrics[0].tags)
        report.revision += 1
        encoded = encode_report(report)
        total += len(encoded)

    print(total)


if __name__ == "__main__":
    main()
