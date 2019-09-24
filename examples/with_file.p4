import io

def main() -> None:
    path: str = "/tmp/py4_with_file.txt"
    writer: io.File = io.open(path, "w")

    with writer as file:
        io.write(file, "first\n")
        io.write(file, "second\n")
        io.flush(file)
    io.close(writer)

    with io.open(path, "r") as file:
        print(io.read_line(file))
        print(io.read_line(file))
