import io


def main() -> None:
    path: str = "/tmp/py4_file_lines_example.txt"
    file: io.File = io.open(path, "w")
    io.write(file, "alpha\nbeta\n")
    io.flush(file)
    io.close(file)

    file = io.open(path, "r")
    print(io.read_line(file))
    print(io.read_line(file))
    print(len(io.read_line(file)))
    io.close(file)
