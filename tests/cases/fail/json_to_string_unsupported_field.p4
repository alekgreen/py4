import json


class Config:
    meta: dict[str, int]


def main() -> None:
    config = Config({"port": 8080})
    print(json.to_string(config))
