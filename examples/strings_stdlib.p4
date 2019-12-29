import strings


def main() -> None:
    print(strings.starts_with("compiler", "com"))
    print(strings.ends_with("compiler", "ler"))
    print(strings.find("compiler", "pil"))
    print(strings.find("compiler", "xyz"))
