import json

def main() -> None:
    data: dict[str, json.Value] = {
        "name": json.parse("\"py4\"")
    }
    print(data)
