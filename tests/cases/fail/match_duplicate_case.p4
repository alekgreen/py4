enum TokenKind:
    INT
    IDENT

def main() -> None:
    kind = TokenKind.INT

    match kind:
        case TokenKind.INT:
            print("int")
        case TokenKind.INT:
            print("again")
