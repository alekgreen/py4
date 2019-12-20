import json


class Config:
    mapping: dict[int, int]


def main() -> None:
    value = json.from_string[Config]("{}")
    print(value)
