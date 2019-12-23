def banner(name: str) -> str:
    return "hi, " + name + "!"


def main() -> None:
    word = "compiler"
    print(banner("py4"))
    print(word[0])
    print(word[3])
    print("a" + "b" + "c")
