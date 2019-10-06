import json


def main() -> None:
    root = json.parse("{\"name\":")
    print(json.stringify(root))
