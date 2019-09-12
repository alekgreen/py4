import pkgfix.models


def make_total() -> int:
    task: pkgfix.models.Task = pkgfix.models.Task(7)
    return task.points
