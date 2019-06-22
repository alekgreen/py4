def main() -> None:
    colors: dict[str, str] = {"apple": "red", "sky": "blue"}
    print(colors)
    print(colors.get("apple"))
    print(colors["sky"])
    print(colors.contains("grass"))
    print(len(colors))

    copy: dict[str, str] = colors.copy()
    copy.set("grass", "green")
    print(copy)

    colors = {}
    print(colors)
