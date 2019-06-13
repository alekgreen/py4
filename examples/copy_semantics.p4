class Bucket:
    values: list[int]

    def total(self: Bucket) -> int:
        total: int = 0
        for value in self.values:
            total = total + value
        return total

def main() -> None:
    original: Bucket = Bucket([1, 2])
    alias: Bucket = original
    alias.values.append(3)
    print(original.total())

    alias.values = [9]
    alias.values.append(10)
    print(original.total())
    print(alias.total())
    print(original)
    print(alias)

    pair_a: (Bucket, list[int]) = (original, [4])
    pair_b: (Bucket, list[int]) = pair_a
    pair_b[0].values.append(5)
    pair_b[1].append(6)
    print(pair_a)
    print(pair_b)
