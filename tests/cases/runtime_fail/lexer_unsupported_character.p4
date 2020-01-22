import chars
import strings

enum TokenKind:
    IDENT
    EOF

class Token:
    kind: TokenKind
    text: str
    line: int
    column: int

    def __init__(self: Token, kind: TokenKind, text: str, line: int, column: int) -> None:
        self.kind = kind
        self.text = text
        self.line = line
        self.column = column

def lexer_fail(line_no: int, column: int, message: str) -> None:
    formatted = "lexer error at " + strings.from_int(line_no)
    formatted = formatted + ":"
    formatted = formatted + strings.from_int(column)
    formatted = formatted + ": "
    formatted = formatted + message
    assert False, formatted

def is_ident_start(ch: char) -> bool:
    return chars.is_alpha(ch) or ch == '_'

def lex_symbol(tokens: list[Token], line: str, line_no: int, index: int) -> int:
    lexer_fail(line_no, index + 1, "unsupported character")
    return index + 1

def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    i = 0

    while i < len(source):
        if is_ident_start(source[i]):
            i = i + 1
        else:
            i = lex_symbol(tokens, source, 1, i)

    tokens.append(Token(TokenKind.EOF, "", 1, len(source) + 1))
    return tokens

def main() -> None:
    tokenize("@")
