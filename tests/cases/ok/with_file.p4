import io

def main() -> None:
    path: str = "/tmp/py4_with_file_ok.txt"

    with io.open(path, "w") as file:
        io.write(file, "first\n")
        io.write(file, "second\n")
        io.flush(file)

    with io.open(path, "r") as file:
        print(io.read_line(file))
        print(io.read_line(file))
