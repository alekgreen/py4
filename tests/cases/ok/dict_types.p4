def main() -> None:
    counts: dict[str, int] = {"todo": 2, "done": 5}
    labels: dict[int, str] = {1: "one", 2: "two"}

    print(counts)
    print(counts["done"])
    print(counts.keys())
    print(counts.values())
    print(counts.items())
    print(2 in labels)
    print(labels[1])
