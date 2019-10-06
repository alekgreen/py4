import json


def main() -> None:
    root = json.parse("{\"name\":\"py4\",\"active\":true,\"score\":3.5}")
    name = json.get(root, "name")
    active = json.get(root, "active")
    score = json.get(root, "score")

    assert json.is_object(root)
    assert json.is_string(name)
    assert json.is_bool(active)
    assert json.is_number(score)

    print(json.as_string(name))
    print(json.as_bool(active))
    print(json.as_float(score))
    print(json.stringify(root))
