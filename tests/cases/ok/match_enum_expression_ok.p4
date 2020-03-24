enum TokenKind:
    INT
    IDENT
    END

def main() -> None:
    first = TokenKind.INT
    second = TokenKind.END

    first_label = match first:
        case TokenKind.INT: "int"
        case TokenKind.IDENT: "ident"
        case _: "other"

    second_label = match second:
        case TokenKind.INT: "int"
        case TokenKind.IDENT: "ident"
        case _: "other"

    print(first_label)
    print(second_label)
