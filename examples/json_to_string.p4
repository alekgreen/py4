import json


class Meta:
    label: str


class User:
    name: str
    tags: list[str]
    meta: Meta | None


def main() -> None:
    user = User("ann", ["x", "y"], Meta("core"))
    print(json.to_string(user))
