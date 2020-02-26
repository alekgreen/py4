#!/usr/bin/env python3

from __future__ import annotations

from pydantic import BaseModel

ROUNDS = 3000


class Owner(BaseModel):
    name: str
    team: str
    email: str | None


class Metric(BaseModel):
    name: str
    value: float
    active: bool
    tags: list[str]
    note: str | None


class Report(BaseModel):
    id: int
    title: str
    revision: int
    owner: Owner
    flags: list[bool]
    metrics: list[Metric]


def main() -> None:
    with open("benchmarks/json_roundtrip_input.json", "r", encoding="utf-8") as handle:
        text = handle.read()

    total = 0
    for _ in range(ROUNDS):
        report = Report.model_validate_json(text)
        total += report.id
        total += report.revision
        total += len(report.owner.team)
        total += len(report.flags)
        total += len(report.metrics)
        total += len(report.metrics[0].name)
        total += len(report.metrics[0].tags)
        report.revision += 1
        encoded = report.model_dump_json()
        total += len(encoded)

    print(total)


if __name__ == "__main__":
    main()
