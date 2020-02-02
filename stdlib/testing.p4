import strings

def fail(message: str) -> None:
    assert False, message

def assert_true(value: bool) -> None:
    assert value, "expected True"

def assert_false(value: bool) -> None:
    assert not value, "expected False"

def assert_eq(left: int, right: int) -> None:
    assert left == right, "expected equal ints"

def assert_eq(left: float, right: float) -> None:
    assert left == right, "expected equal floats"

def assert_eq(left: bool, right: bool) -> None:
    assert left == right, "expected equal bools"

def assert_eq(left: char, right: char) -> None:
    assert left == right, "expected equal chars"

def assert_eq(left: str, right: str) -> None:
    same = len(left) == len(right) and strings.starts_with(left, right)
    assert same, "expected equal strings"

def assert_ne(left: int, right: int) -> None:
    assert not (left == right), "expected different ints"

def assert_ne(left: float, right: float) -> None:
    assert not (left == right), "expected different floats"

def assert_ne(left: bool, right: bool) -> None:
    assert not (left == right), "expected different bools"

def assert_ne(left: char, right: char) -> None:
    assert not (left == right), "expected different chars"

def assert_ne(left: str, right: str) -> None:
    same = len(left) == len(right) and strings.starts_with(left, right)
    assert not same, "expected different strings"
