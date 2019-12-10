import json


class User:
    name: str


def main() -> None:
    user = json.from_string[User]("{}")
    print(user)
