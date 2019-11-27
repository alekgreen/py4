class Animal:
    name: str
    age: int

    def label(self: Animal) -> str:
        return self.name

class Dog(Animal):
    bark: int

    def total(self: Dog) -> int:
        return self.age + self.bark

class LoudDog(Dog):
    def twice(self: LoudDog) -> int:
        return self.total() + self.total()

def main() -> None:
    dog = Dog("rex", 4, 3)
    loud = LoudDog("max", 2, 5)
    print(dog.name)
    print(dog.label())
    print(dog.total())
    print(loud.label())
    print(loud.total())
    print(loud.twice())
    print(loud)
