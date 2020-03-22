enum TokenKind:
    INT
    IDENT

def main() -> None:
    kind = TokenKind.INT

    match kind:
        case _:
            print("default")
        case TokenKind.INT:
            print("int")
