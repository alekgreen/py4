class Bucket:
    name: str
    values: list[int]


def main() -> None:
    shelves: dict[str, Bucket] = {
        "left": Bucket("left", [1, 2]),
        "right": Bucket("right", [4])
    }

    shelves["left"].values.append(3)
    shelves["right"].values.append(5)

    for (name, bucket) in shelves.items():
        print(name)
        print(bucket.values)
