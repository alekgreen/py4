def main() -> None:
    value = 1
    label = match value:
        case 1: "one"
        case 2: "two"

    print(label)
