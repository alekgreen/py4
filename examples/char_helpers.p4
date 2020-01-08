import chars

def main() -> None:
    ch = "hello"[1]

    print(ch)
    print(ord(ch))
    print(chr(65))
    print(chars.is_digit('7'))
    print(chars.is_alpha('Q'))
    print(chars.is_space(' '))
