enum TokenKind:
    INT
    IDENT

def main() -> None:
    kind = TokenKind.INT
    value = match kind:
        case TokenKind.INT: "int"
        case TokenKind.IDENT: 1
        case _: "other"

    print(value)
