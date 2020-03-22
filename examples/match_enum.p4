enum TokenKind:
    INT
    IDENT
    END

def main() -> None:
    kind = TokenKind.IDENT

    match kind:
        case TokenKind.INT:
            print("int")
        case TokenKind.IDENT:
            print("ident")
        case _:
            print("other")
