enum TokenKind:
    INT
    IDENT
    END

def main() -> None:
    kind = TokenKind.INT
    label = match kind:
        case TokenKind.INT: "int"

    print(label)
