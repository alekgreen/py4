class Task:
    code: char
    points: int
    state: str
    done: bool

    def __init__(self: Task, code: char, points: int, state: str, done: bool) -> None:
        self.code = code
        self.points = points
        self.state = state
        self.done = done
