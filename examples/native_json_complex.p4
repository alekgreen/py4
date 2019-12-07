import io
import json


def main() -> None:
    path = "native_json_complex.tmp"
    io.write_text(path, "{\"m\":{\"n\":\"p\"},")
    io.append_text(path, "\"u\":[{\"t\":[\"a\"]},{\"t\":[\"b\"]}],")
    io.append_text(path, "\"s\":{\"a\":1,\"b\":2}}")

    root = json.parse(io.read_text(path))
    meta = json.get(root, "m")
    users = json.get(root, "u")
    scores = json.get(root, "s")
    missing = json.get_or(meta, "x", json.parse("\"fallback\""))
    second_user = json.get_index(users, 1)

    print(json.as_string(json.get(meta, "n")))
    print(json.has(meta, "n"))
    print(json.len(users))
    print(json.stringify(json.get(json.get_index(users, 0), "t")))
    print(json.keys(scores))
    print(len(json.values(scores)))

    for (name, value) in json.items(scores):
        print(name)
        print(json.as_float(value))

    print(json.as_string(missing))
    print(json.stringify(second_user))
