import chars
import strings

INDENT_WIDTH = 4
MAX_TOKEN_LEN = 64

enum TokenType:
    KEYWORD
    IDENTIFIER
    SYMBOL
    NUMBER
    STRING
    CHAR
    OPERATOR
    ASSIGN
    PLUS
    MINUS
    PUNCT
    COLON
    UNKNOWN
    INDENT
    DEDENT
    NEWLINE
    LPAREN
    RPAREN
    LBRACKET
    RBRACKET
    LBRACE
    RBRACE
    COMMA
    DOT
    RARROW
    NULL
    EOF

class Token:
    kind: TokenType
    value: str
    path: str
    line_text: str
    line: int
    column: int

    def __init__(self: Token, kind: TokenType, value: str, path: str, line_text: str, line: int, column: int) -> None:
        self.kind = kind
        self.value = value
        self.path = path
        self.line_text = line_text
        self.line = line
        self.column = column

class TokenStream:
    data: list[Token]
    pos: int
    source_path: str
    source_lines: list[str]

    def __init__(self: TokenStream, source_path: str) -> None:
        self.data = []
        self.pos = 0
        self.source_path = source_path
        self.source_lines = []

def lexer_fail(path: str, line_no: int, column: int, message: str) -> None:
    formatted = path + ":" + strings.from_int(line_no)
    formatted = formatted + ":"
    formatted = formatted + strings.from_int(column)
    formatted = formatted + ": Lexer error: "
    formatted = formatted + message
    assert False, formatted

def token_type_name(kind: TokenType) -> str:
    names: dict[TokenType, str] = {
        TokenType.KEYWORD: "KEYWORD",
        TokenType.IDENTIFIER: "IDENTIFIER",
        TokenType.SYMBOL: "SYMBOL",
        TokenType.NUMBER: "NUMBER",
        TokenType.STRING: "STRING",
        TokenType.CHAR: "CHAR",
        TokenType.OPERATOR: "OPERATOR",
        TokenType.ASSIGN: "ASSIGN",
        TokenType.PLUS: "PLUS",
        TokenType.MINUS: "MINUS",
        TokenType.PUNCT: "PUNCT",
        TokenType.COLON: "COLON",
        TokenType.UNKNOWN: "UNKNOWN",
        TokenType.INDENT: "INDENT",
        TokenType.DEDENT: "DEDENT",
        TokenType.NEWLINE: "NEWLINE",
        TokenType.LPAREN: "LPAREN",
        TokenType.RPAREN: "RPAREN",
        TokenType.LBRACKET: "LBRACKET",
        TokenType.RBRACKET: "RBRACKET",
        TokenType.LBRACE: "LBRACE",
        TokenType.RBRACE: "RBRACE",
        TokenType.COMMA: "COMMA",
        TokenType.DOT: "DOT",
        TokenType.RARROW: "RARROW",
        TokenType.NULL: "NULL",
        TokenType.EOF: "EOF"
    }

    return names[kind]

def token_string(token: Token) -> str:
    text = "(" + token_type_name(token.kind) + ", "
    text = text + "\""
    text = text + token.value
    text = text + "\""
    if token.line > 0:
        text = text + ", "
        text = text + token.path
        text = text + ":"
        text = text + strings.from_int(token.line)
        text = text + ":"
        text = text + strings.from_int(token.column)
    text = text + ")"
    return text

def add_source_line(ts: TokenStream, line_text: str) -> None:
    ts.source_lines.append(line_text)

def add_token(ts: TokenStream, token: Token) -> None:
    ts.data.append(token)

def append_token(ts: TokenStream, kind: TokenType, value: str, line_no: int, column: int, line_text: str) -> None:
    add_token(ts, Token(kind, value, ts.source_path, line_text, line_no, column))

def is_keyword(text: str) -> bool:
    keywords: list[str] = [
        "def",
        "return",
        "break",
        "continue",
        "assert",
        "import",
        "from",
        "as",
        "native",
        "type",
        "class",
        "enum",
        "if",
        "while",
        "for",
        "with",
        "in",
        "elif",
        "else",
        "and",
        "or",
        "not",
        "int",
        "float",
        "bool",
        "char",
        "str",
        "string",
        "list",
        "dict",
        "None",
        "True",
        "False"
    ]

    for keyword in keywords:
        if len(text) == len(keyword) and strings.starts_with(text, keyword):
            return True

    return False

def is_letter(ch: char) -> bool:
    return chars.is_alpha(ch) or ch == '_'

def is_digit(ch: char) -> bool:
    return chars.is_digit(ch)

def is_blank_line(line: str) -> bool:
    i = 0

    while i < len(line):
        if line[i] == '#':
            return True
        if line[i] != ' ' and line[i] != '\t' and line[i] != '\r':
            return False
        i = i + 1

    return True

def consume_indent(line: str, start: int) -> (int, int):
    indent = 0
    i = start

    while i < len(line) and (line[i] == ' ' or line[i] == '\t'):
        if line[i] == ' ':
            indent = indent + 1
        else:
            indent = indent + INDENT_WIDTH
        i = i + 1

    return (indent, i)

def add_indent_tokens(ts: TokenStream, indent_stack: list[int], indent: int, line_no: int, line_text: str) -> None:
    current = indent_stack[len(indent_stack) - 1]

    if indent > current:
        indent_stack.append(indent)
        append_token(ts, TokenType.INDENT, "<INDENT>", line_no, 1, line_text)
        return

    while indent < current:
        indent_stack.pop()
        current = indent_stack[len(indent_stack) - 1]
        append_token(ts, TokenType.DEDENT, "<DEDENT>", line_no, 1, line_text)

    if indent != current:
        lexer_fail(ts.source_path, line_no, 1, "inconsistent dedent to column " + strings.from_int(indent))

def push_buffered_token(ts: TokenStream, kind: TokenType, line: str, start: int, end: int, line_no: int, column: int, line_text: str) -> None:
    value = line[start:end]

    if len(value) >= MAX_TOKEN_LEN:
        lexer_fail(ts.source_path, line_no, column, "token too long")

    append_token(ts, kind, value, line_no, column, line_text)

def push_quoted_token(ts: TokenStream, kind: TokenType, line: str, start: int, line_no: int, line_text: str, quote: char) -> int:
    i = start
    column = start + 1

    while i < len(line) and len(line[start:i + 1]) < MAX_TOKEN_LEN:
        i = i + 1
        if i >= len(line):
            break
        if line[i] == '\\':
            i = i + 1
            if i >= len(line):
                lexer_fail(ts.source_path, line_no, column, "unterminated escape sequence")
            i = i + 1
            continue
        if line[i] == quote:
            i = i + 1
            append_token(ts, kind, line[start:i], line_no, column, line_text)
            return i

    if len(line[start:len(line)]) >= MAX_TOKEN_LEN:
        lexer_fail(ts.source_path, line_no, column, "string literal too long")

    lexer_fail(ts.source_path, line_no, column, "unterminated quoted literal")
    return len(line)

def scan_identifier_or_keyword(text: str, start: int) -> (TokenType, str, int):
    cursor = start + 1

    while cursor < len(text) and (is_letter(text[cursor]) or is_digit(text[cursor])):
        cursor = cursor + 1

    value = text[start:cursor]
    if len(value) >= MAX_TOKEN_LEN:
        assert False, "token too long"

    if is_keyword(value):
        return (TokenType.KEYWORD, value, cursor)
    return (TokenType.IDENTIFIER, value, cursor)

def scan_number(text: str, start: int) -> (str, int):
    cursor = start + 1
    seen_dot = False

    while cursor < len(text) and (is_digit(text[cursor]) or (not seen_dot and text[cursor] == '.' and cursor + 1 < len(text) and is_digit(text[cursor + 1]))):
        if text[cursor] == '.':
            seen_dot = True
        cursor = cursor + 1

    value = text[start:cursor]
    if len(value) >= MAX_TOKEN_LEN:
        assert False, "token too long"
    return (value, cursor)

def next_token_from_text(text: str, start: int) -> (Token, int):
    cursor = start

    while cursor < len(text) and (text[cursor] == ' ' or text[cursor] == '\t'):
        cursor = cursor + 1

    if cursor >= len(text):
        return (Token(TokenType.EOF, "", "", "", 0, 0), cursor)

    if text[cursor] == '\n':
        return (Token(TokenType.NEWLINE, "\\n", "", "", 0, 0), cursor + 1)

    if is_letter(text[cursor]):
        ident_info = scan_identifier_or_keyword(text, cursor)
        return (Token(ident_info[0], ident_info[1], "", "", 0, 0), ident_info[2])

    if is_digit(text[cursor]):
        number_info = scan_number(text, cursor)
        return (Token(TokenType.NUMBER, number_info[0], "", "", 0, 0), number_info[1])

    if text[cursor] == '-' and cursor + 1 < len(text) and text[cursor + 1] == '>':
        return (Token(TokenType.RARROW, "->", "", "", 0, 0), cursor + 2)

    if text[cursor] == '=' and cursor + 1 < len(text) and text[cursor + 1] == '=':
        return (Token(TokenType.OPERATOR, "==", "", "", 0, 0), cursor + 2)

    value = text[cursor:cursor + 1]
    if text[cursor] == '=':
        return (Token(TokenType.ASSIGN, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '+':
        return (Token(TokenType.PLUS, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '-':
        return (Token(TokenType.MINUS, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '*' or text[cursor] == '/' or text[cursor] == '<' or text[cursor] == '>' or text[cursor] == '|':
        return (Token(TokenType.OPERATOR, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '(':
        return (Token(TokenType.LPAREN, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == ')':
        return (Token(TokenType.RPAREN, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '[':
        return (Token(TokenType.LBRACKET, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == ']':
        return (Token(TokenType.RBRACKET, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '{':
        return (Token(TokenType.LBRACE, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '}':
        return (Token(TokenType.RBRACE, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == ':':
        return (Token(TokenType.COLON, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == ',':
        return (Token(TokenType.COMMA, value, "", "", 0, 0), cursor + 1)
    if text[cursor] == '.':
        return (Token(TokenType.DOT, value, "", "", 0, 0), cursor + 1)
    return (Token(TokenType.SYMBOL, value, "", "", 0, 0), cursor + 1)

def tokenize_line(ts: TokenStream, line: str, line_text: str, line_no: int, start: int) -> None:
    cursor = start

    while cursor < len(line):
        if line[cursor] == '#':
            return
        if line[cursor] == ' ' or line[cursor] == '\t':
            cursor = cursor + 1
            continue

        if is_letter(line[cursor]):
            token_start = cursor
            column = token_start + 1
            cursor = cursor + 1
            while cursor < len(line) and (is_letter(line[cursor]) or is_digit(line[cursor])):
                cursor = cursor + 1
            text = line[token_start:cursor]
            if len(text) >= MAX_TOKEN_LEN:
                lexer_fail(ts.source_path, line_no, column, "token too long")
            if is_keyword(text):
                append_token(ts, TokenType.KEYWORD, text, line_no, column, line_text)
            else:
                append_token(ts, TokenType.IDENTIFIER, text, line_no, column, line_text)
            continue

        if is_digit(line[cursor]):
            seen_dot = False
            token_start = cursor
            column = token_start + 1
            cursor = cursor + 1
            while cursor < len(line) and (is_digit(line[cursor]) or (not seen_dot and line[cursor] == '.' and cursor + 1 < len(line) and is_digit(line[cursor + 1]))):
                if line[cursor] == '.':
                    seen_dot = True
                cursor = cursor + 1
            push_buffered_token(ts, TokenType.NUMBER, line, token_start, cursor, line_no, column, line_text)
            continue

        if line[cursor] == '"':
            cursor = push_quoted_token(ts, TokenType.STRING, line, cursor, line_no, line_text, '"')
            continue

        if line[cursor] == '\'':
            cursor = push_quoted_token(ts, TokenType.CHAR, line, cursor, line_no, line_text, '\'')
            continue

        column = cursor + 1
        if cursor + 1 < len(line):
            if line[cursor] == '-' and line[cursor + 1] == '>':
                append_token(ts, TokenType.RARROW, "->", line_no, column, line_text)
                cursor = cursor + 2
                continue
            if line[cursor] == '=' and line[cursor + 1] == '=':
                append_token(ts, TokenType.OPERATOR, "==", line_no, column, line_text)
                cursor = cursor + 2
                continue
            if line[cursor] == '!' and line[cursor + 1] == '=':
                append_token(ts, TokenType.OPERATOR, "!=", line_no, column, line_text)
                cursor = cursor + 2
                continue
            if line[cursor] == '<' and line[cursor + 1] == '=':
                append_token(ts, TokenType.OPERATOR, "<=", line_no, column, line_text)
                cursor = cursor + 2
                continue
            if line[cursor] == '>' and line[cursor + 1] == '=':
                append_token(ts, TokenType.OPERATOR, ">=", line_no, column, line_text)
                cursor = cursor + 2
                continue

        if line[cursor] == '=':
            append_token(ts, TokenType.ASSIGN, "=", line_no, column, line_text)
        elif line[cursor] == '+':
            append_token(ts, TokenType.PLUS, "+", line_no, column, line_text)
        elif line[cursor] == '-':
            append_token(ts, TokenType.MINUS, "-", line_no, column, line_text)
        elif line[cursor] == '*' or line[cursor] == '/' or line[cursor] == '<' or line[cursor] == '>' or line[cursor] == '|':
            append_token(ts, TokenType.OPERATOR, line[cursor:cursor + 1], line_no, column, line_text)
        elif line[cursor] == '(':
            append_token(ts, TokenType.LPAREN, "(", line_no, column, line_text)
        elif line[cursor] == ')':
            append_token(ts, TokenType.RPAREN, ")", line_no, column, line_text)
        elif line[cursor] == '[':
            append_token(ts, TokenType.LBRACKET, "[", line_no, column, line_text)
        elif line[cursor] == ']':
            append_token(ts, TokenType.RBRACKET, "]", line_no, column, line_text)
        elif line[cursor] == '{':
            append_token(ts, TokenType.LBRACE, "{", line_no, column, line_text)
        elif line[cursor] == '}':
            append_token(ts, TokenType.RBRACE, "}", line_no, column, line_text)
        elif line[cursor] == ':':
            append_token(ts, TokenType.COLON, ":", line_no, column, line_text)
        elif line[cursor] == ',':
            append_token(ts, TokenType.COMMA, ",", line_no, column, line_text)
        elif line[cursor] == '.':
            append_token(ts, TokenType.DOT, ".", line_no, column, line_text)
        else:
            append_token(ts, TokenType.SYMBOL, line[cursor:cursor + 1], line_no, column, line_text)

        cursor = cursor + 1

def lexer(source: str, source_path: str) -> TokenStream:
    ts = TokenStream(source_path)
    indent_stack: list[int] = [0]
    lines = source.split("\n")
    line_no = 0

    for line in lines:
        line_no = line_no + 1
        add_source_line(ts, line)

        if is_blank_line(line):
            append_token(ts, TokenType.NEWLINE, "\\n", line_no, 1, line)
        else:
            indent_info = consume_indent(line, 0)
            indent = indent_info[0]
            cursor = indent_info[1]
            add_indent_tokens(ts, indent_stack, indent, line_no, line)
            tokenize_line(ts, line, line, line_no, cursor)
            append_token(ts, TokenType.NEWLINE, "\\n", line_no, len(line) + 1, line)

    while len(indent_stack) > 1:
        indent_stack.pop()
        append_token(ts, TokenType.DEDENT, "<DEDENT>", line_no, 1, "")

    append_token(ts, TokenType.EOF, "", line_no, 1, "")
    return ts

def main() -> None:
    source = "import json\n"
    source = source + "from tools import helper as alias\n"
    source = source + "enum Mode:\n"
    source = source + "    FAST\n"
    source = source + "    SLOW\n"
    source = source + "native type Handle\n"
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
    source = source + "            for item in data:\n"
    source = source + "                break\n"
    source = source + "            with handle as stream:\n"
    source = source + "                assert value != 0\n"
    source = source + "            pipe = a | b @ c\n"
    source = source + "            return alias\n"

    for token in lexer(source, "lexer_proto_input.p4").data:
        print(token_string(token))
