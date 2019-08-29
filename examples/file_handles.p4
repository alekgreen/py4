import io


def main() -> None:
    path: str = "/tmp/py4_file_handles_example.txt"
    f: io.File = io.open(path, "w")
    io.write(f, "hello")
    io.close(f)

    f = io.open(path, "r")
    print(io.read(f))
    io.close(f)
