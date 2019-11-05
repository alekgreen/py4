def main() -> None:
    data: dict[str, (list[int], int)] = {
        "a": ([1, 2], 3),
        "b": ([4], 5)
    }

    print(data)
    print(data["a"])
    print(data.get_or("z", ([9], 0)))
    print(data.values())
    print(data.items())
