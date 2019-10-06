import json


def main() -> None:
    root = json.parse("{\"ok\":true}")
    value = json.get(root, "ok")
    print(json.as_string(value))
