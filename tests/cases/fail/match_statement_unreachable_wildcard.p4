enum TokenKind:
    INT
    IDENT

def main() -> None:
    kind = TokenKind.INT

    match kind:
        case TokenKind.INT:
            print("int")
        case TokenKind.IDENT:
            print("ident")
        case _:
            print("other")
