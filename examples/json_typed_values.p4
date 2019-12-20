import json


class User:
    name: str
    age: int


def main() -> None:
    users = json.from_string[list[User]]("[{\"name\":\"ann\",\"age\":3},{\"name\":\"bob\",\"age\":4}]")
    pair = json.from_string[(str, int)]("[\"team\",2]")
    scores = json.from_string[dict[str, int]]("{\"a\":1,\"b\":2}")
    maybe = json.from_string[str | None]("null")
    maybe2 = json.from_string[str | None]("\"ok\"")

    print(users)
    print(pair)
    print(scores)
    print((maybe, maybe2))
    print(json.to_string(users))
    print(json.to_string(pair))
    print(json.to_string(scores))
