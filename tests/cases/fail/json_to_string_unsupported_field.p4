import json


class Config:
    meta: dict[int, int]


def main() -> None:
    config = Config({1: 8080})
    print(json.to_string(config))
