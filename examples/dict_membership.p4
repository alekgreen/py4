def main() -> None:
    colors: dict[str, str] = {"apple": "red", "sky": "blue"}

    print("apple" in colors)
    print("grass" in colors)
    print(not "grass" in colors)
