class Bucket:
    values: list[int]

    def total(self: Bucket) -> int:
        total: int = 0
        for value in self.values:
            total = total + value
        return total


def make_bucket(seed: int) -> Bucket:
    return Bucket([seed, seed + 1])


def pair_for(seed: int) -> (Bucket, list[int]):
    return (make_bucket(seed), [seed, seed + 10])


def main() -> None:
    for i in range(4):
        pair: (Bucket, list[int]) = pair_for(i)
        pair[1].append(i + 20)
        if i == 1:
            continue
        if i == 3:
            break
        print(pair[0].total())
        print(pair[1])
