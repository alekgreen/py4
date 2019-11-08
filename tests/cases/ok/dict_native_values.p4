import json

def main() -> None:
    data: dict[str, json.Value] = {}
    data["name"] = json.parse("\"py4\"")
    data["active"] = json.parse("true")

    name = data["name"]
    active = data.get_or("active", json.parse("false"))

    assert json.is_string(name)
    assert json.is_bool(active)

    print(json.as_string(name))
    print(json.as_bool(active))
