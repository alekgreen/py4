class Animal:
    name: str

    def __init__(self: Animal, name: str) -> None:
        self.name = name

    def label(self: Animal) -> str:
        return self.name

class Dog(Animal):
    bark: int

    def __init__(self: Dog, name: str, bark: int) -> None:
        super().__init__(name)
        self.bark = bark

    def label(self: Dog) -> str:
        return super().label()

    def total(self: Dog) -> int:
        return self.bark + len(super().label())

def main() -> None:
    dog = Dog("rex", 4)
    print(dog.label())
    print(dog.total())
    print(dog)
