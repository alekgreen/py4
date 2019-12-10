import json


class Config:
    mapping: dict[str, int]


def main() -> None:
    value = json.from_string[Config]("{}")
    print(value)
