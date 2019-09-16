import io


def main() -> None:
    path: str = "/tmp/py4_runtime_read_after_close.txt"
    file: io.File = io.open(path, "w")
    io.write(file, "hello")
    io.close(file)
    print(io.read(file))
