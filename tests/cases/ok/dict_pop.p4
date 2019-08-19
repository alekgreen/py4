def main() -> None:
    colors: dict[str, str] = {"apple": "red", "sky": "blue"}
    print(colors.pop("apple"))
    print(colors.contains("apple"))
    print(len(colors))
    print(colors)
