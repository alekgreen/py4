import chars

enum TokenKind:
    STRING
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

def lexer_fail(message: str) -> None:
    assert False, message

def lex_quoted(tokens: list[Token], kind: TokenKind, quote: char, line: str, line_no: int, start: int) -> int:
    i = start + 1

    while i < len(line):
        ch = line[i]
        if ch == '\\':
            i = i + 2
        elif ch == quote:
            i = i + 1
            tokens.append(Token(kind, line[start:i], line_no, start + 1))
            return i
        else:
            i = i + 1

    lexer_fail("unterminated quoted literal")
    return len(line)

def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    lex_quoted(tokens, TokenKind.STRING, '"', source, 1, 0)
    tokens.append(Token(TokenKind.EOF, "", 1, len(source) + 1))
    return tokens

def main() -> None:
    tokenize("\"oops")
