def main() -> None:
    coords: dict[str, (int, int)] = {"a": (1, 2), "b": (3, 4)}

    print(coords)
    print(coords["a"])
    print(coords.get_or("z", (9, 9)))
    print(coords.items())
