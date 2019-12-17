class Contact:
    name: str
    email: str | None
    score: float | None


def main() -> None:
    a = Contact("ann", None, 3.5)
    b = Contact("bob", "bob@example.com", None)
    print(a)
    print(b)
