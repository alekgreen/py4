enum TokenKind:
    INT
    IDENT
    END

def main() -> None:
    kind = TokenKind.END
    label = match kind:
        case TokenKind.INT: "int"
        case TokenKind.IDENT: "ident"
        case TokenKind.END: "end"

    print(label)
