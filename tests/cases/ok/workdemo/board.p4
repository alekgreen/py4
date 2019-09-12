import workdemo.models


class Board:
    name: str
    tasks: list[workdemo.models.Task]
    labels: dict[str, str]

    def __init__(self: Board, name: str) -> None:
        self.name = name
        self.tasks = []
        self.labels = {"todo": "open", "done": "closed"}

    def add(self: Board, task: workdemo.models.Task) -> None:
        self.tasks.append(task)

    def total_points(self: Board) -> int:
        total: int = 0
        for task in self.tasks:
            total = total + task.points
        return total

    def done_count(self: Board) -> int:
        count: int = 0
        for task in self.tasks:
            if task.done:
                count = count + 1
        return count

    def summary(self: Board) -> (int, int):
        return (self.total_points(), self.done_count())
