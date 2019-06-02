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

    def count(self: Shelf) -> int:
        return len(self.bucket.values)

def main() -> None:
    b: Bucket = Bucket("tools", [1, 2])
    s: Shelf = Shelf(b, 7)
    b.values.append(3)
    print(len(b.values))
    print(b.total())
    b.values = [8, 9]
    print(b.total())
    print(s.count())
    print(b)
    print(s)
