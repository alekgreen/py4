import json


def main() -> None:
    root = json.parse("{\"n\":\"py4\",\"a\":true,\"s\":3.5,\"t\":[\"c\",\"p\"]}")
    name = json.get(root, "n")
    active = json.get(root, "a")
    score = json.get(root, "s")
    tags = json.get(root, "t")

    assert json.is_object(root)
    assert json.is_string(name)
    assert json.is_bool(active)
    assert json.is_number(score)
    assert json.is_array(tags)
    assert json.has(root, "a")

    print(json.as_string(name))
    print(json.as_bool(active))
    print(json.as_float(score))
    print(json.len(tags))
    print(json.stringify(json.get_index(tags, 0)))
    print(json.keys(root))
    print(json.stringify(root))
