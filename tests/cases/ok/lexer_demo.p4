import chars

enum TokenKind:
    IDENT
    NUMBER
    DEF
    CLASS
    PLUS
    LPAREN
    RPAREN
    EOF

class Token:
    kind: TokenKind
    text: str

    def __init__(self: Token, kind: TokenKind, text: str) -> None:
        self.kind = kind
        self.text = text

def is_ident_start(ch: char) -> bool:
    return chars.is_alpha(ch) or ch == '_'

def is_ident_continue(ch: char) -> bool:
    return chars.is_alpha(ch) or chars.is_digit(ch) or ch == '_'

def tokenize(source: str) -> list[Token]:
    keywords: dict[str, TokenKind] = {
        "def": TokenKind.DEF,
        "class": TokenKind.CLASS
    }
    tokens: list[Token] = []
    i = 0

    while i < len(source):
        ch = source[i]

        if chars.is_space(ch):
            i = i + 1
        elif is_ident_start(ch):
            start = i
            i = i + 1
            while i < len(source) and is_ident_continue(source[i]):
                i = i + 1
            text = source[start:i]
            kind = keywords.get_or(text, TokenKind.IDENT)
            tokens.append(Token(kind, text))
        elif chars.is_digit(ch):
            start = i
            i = i + 1
            while i < len(source) and chars.is_digit(source[i]):
                i = i + 1
            tokens.append(Token(TokenKind.NUMBER, source[start:i]))
        elif ch == '+':
            tokens.append(Token(TokenKind.PLUS, source[i:i + 1]))
            i = i + 1
        elif ch == '(':
            tokens.append(Token(TokenKind.LPAREN, source[i:i + 1]))
            i = i + 1
        elif ch == ')':
            tokens.append(Token(TokenKind.RPAREN, source[i:i + 1]))
            i = i + 1
        else:
            i = i + 1

    tokens.append(Token(TokenKind.EOF, ""))
    return tokens

def main() -> None:
    source = "def sum(x1 + 42)"
    tokens = tokenize(source)

    for token in tokens:
        print(token)
