enum TokenKind:
    INT
    IDENT
    END

def next_kind() -> TokenKind:
    return TokenKind.IDENT

def main() -> None:
    label = match next_kind():
        case TokenKind.INT: "int"
        case TokenKind.IDENT: "ident"
        case _: "other"

    print(label)
