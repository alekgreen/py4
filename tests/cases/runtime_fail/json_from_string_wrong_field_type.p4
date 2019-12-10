import json


class User:
    name: str


def main() -> None:
    user = json.from_string[User]("{\"name\":true}")
    print(user)
