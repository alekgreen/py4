import chars
import strings

enum TokenKind:
    IDENT
    NUMBER
    FLOAT
    STRING
    CHAR
    DEF
    CLASS
    RETURN
    IF
    ELIF
    ELSE
    WHILE
    WITH
    ASSERT
    ENUM
    IMPORT
    FROM
    AS
    NATIVE
    TRUE
    FALSE
    NONE
    AND
    OR
    NOT
    COLON
    COMMA
    DOT
    EQ
    EQEQ
    NEQ
    PLUS
    MINUS
    STAR
    SLASH
    LT
    GT
    LE
    GE
    ARROW
    LPAREN
    RPAREN
    LBRACKET
    RBRACKET
    LBRACE
    RBRACE
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

def lexer_fail(line_no: int, column: int, message: str) -> None:
    formatted = "lexer proto error at " + strings.from_int(line_no)
    formatted = formatted + ":"
    formatted = formatted + strings.from_int(column)
    formatted = formatted + ": "
    formatted = formatted + message
    assert False, formatted

def is_ident_start(ch: char) -> bool:
    return chars.is_alpha(ch) or ch == '_'

def is_ident_continue(ch: char) -> bool:
    return chars.is_alpha(ch) or chars.is_digit(ch) or ch == '_'

def is_blank_or_comment_line(line: str) -> bool:
    i = 0

    while i < len(line) and (line[i] == ' ' or line[i] == '\t' or line[i] == '\r'):
        i = i + 1

    return i == len(line) or line[i] == '#'

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
        "if": TokenKind.IF,
        "elif": TokenKind.ELIF,
        "else": TokenKind.ELSE,
        "while": TokenKind.WHILE,
        "with": TokenKind.WITH,
        "assert": TokenKind.ASSERT,
        "enum": TokenKind.ENUM,
        "import": TokenKind.IMPORT,
        "from": TokenKind.FROM,
        "as": TokenKind.AS,
        "native": TokenKind.NATIVE,
        "True": TokenKind.TRUE,
        "False": TokenKind.FALSE,
        "None": TokenKind.NONE,
        "and": TokenKind.AND,
        "or": TokenKind.OR,
        "not": TokenKind.NOT
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

def lex_identifier(tokens: list[Token], line: str, line_no: int, start: int) -> int:
    i = start + 1

    while i < len(line) and is_ident_continue(line[i]):
        i = i + 1

    text = line[start:i]
    tokens.append(Token(keyword_kind(text), text, line_no, start + 1))
    return i

def lex_number(tokens: list[Token], line: str, line_no: int, start: int) -> int:
    i = start + 1

    while i < len(line) and chars.is_digit(line[i]):
        i = i + 1

    if i < len(line) and line[i] == '.':
        j = i + 1

        while j < len(line) and chars.is_digit(line[j]):
            j = j + 1

        if j > i + 1:
            tokens.append(Token(TokenKind.FLOAT, line[start:j], line_no, start + 1))
            return j

    tokens.append(Token(TokenKind.NUMBER, line[start:i], line_no, start + 1))
    return i

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

    lexer_fail(line_no, start + 1, "unterminated quoted literal")
    return len(line)

def lex_symbol(tokens: list[Token], line: str, line_no: int, index: int) -> int:
    column = index + 1

    if index + 1 < len(line):
        if line[index] == '-' and line[index + 1] == '>':
            tokens.append(Token(TokenKind.ARROW, line[index:index + 2], line_no, column))
            return index + 2
        if line[index] == '=' and line[index + 1] == '=':
            tokens.append(Token(TokenKind.EQEQ, line[index:index + 2], line_no, column))
            return index + 2
        if line[index] == '!' and line[index + 1] == '=':
            tokens.append(Token(TokenKind.NEQ, line[index:index + 2], line_no, column))
            return index + 2
        if line[index] == '<' and line[index + 1] == '=':
            tokens.append(Token(TokenKind.LE, line[index:index + 2], line_no, column))
            return index + 2
        if line[index] == '>' and line[index + 1] == '=':
            tokens.append(Token(TokenKind.GE, line[index:index + 2], line_no, column))
            return index + 2
    ch = line[index]

    if ch == ':':
        tokens.append(Token(TokenKind.COLON, line[index:index + 1], line_no, column))
        return index + 1
    if ch == ',':
        tokens.append(Token(TokenKind.COMMA, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '.':
        tokens.append(Token(TokenKind.DOT, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '=':
        tokens.append(Token(TokenKind.EQ, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '+':
        tokens.append(Token(TokenKind.PLUS, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '-':
        tokens.append(Token(TokenKind.MINUS, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '*':
        tokens.append(Token(TokenKind.STAR, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '/':
        tokens.append(Token(TokenKind.SLASH, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '<':
        tokens.append(Token(TokenKind.LT, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '>':
        tokens.append(Token(TokenKind.GT, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '(':
        tokens.append(Token(TokenKind.LPAREN, line[index:index + 1], line_no, column))
        return index + 1
    if ch == ')':
        tokens.append(Token(TokenKind.RPAREN, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '[':
        tokens.append(Token(TokenKind.LBRACKET, line[index:index + 1], line_no, column))
        return index + 1
    if ch == ']':
        tokens.append(Token(TokenKind.RBRACKET, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '{':
        tokens.append(Token(TokenKind.LBRACE, line[index:index + 1], line_no, column))
        return index + 1
    if ch == '}':
        tokens.append(Token(TokenKind.RBRACE, line[index:index + 1], line_no, column))
        return index + 1

    lexer_fail(line_no, column, "unsupported character")
    return index + 1

def lex_line_tokens(tokens: list[Token], line: str, line_no: int, start_index: int) -> None:
    i = start_index

    while i < len(line):
        ch = line[i]

        if ch == '#':
            i = len(line)
        elif ch == ' ' or ch == '\t' or ch == '\r':
            i = i + 1
        elif is_ident_start(ch):
            i = lex_identifier(tokens, line, line_no, i)
        elif chars.is_digit(ch):
            i = lex_number(tokens, line, line_no, i)
        elif ch == '"':
            i = lex_quoted(tokens, TokenKind.STRING, '"', line, line_no, i)
        elif ch == '\'':
            i = lex_quoted(tokens, TokenKind.CHAR, '\'', line, line_no, i)
        else:
            i = lex_symbol(tokens, line, line_no, i)

    tokens.append(Token(TokenKind.NEWLINE, "", line_no, len(line) + 1))

def tokenize(source: str) -> list[Token]:
    lines = source.split("\n")
    indent_stack: list[int] = [0]
    tokens: list[Token] = []
    line_no = 1

    for line in lines:
        if is_blank_or_comment_line(line):
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
    source = "import json\n"
    source = source + "from tools import helper as alias\n"
    source = source + "enum Mode:\n"
    source = source + "    FAST\n"
    source = source + "    SLOW\n"
    source = source + "native def helper(x, y) -> Item:\n"
    source = source + "    return x\n"
    source = source + "class User:\n"
    source = source + "    def load(name, value) -> Item:\n"
    source = source + "        label = \"hi\" # greet\n"
    source = source + "        mark = 'x'\n"
    source = source + "        if value >= 1.5 and not False:\n"
    source = source + "            return {\"name\": [name, label]}\n"
    source = source + "        elif value <= 0.5 or super_flag:\n"
    source = source + "            return None\n"
    source = source + "        else:\n"
    source = source + "            while value > 0:\n"
    source = source + "                value = value - 1\n"
    source = source + "            with handle as stream:\n"
    source = source + "                assert value != 0\n"
    source = source + "            return alias\n"

    for token in tokenize(source):
        print(token)
