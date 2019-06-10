class Bucket:
    name: str
    values: list[int]

    def total(self: Bucket) -> int:
        total: int = 0
        for value in self.values:
            total = total + value
        return total

class Shelf:
    bucket: Bucket
    slot: int

    def add_to_bucket(self: Shelf, value: int) -> None:
        self.bucket.values.append(value)

    def bucket_total(self: Shelf) -> int:
        return self.bucket.total()

def main() -> None:
    shelf: Shelf = Shelf(Bucket("tools", [1, 2]), 7)
    shelf.add_to_bucket(3)
    print(shelf.bucket_total())
    print(shelf.bucket.values)

    pair: (Shelf, list[int]) = (shelf, [9])
    pair[0].add_to_bucket(4)
    pair[1].append(8)
    print(pair[0].bucket_total())
    print(pair[0].bucket.values)
    print(pair)
