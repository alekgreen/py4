enum Color:
    RED
    BLUE

enum Shape:
    CIRCLE
    SQUARE

def main() -> None:
    color = Color.RED

    match color:
        case Shape.CIRCLE:
            print("circle")
        case _:
            print("other")
