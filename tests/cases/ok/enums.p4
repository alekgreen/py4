enum TokenKind:
    IDENT
    NUMBER
    EOF

class Token:
    kind: TokenKind
    text: str

def is_ident(kind: TokenKind) -> bool:
    return kind == TokenKind.IDENT

def main() -> None:
    kind = TokenKind.NUMBER
    kinds: list[TokenKind] = [TokenKind.IDENT, TokenKind.EOF]
    token = Token(TokenKind.IDENT, "x")
    names: dict[TokenKind, str] = {
        TokenKind.IDENT: "id",
        TokenKind.NUMBER: "num"
    }

    print(kind)
    print(kinds)
    print(token)
    print(names)
    print(is_ident(token.kind))
