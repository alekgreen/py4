import json


class Data:
    name: str | None
    score: float | None


def main() -> None:
    first = json.from_string[Data]("{\"name\":null,\"score\":3.5}")
    second = json.from_string[Data]("{\"name\":\"bob\",\"score\":null}")
    print(first)
    print(second)
    print(json.to_string(first))
    print(json.to_string(second))
