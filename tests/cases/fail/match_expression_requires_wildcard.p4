enum TokenKind:
    INT
    IDENT

def main() -> None:
    kind = TokenKind.INT
    label = match kind:
        case TokenKind.INT: "int"
        case TokenKind.IDENT: "ident"

    print(label)
