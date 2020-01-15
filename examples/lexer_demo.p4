import chars

enum TokenKind:
    IDENT
    NUMBER
    DEF
    CLASS
    RETURN
    IF
    COLON
    COMMA
    EQ
    PLUS
    LPAREN
    RPAREN
    NEWLINE
    INDENT
    DEDENT
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

def is_ident_start(ch: char) -> bool:
    return chars.is_alpha(ch) or ch == '_'

def is_ident_continue(ch: char) -> bool:
    return chars.is_alpha(ch) or chars.is_digit(ch) or ch == '_'

def is_blank_line(line: str) -> bool:
    i = 0
    while i < len(line):
        if line[i] != ' ' and line[i] != '\t' and line[i] != '\r':
            return False
        i = i + 1
    return True

def count_indent(line: str) -> int:
    i = 0
    while i < len(line) and line[i] == ' ':
        i = i + 1
    return i

def keyword_kind(text: str) -> TokenKind:
    keywords: dict[str, TokenKind] = {
        "def": TokenKind.DEF,
        "class": TokenKind.CLASS,
        "return": TokenKind.RETURN,
        "if": TokenKind.IF
    }
    return keywords.get_or(text, TokenKind.IDENT)

def emit_indent_tokens(tokens: list[Token], indent_stack: list[int], indent: int, line_no: int) -> None:
    current = indent_stack[len(indent_stack) - 1]

    if indent > current:
        indent_stack.append(indent)
        tokens.append(Token(TokenKind.INDENT, "", line_no, 1))
        return

    while indent < indent_stack[len(indent_stack) - 1]:
        indent_stack.pop()
        tokens.append(Token(TokenKind.DEDENT, "", line_no, 1))

def lex_line_tokens(tokens: list[Token], line: str, line_no: int, start_index: int) -> None:
    i = start_index

    while i < len(line):
        ch = line[i]
        column = i + 1

        if ch == ' ' or ch == '\t' or ch == '\r':
            i = i + 1
        elif is_ident_start(ch):
            start = i
            i = i + 1
            while i < len(line) and is_ident_continue(line[i]):
                i = i + 1
            text = line[start:i]
            tokens.append(Token(keyword_kind(text), text, line_no, start + 1))
        elif chars.is_digit(ch):
            start = i
            i = i + 1
            while i < len(line) and chars.is_digit(line[i]):
                i = i + 1
            tokens.append(Token(TokenKind.NUMBER, line[start:i], line_no, start + 1))
        elif ch == ':':
            tokens.append(Token(TokenKind.COLON, line[i:i + 1], line_no, column))
            i = i + 1
        elif ch == ',':
            tokens.append(Token(TokenKind.COMMA, line[i:i + 1], line_no, column))
            i = i + 1
        elif ch == '=':
            tokens.append(Token(TokenKind.EQ, line[i:i + 1], line_no, column))
            i = i + 1
        elif ch == '+':
            tokens.append(Token(TokenKind.PLUS, line[i:i + 1], line_no, column))
            i = i + 1
        elif ch == '(':
            tokens.append(Token(TokenKind.LPAREN, line[i:i + 1], line_no, column))
            i = i + 1
        elif ch == ')':
            tokens.append(Token(TokenKind.RPAREN, line[i:i + 1], line_no, column))
            i = i + 1
        else:
            i = i + 1

    tokens.append(Token(TokenKind.NEWLINE, "", line_no, len(line) + 1))

def tokenize(source: str) -> list[Token]:
    lines = source.split("\n")
    indent_stack: list[int] = [0]
    tokens: list[Token] = []
    line_no = 1

    for line in lines:
        if is_blank_line(line):
            line_no = line_no + 1
        else:
            indent = count_indent(line)
            emit_indent_tokens(tokens, indent_stack, indent, line_no)
            lex_line_tokens(tokens, line, line_no, indent)
            line_no = line_no + 1

    while len(indent_stack) > 1:
        indent_stack.pop()
        tokens.append(Token(TokenKind.DEDENT, "", line_no, 1))

    tokens.append(Token(TokenKind.EOF, "", line_no, 1))
    return tokens

def main() -> None:
    source = "class User:\n" + "    def load(name, count):\n" + "        if count:\n" + "            return name + count\n"

    for token in tokenize(source):
        print(token)
