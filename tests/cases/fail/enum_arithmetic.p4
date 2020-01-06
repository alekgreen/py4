enum TokenKind:
    IDENT
    NUMBER

def main() -> None:
    value = TokenKind.IDENT + 1
    print(value)
