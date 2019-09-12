import io
import workdemo
import workdemo.board
import workdemo.models


def main() -> None:
    path: str = "/tmp/py4_project_audit.txt"
    board: workdemo.board.Board = workdemo.board.Board(workdemo.project_name)

    board.add(workdemo.models.Task('A', 3, "todo", False))
    board.add(workdemo.models.Task('B', 5, "done", True))
    board.add(workdemo.models.Task('C', 2, "done", True))

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
