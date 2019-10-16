def main() -> None:
    colors: dict[str, str] = {"apple": "red"}
    print(colors.get_or("apple"))
