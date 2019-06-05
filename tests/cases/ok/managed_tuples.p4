class Bucket:
    values: list[int]

    def total(self: Bucket) -> int:
        total: int = 0
        for value in self.values:
            total = total + value
        return total

def main() -> None:
    pair: (list[int], int) = ([1, 2], 3)
    pair[0].append(4)
    print(pair)
    bucket_pair: (Bucket, list[int]) = (Bucket(pair[0]), [9, 8])
    print(bucket_pair)
    bucket_pair[1].append(7)
    print(bucket_pair[0].total())
    print(bucket_pair)
