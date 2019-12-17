import io
import json


class Owner:
    name: str
    team: str
    email: str | None


class Metric:
    name: str
    value: float
    active: bool
    tags: list[str]
    note: str | None


class Report:
    id: int
    title: str
    revision: int
    owner: Owner
    flags: list[bool]
    metrics: list[Metric]


def main() -> None:
    text = io.read_text("benchmarks/json_roundtrip_input.json")
    total = 0
    i = 0

    while i < 2000:
        report = json.from_string[Report](text)
        total = total + report.id
        total = total + report.revision
        total = total + len(report.owner.team)
        total = total + len(report.flags)
        total = total + len(report.metrics)
        total = total + len(report.metrics[0].name)
        total = total + len(report.metrics[0].tags)

        report.revision = report.revision + 1
        encoded = json.to_string(report)
        total = total + len(encoded)
        i = i + 1

    print(total)
