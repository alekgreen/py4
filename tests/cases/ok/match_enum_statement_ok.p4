enum TokenKind:
    INT
    IDENT
    END

def main() -> None:
    first = TokenKind.INT
    second = TokenKind.END

    match first:
        case TokenKind.INT:
            print("int")
        case TokenKind.IDENT:
            print("ident")
        case _:
            print("other")

    match second:
        case TokenKind.INT:
            print("int")
        case TokenKind.IDENT:
            print("ident")
        case _:
            print("other")
