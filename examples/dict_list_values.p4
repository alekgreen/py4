def main() -> None:
    groups: dict[str, list[int]] = {
        "a": [1, 2],
        "b": [3]
    }

    print(groups)
    print(groups["a"])
    print(groups.get_or("c", [0, 0]))
    print(groups.values())
    print(groups.items())

    for (name, values) in groups.items():
        print(name)
        print(values)
