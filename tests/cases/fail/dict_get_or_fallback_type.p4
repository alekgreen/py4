def main() -> None:
    counts: dict[str, int] = {"a": 1}
    print(counts.get_or("x", "bad"))
