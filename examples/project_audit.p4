import io
import workdemo
from workdemo.board import Board
from workdemo.board import Task


def main() -> None:
    path: str = "/tmp/py4_project_audit.txt"
    board: Board = Board(workdemo.project_name)

    board.add(Task('A', 3, "todo", False))
    board.add(Task('B', 5, "done", True))
    board.add(Task('C', 2, "done", True))

    totals: (int, int) = board.summary()

    print(board.name)
    print(totals)
    print(board.labels.keys())
    print(board.labels.values())
    print(board.tasks)

    io.write_text(path, "audit\n")
    file: io.File = io.open(path, "a")
    io.write(file, "saved\n")
    io.close(file)
    print(io.read_text(path))
