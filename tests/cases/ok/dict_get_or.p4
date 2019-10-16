def main() -> None:
    colors: dict[str, str] = {"apple": "red"}
    counts: dict[str, int] = {"a": 1, "b": 2}

    print(colors.get_or("apple", "missing"))
    print(colors.get_or("grass", "missing"))

    for (key, value) in counts.items():
        print(key)
        print(value)
