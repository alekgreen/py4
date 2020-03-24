enum TokenKind:
    INT
    IDENT
    END

def main() -> None:
    kind = TokenKind.IDENT
    label = match kind:
        case TokenKind.INT: "int"
        case TokenKind.IDENT: "ident"
        case _: "other"

    print(label)
