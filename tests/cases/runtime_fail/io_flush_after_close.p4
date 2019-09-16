import io


def main() -> None:
    path: str = "/tmp/py4_runtime_flush_after_close.txt"
    file: io.File = io.open(path, "w")
    io.close(file)
    io.flush(file)
