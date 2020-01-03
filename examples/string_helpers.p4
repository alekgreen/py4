import strings


def main() -> None:
    value = "a::b::c"
    print(value.split("::"))
    print(strings.split("x-y-z", "-"))
    print("compiler".replace("pile", "pact"))
    print(strings.replace("aaaa", "aa", "b"))
