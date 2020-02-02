import testing

def main() -> None:
    testing.assert_true(True)
    testing.assert_false(False)
    testing.assert_eq(3, 3)
    testing.assert_eq(1.5, 1.5)
    testing.assert_eq('x', 'x')
    testing.assert_eq("py4", "py4")
    testing.assert_ne(3, 4)
    testing.assert_ne("aa", "ab")

    print("ok")
