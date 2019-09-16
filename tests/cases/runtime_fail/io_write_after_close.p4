import io


def main() -> None:
    path: str = "/tmp/py4_runtime_write_after_close.txt"
    file: io.File = io.open(path, "w")
    io.close(file)
    io.write(file, "hello")
